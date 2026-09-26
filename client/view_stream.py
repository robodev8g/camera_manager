#!/usr/bin/env python3

import argparse
import signal

import gi

gi.require_version("Gst", "1.0")
from gi.repository import GLib, Gst


def main() -> int:
    parser = argparse.ArgumentParser(description="Display the camera RTP stream")
    parser.add_argument("--port", type=int, default=5000, help="UDP port")
    parser.add_argument(
        "--latency", type=int, default=50, help="jitter-buffer latency in ms"
    )
    args = parser.parse_args()

    if not 1 <= args.port <= 65535 or args.latency < 0:
        parser.error("port must be 1-65535 and latency must not be negative")

    Gst.init(None)
    pipeline = Gst.parse_launch(
        f'udpsrc port={args.port} '
        'caps="application/x-rtp,media=video,encoding-name=H264,'
        'payload=96,clock-rate=90000" ! '
        f"rtpjitterbuffer latency={args.latency} drop-on-latency=true ! "
        "rtph264depay ! avdec_h264 ! videoconvert ! "
        "autovideosink sync=false"
    )

    loop = GLib.MainLoop()

    def stop(*_: object) -> None:
        loop.quit()

    def on_message(_: Gst.Bus, message: Gst.Message) -> None:
        if message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            print(f"Stream error: {error.message}")
            if debug:
                print(debug)
            stop()
        elif message.type == Gst.MessageType.EOS:
            stop()

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", on_message)
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
        print("Could not start the GStreamer receiver")
        return 1

    print(f"Listening on UDP port {args.port}; press Ctrl+C to stop")
    try:
        loop.run()
    finally:
        pipeline.set_state(Gst.State.NULL)
        bus.remove_signal_watch()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

