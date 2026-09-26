#!/usr/bin/env python3

import argparse
import queue
import socket
import threading
import time
import tkinter as tk
from tkinter import ttk

from PIL import ImageTk

from stream_receiver import StreamReceiver


class CameraManagerGui:
    def __init__(self, root: tk.Tk, heartbeat_port: int) -> None:
        self.root = root
        self.root.title("Camera Manager")
        self.root.protocol("WM_DELETE_WINDOW", self.close)

        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(("0.0.0.0", heartbeat_port))
        self.socket.settimeout(0.5)

        self.running = True
        self.agent_address: tuple[str, int] | None = None
        self.stream_port = 5000
        self.last_heartbeat = 0.0
        self.recording = False
        self.streaming = False
        self.waiting_for_stream = False
        self.messages: queue.Queue[tuple[str, tuple[str, int]]] = queue.Queue()
        self.receiver = StreamReceiver()
        self.photo_image: ImageTk.PhotoImage | None = None

        self._build_ui()
        threading.Thread(target=self._network_loop, daemon=True).start()
        self.root.after(50, self._poll)

    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=10)
        main.grid(sticky="nsew")
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main.columnconfigure(0, weight=1)
        main.rowconfigure(1, weight=1)

        self.status = ttk.Label(main, text="● Agent offline", foreground="red")
        self.status.grid(row=0, column=0, sticky="w", pady=(0, 8))

        self.video = tk.Label(
            main,
            text="Live stream is stopped",
            background="black",
            foreground="white",
            width=100,
            height=32,
        )
        self.video.grid(row=1, column=0, sticky="nsew")

        controls = ttk.Frame(main)
        controls.grid(row=2, column=0, pady=10)
        self.photo_button = ttk.Button(
            controls, text="Take photo", command=lambda: self._send("PHOTO")
        )
        self.record_start_button = ttk.Button(
            controls,
            text="Start recording",
            command=lambda: self._send("RECORD_START"),
        )
        self.record_stop_button = ttk.Button(
            controls,
            text="Stop recording",
            command=lambda: self._send("RECORD_STOP"),
        )
        self.stream_start_button = ttk.Button(
            controls, text="Start stream", command=self._start_stream
        )
        self.stream_stop_button = ttk.Button(
            controls, text="Stop stream", command=self._stop_stream
        )
        for column, button in enumerate(
            (
                self.photo_button,
                self.record_start_button,
                self.record_stop_button,
                self.stream_start_button,
                self.stream_stop_button,
            )
        ):
            button.grid(row=0, column=column, padx=4)

        self.message = ttk.Label(main, text="Waiting for heartbeat...")
        self.message.grid(row=3, column=0, sticky="w")
        self._update_buttons(False)

    def _network_loop(self) -> None:
        while self.running:
            try:
                data, address = self.socket.recvfrom(2048)
                self.messages.put((data.decode("utf-8", errors="replace"), address))
            except socket.timeout:
                continue
            except OSError:
                return

    def _poll(self) -> None:
        while True:
            try:
                message, address = self.messages.get_nowait()
            except queue.Empty:
                break
            self._handle_message(message, address)

        online = time.monotonic() - self.last_heartbeat < 3.0
        self.status.configure(
            text="● Agent online" if online else "● Agent offline",
            foreground="green" if online else "red",
        )
        self._update_buttons(online)
        if not online and self.receiver.running:
            self.receiver.stop()

        frame = self.receiver.latest_frame()
        if frame is not None:
            frame.thumbnail((960, 540))
            self.photo_image = ImageTk.PhotoImage(frame)
            self.video.configure(image=self.photo_image, text="")

        stream_error = self.receiver.poll_error()
        if stream_error:
            self.message.configure(text=f"Stream error: {stream_error}")
            self.receiver.stop()

        if self.running:
            self.root.after(50, self._poll)

    def _handle_message(self, message: str, address: tuple[str, int]) -> None:
        if not message.startswith("CAMERA_MANAGER|"):
            self.message.configure(text=message)
            if message.startswith("OK STREAMING"):
                self.waiting_for_stream = False
            elif message.startswith("ERROR") and self.waiting_for_stream:
                self.waiting_for_stream = False
                self.receiver.stop()
            return

        fields = message.split("|")
        if len(fields) < 2 or fields[1] != "ONLINE":
            self.last_heartbeat = 0.0
            return

        values = dict(
            field.split("=", 1) for field in fields[2:] if "=" in field
        )
        self.last_heartbeat = time.monotonic()
        self.agent_address = (address[0], int(values.get("command_port", "7000")))
        self.stream_port = int(values.get("stream_port", "5000"))
        self.recording = values.get("recording") == "1"
        self.streaming = values.get("streaming") == "1"

    def _send(self, command: str) -> None:
        if self.agent_address is None:
            self.message.configure(text="Agent is offline")
            return
        self.socket.sendto(command.encode(), self.agent_address)
        self.message.configure(text=f"Sent: {command}")

    def _start_stream(self) -> None:
        try:
            self.receiver.start(self.stream_port)
            self.waiting_for_stream = True
            self._send("STREAM_START")
        except Exception as error:
            self.waiting_for_stream = False
            self.message.configure(text=f"Cannot start receiver: {error}")

    def _stop_stream(self) -> None:
        self._send("STREAM_STOP")
        self.receiver.stop()
        self.video.configure(image="", text="Live stream is stopped")
        self.photo_image = None

    def _update_buttons(self, online: bool) -> None:
        self.photo_button.configure(state="normal" if online else "disabled")
        self.record_start_button.configure(
            state="normal" if online and not self.recording else "disabled"
        )
        self.record_stop_button.configure(
            state="normal" if online and self.recording else "disabled"
        )
        self.stream_start_button.configure(
            state="normal" if online and not self.streaming else "disabled"
        )
        self.stream_stop_button.configure(
            state="normal" if online and self.streaming else "disabled"
        )

    def close(self) -> None:
        if self.receiver.running and self.agent_address is not None:
            self._send("STREAM_STOP")
        self.running = False
        self.receiver.stop()
        self.socket.close()
        self.root.destroy()


def main() -> None:
    parser = argparse.ArgumentParser(description="Camera manager client GUI")
    parser.add_argument(
        "--heartbeat-port", type=int, default=7001, help="heartbeat UDP port"
    )
    args = parser.parse_args()
    if not 1 <= args.heartbeat_port <= 65535:
        parser.error("heartbeat port must be 1-65535")

    root = tk.Tk()
    CameraManagerGui(root, args.heartbeat_port)
    root.mainloop()


if __name__ == "__main__":
    main()
