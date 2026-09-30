#!/usr/bin/env python3

import argparse
import sys

from PySide6.QtCore import Qt
from PySide6.QtGui import QCloseEvent, QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication,
    QLabel,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from control_client import ControlClient
from video_receiver import VideoReceiver


class CameraClientWindow(QMainWindow):
    def __init__(
        self,
        host: str,
        control_port: int,
        stream_port: int,
    ) -> None:
        super().__init__()
        self.setWindowTitle("Camera Manager")

        self._stream_port = stream_port
        self._streaming = False
        self._pending_request: int | None = None
        self._pending_action: str | None = None
        self._last_frame: QImage | None = None
        self._video_receiver: VideoReceiver | None = None

        self._video = QLabel("Stream stopped")
        self._video.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._video.setMinimumSize(640, 360)
        self._video.setStyleSheet("background: black; color: white;")

        self._stream_button = QPushButton("Start Stream")
        self._stream_button.setEnabled(False)
        self._stream_button.clicked.connect(self._toggle_stream)

        self._status = QLabel(f"Connecting to {host}:{control_port}...")

        layout = QVBoxLayout()
        layout.addWidget(self._video, 1)
        layout.addWidget(self._stream_button)
        layout.addWidget(self._status)

        container = QWidget()
        container.setLayout(layout)
        self.setCentralWidget(container)
        self.resize(960, 620)

        self._control = ControlClient(host, control_port)
        self._control.connected.connect(self._on_connected)
        self._control.disconnected.connect(self._on_disconnected)
        self._control.response_received.connect(self._on_response)
        self._control.error.connect(self._show_error)
        self._control.start()

    def _on_connected(self) -> None:
        self._status.setText("Connected")
        self._stream_button.setEnabled(True)

    def _on_disconnected(self) -> None:
        self._stop_receiver()
        self._streaming = False
        self._pending_request = None
        self._pending_action = None
        self._stream_button.setText("Start Stream")
        self._stream_button.setEnabled(False)
        self._status.setText("Disconnected")

    def _toggle_stream(self) -> None:
        if self._pending_request is not None:
            return

        if self._streaming:
            self._pending_action = "stop"
            self._pending_request = self._control.send_command("stop_stream")
            self._status.setText("Stopping stream...")
        else:
            self._start_receiver()
            self._pending_action = "start"
            self._pending_request = self._control.send_command(
                "start_stream",
                {"udp_port": self._stream_port},
            )
            self._status.setText("Starting stream...")

        self._stream_button.setEnabled(False)

    def _on_response(self, response: dict) -> None:
        if response.get("id") != self._pending_request:
            return

        action = self._pending_action
        self._pending_request = None
        self._pending_action = None

        if not response.get("success", False):
            if action == "start":
                self._stop_receiver()
            self._status.setText(f"Error: {response.get('message', 'command failed')}")
            self._stream_button.setEnabled(True)
            return

        if action == "start":
            self._streaming = True
            self._stream_button.setText("Stop Stream")
            self._status.setText(response.get("message", "Stream started"))
        elif action == "stop":
            self._streaming = False
            self._stop_receiver()
            self._stream_button.setText("Start Stream")
            self._status.setText(response.get("message", "Stream stopped"))

        self._stream_button.setEnabled(True)

    def _start_receiver(self) -> None:
        self._stop_receiver()
        receiver = VideoReceiver(self._stream_port)
        receiver.frame_ready.connect(self._show_frame)
        receiver.error.connect(self._video_error)
        receiver.finished.connect(receiver.deleteLater)
        self._video_receiver = receiver
        receiver.start()

    def _stop_receiver(self) -> None:
        receiver = self._video_receiver
        self._video_receiver = None
        if receiver is not None:
            receiver.stop()
            receiver.wait()

        self._last_frame = None
        self._video.clear()
        self._video.setText("Stream stopped")

    def _show_frame(self, frame: QImage) -> None:
        self._last_frame = frame
        self._render_frame()

    def _render_frame(self) -> None:
        if self._last_frame is None:
            return
        pixmap = QPixmap.fromImage(self._last_frame).scaled(
            self._video.size(),
            Qt.AspectRatioMode.KeepAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        )
        self._video.setPixmap(pixmap)

    def resizeEvent(self, event: object) -> None:
        super().resizeEvent(event)
        self._render_frame()

    def _video_error(self, message: str) -> None:
        if self._streaming or self._pending_action == "start":
            self._control.send_command("stop_stream")
        self._stop_receiver()
        self._streaming = False
        self._pending_request = None
        self._pending_action = None
        self._stream_button.setText("Start Stream")
        self._stream_button.setEnabled(self._control.isRunning())
        self._status.setText(f"Video error: {message}")

    def _show_error(self, message: str) -> None:
        self._status.setText(message)

    def closeEvent(self, event: QCloseEvent) -> None:
        if self._streaming or self._pending_action == "start":
            self._control.send_command("stop_stream")
        self._stop_receiver()

        self._control.stop_when_idle()
        if not self._control.wait(1000):
            self._control.stop()
            self._control.wait()

        event.accept()


def main() -> int:
    parser = argparse.ArgumentParser(description="Camera Manager client")
    parser.add_argument("--host", default="127.0.0.1", help="camera agent IP")
    parser.add_argument("--control-port", type=int, default=7000)
    parser.add_argument("--stream-port", type=int, default=5000)
    args = parser.parse_args()

    if not 1 <= args.control_port <= 65535:
        parser.error("control port must be 1-65535")
    if not 1 <= args.stream_port <= 65535:
        parser.error("stream port must be 1-65535")

    app = QApplication(sys.argv)
    window = CameraClientWindow(
        args.host,
        args.control_port,
        args.stream_port,
    )
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
