import json
import queue
import socket
import struct
import threading
from typing import Any
import os
import ipaddress

from PySide6.QtCore import QThread, Signal

try:
    import zmq
except Exception:
    zmq = None


class ControlClient(QThread):
    connected = Signal()
    disconnected = Signal()
    response_received = Signal(dict)
    error = Signal(str)

    def __init__(self, host: str, port: int) -> None:
        super().__init__()
        self._host = host
        self._port = port
        self._commands: queue.Queue[dict[str, Any] | None] = queue.Queue()
        self._next_request_id = 1
        self._stopping = threading.Event()
        self._socket_lock = threading.Lock()
        self._socket: socket.socket | None = None
        self._use_zmq = False
        self._zmq_endpoint: str | None = None

    def send_command(
        self,
        command: str,
        arguments: dict[str, Any] | None = None,
    ) -> int:
        request_id = self._next_request_id
        self._next_request_id += 1

        request: dict[str, Any] = {
            "id": request_id,
            "command": command,
        }
        # Ensure start_stream includes a client_ip so server can send UDP stream
        if command == "start_stream":
            if arguments is None:
                arguments = {}
            if "client_ip" not in arguments:
                # Determine outbound IP used to reach the control host
                try:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    # doesn't send packets; used to determine local IP
                    sock.connect((self._host, self._port))
                    local_ip = sock.getsockname()[0]
                    sock.close()
                except Exception:
                    local_ip = self._host
                arguments["client_ip"] = local_ip
        if arguments:
            request["arguments"] = arguments
        self._commands.put(request)
        return request_id

    def stop_when_idle(self) -> None:
        self._commands.put(None)

    def stop(self) -> None:
        self._stopping.set()
        self._commands.put(None)
        with self._socket_lock:
            if self._socket is not None:
                try:
                    self._socket.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

    def run(self) -> None:
        # If a ZMQ endpoint is provided via env var, prefer ZMQ
        zmq_endpoint = os.environ.get("CAMERA_CONTROL_ZMQ_ENDPOINT")
        if zmq and zmq_endpoint:
            zmq_endpoint = zmq_endpoint.strip()
            # Accept bare host:port and prepend tcp://
            if not (zmq_endpoint.startswith("tcp://") or zmq_endpoint.startswith("ipc://")):
                if ":" in zmq_endpoint:
                    zmq_endpoint = "tcp://" + zmq_endpoint
            # Validate tcp endpoint: tcp://<host>:<port> or bare host:port
            normalized = zmq_endpoint
            if normalized.startswith("tcp://"):
                normalized = normalized[len("tcp://"):]

            # split host:port (port may include trailing garbage if malformed)
            if ":" not in normalized:
                self.error.emit(f"ZMQ endpoint must be host:port, got '{zmq_endpoint}'")
                self.disconnected.emit()
                return

            host, _, port_text = normalized.rpartition(":")
            host = host.strip()
            port_text = port_text.strip()
            try:
                port = int(port_text)
            except Exception:
                self.error.emit(f"ZMQ endpoint has invalid port: '{port_text}'")
                self.disconnected.emit()
                return
            if not (1 <= port <= 65535):
                self.error.emit(f"ZMQ endpoint port out of range: {port}")
                self.disconnected.emit()
                return

            try:
                addr = ipaddress.ip_address(host)
            except Exception:
                self.error.emit(f"ZMQ endpoint host is not a valid IP address: '{host}'")
                self.disconnected.emit()
                return
            if addr.version != 4:
                self.error.emit(f"ZMQ endpoint host must be IPv4, got version {addr.version}")
                self.disconnected.emit()
                return

            # Rebuild normalized endpoint
            zmq_endpoint = f"tcp://{host}:{port}"

            try:
                context = zmq.Context()
                socket_z = context.socket(zmq.REQ)
                socket_z.connect(zmq_endpoint)
            except Exception as exc:
                self.error.emit(
                    f"Cannot connect to camera agent ZMQ endpoint '{zmq_endpoint}': {exc}"
                )
                self.disconnected.emit()
                return

            # We don't expose socket over _socket for ZMQ path
            self.connected.emit()
            try:
                while not self._stopping.is_set():
                    request = self._commands.get()
                    if request is None:
                        break
                    payload = json.dumps(request, separators=(",", ":")).encode("utf-8")
                    socket_z.send(payload)
                    reply = socket_z.recv()
                    response = json.loads(reply.decode("utf-8"))
                    if response.get("id") != request["id"]:
                        raise RuntimeError("response request ID does not match")
                    self.response_received.emit(response)
            except (RuntimeError, ValueError, Exception) as exc:
                if not self._stopping.is_set():
                    self.error.emit(f"Control connection error: {exc}")
            finally:
                socket_z.close()
                context.term()
            self.disconnected.emit()
            return

        # Fall back to TCP
        try:
            connection = socket.create_connection(
                (self._host, self._port), timeout=5.0
            )
            connection.settimeout(None)
        except OSError as exc:
            self.error.emit(f"Cannot connect to camera agent: {exc}")
            self.disconnected.emit()
            return

        with connection:
            with self._socket_lock:
                self._socket = connection
            self.connected.emit()

            try:
                while not self._stopping.is_set():
                    request = self._commands.get()
                    if request is None:
                        break

                    self._send_message(connection, request)
                    response = self._receive_message(connection)
                    if response.get("id") != request["id"]:
                        raise RuntimeError("response request ID does not match")
                    self.response_received.emit(response)
            except (OSError, RuntimeError, ValueError) as exc:
                if not self._stopping.is_set():
                    self.error.emit(f"Control connection error: {exc}")
            finally:
                with self._socket_lock:
                    self._socket = None

        self.disconnected.emit()

    @staticmethod
    def _send_message(connection: socket.socket, message: dict[str, Any]) -> None:
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")
        connection.sendall(struct.pack("!I", len(payload)) + payload)

    @classmethod
    def _receive_message(cls, connection: socket.socket) -> dict[str, Any]:
        message_size = struct.unpack("!I", cls._receive_exact(connection, 4))[0]
        if message_size == 0:
            raise ValueError("received an empty response")

        response = json.loads(
            cls._receive_exact(connection, message_size).decode("utf-8")
        )
        if not isinstance(response, dict):
            raise ValueError("response must be a JSON object")
        return response

    @staticmethod
    def _receive_exact(connection: socket.socket, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = connection.recv(size - len(data))
            if not chunk:
                raise OSError("camera agent closed the connection")
            data.extend(chunk)
        return bytes(data)
