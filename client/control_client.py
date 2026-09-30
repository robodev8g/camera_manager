import json
import queue
import socket
import struct
import threading
from typing import Any

from PySide6.QtCore import QThread, Signal


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
