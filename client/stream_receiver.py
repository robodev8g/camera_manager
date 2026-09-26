import queue

import gi
from PIL import Image

gi.require_version("Gst", "1.0")
from gi.repository import Gst

Gst.init(None)


class StreamReceiver:
    def __init__(self) -> None:
        self._pipeline: Gst.Pipeline | None = None
        self._bus: Gst.Bus | None = None
        self._frames: queue.Queue[Image.Image] = queue.Queue(maxsize=1)

    @property
    def running(self) -> bool:
        return self._pipeline is not None

    def start(self, port: int, latency: int = 50) -> None:
        self.stop()
        self._pipeline = Gst.parse_launch(
            f'udpsrc port={port} '
            'caps="application/x-rtp,media=video,encoding-name=H264,'
            'payload=96,clock-rate=90000" ! '
            f"rtpjitterbuffer latency={latency} drop-on-latency=true ! "
            "rtph264depay ! avdec_h264 ! videoconvert ! "
            "video/x-raw,format=RGB ! "
            "appsink name=video_sink emit-signals=true max-buffers=1 "
            "drop=true sync=false"
        )
        sink = self._pipeline.get_by_name("video_sink")
        sink.connect("new-sample", self._on_sample)
        self._bus = self._pipeline.get_bus()
        if self._pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            self.stop()
            raise RuntimeError("Could not start the video receiver")

    def stop(self) -> None:
        if self._pipeline is not None:
            self._pipeline.set_state(Gst.State.NULL)
        self._pipeline = None
        self._bus = None
        self._clear_frames()

    def latest_frame(self) -> Image.Image | None:
        frame = None
        while True:
            try:
                frame = self._frames.get_nowait()
            except queue.Empty:
                return frame

    def poll_error(self) -> str | None:
        if self._bus is None:
            return None
        message = self._bus.pop_filtered(
            Gst.MessageType.ERROR | Gst.MessageType.EOS
        )
        if message is None:
            return None
        if message.type == Gst.MessageType.ERROR:
            error, _ = message.parse_error()
            return error.message
        return "Video stream ended"

    def _on_sample(self, sink: Gst.Element) -> Gst.FlowReturn:
        sample = sink.emit("pull-sample")
        if sample is None:
            return Gst.FlowReturn.ERROR

        caps = sample.get_caps().get_structure(0)
        width = caps.get_value("width")
        height = caps.get_value("height")
        buffer = sample.get_buffer()
        success, mapped = buffer.map(Gst.MapFlags.READ)
        if not success:
            return Gst.FlowReturn.ERROR
        try:
            image = Image.frombytes("RGB", (width, height), bytes(mapped.data))
        finally:
            buffer.unmap(mapped)

        if self._frames.full():
            try:
                self._frames.get_nowait()
            except queue.Empty:
                pass
        self._frames.put_nowait(image)
        return Gst.FlowReturn.OK

    def _clear_frames(self) -> None:
        while True:
            try:
                self._frames.get_nowait()
            except queue.Empty:
                return

