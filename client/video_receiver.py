from PySide6.QtCore import QThread, Signal
from PySide6.QtGui import QImage


class VideoReceiver(QThread):
    frame_ready = Signal(QImage)
    error = Signal(str)

    def __init__(self, port: int, latency: int = 50) -> None:
        super().__init__()
        self._port = port
        self._latency = latency

    def stop(self) -> None:
        self.requestInterruption()

    def run(self) -> None:
        try:
            import gi

            gi.require_version("Gst", "1.0")
            from gi.repository import Gst

            Gst.init(None)
            pipeline = Gst.parse_launch(
                f"udpsrc port={self._port} "
                'caps="application/x-rtp,media=video,encoding-name=H264,'
                'payload=96,clock-rate=90000" ! '
                f"rtpjitterbuffer latency={self._latency} "
                "drop-on-latency=true ! "
                "rtph264depay ! avdec_h264 ! videoconvert ! "
                "video/x-raw,format=RGB ! "
                "appsink name=video_sink max-buffers=1 drop=true sync=false"
            )
            sink = pipeline.get_by_name("video_sink")
            bus = pipeline.get_bus()

            if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
                raise RuntimeError("could not start the GStreamer receiver")

            try:
                while not self.isInterruptionRequested():
                    message = bus.pop_filtered(
                        Gst.MessageType.ERROR | Gst.MessageType.EOS
                    )
                    if message is not None:
                        if message.type == Gst.MessageType.ERROR:
                            gst_error, _ = message.parse_error()
                            raise RuntimeError(gst_error.message)
                        raise RuntimeError("video stream ended")

                    sample = sink.emit("try-pull-sample", 100 * Gst.MSECOND)
                    if sample is not None:
                        self._emit_sample(sample, Gst)
            finally:
                pipeline.set_state(Gst.State.NULL)
        except Exception as exc:
            if not self.isInterruptionRequested():
                self.error.emit(str(exc))

    def _emit_sample(self, sample: object, Gst: object) -> None:
        caps = sample.get_caps().get_structure(0)
        width = caps.get_value("width")
        height = caps.get_value("height")
        buffer = sample.get_buffer()
        mapped_ok, mapped = buffer.map(Gst.MapFlags.READ)
        if not mapped_ok:
            raise RuntimeError("could not read a decoded video frame")

        try:
            bytes_per_line = mapped.size // height
            if bytes_per_line < width * 3:
                raise RuntimeError("decoded video frame has an invalid stride")
            image = QImage(
                mapped.data,
                width,
                height,
                bytes_per_line,
                QImage.Format.Format_RGB888,
            ).copy()
        finally:
            buffer.unmap(mapped)

        self.frame_ready.emit(image)
