# Camera Manager

Camera Manager is planned as a distributed camera-control system:

- a C++ agent runs beside each remote camera;
- a Python GUI receives agent heartbeats and sends camera commands;
- a minimal UDP control protocol carries heartbeat and camera requests;
- a separate media transport carries low-latency live video.

The project currently contains its first working agent/client iteration. See:

- [Architecture](docs/architecture.md)
- [One-agent block diagram and components](docs/agent-design.md)
- [Iteration plan](docs/roadmap.md)

The initial target is Linux with one V4L2/UVC camera per agent instance on a
trusted LAN. Multiple agent instances may run on one or more remote computers.
The design keeps camera hardware and streaming behind interfaces so libcamera,
WebRTC, authentication, and other platforms can be added without changing the
client-facing domain model.

## Build

Requirements are a C++20 compiler, CMake 3.24+, OpenCV 4 development files,
Linux V4L2 headers, and an OpenCV build with GStreamer support. The live stream
pipeline also needs the GStreamer OpenH264 and RTP plugins at runtime.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run the agent and GUI

Copy the example configuration and edit at least `client_host` and
`camera_device`. `client_host` is the IPv4 address of the computer running the
Python GUI; photo and video paths are created under `media_directory` on the
agent computer.

```bash
cp config/agent.conf.example config/agent.conf
```

Start the GUI on the client computer:

```bash
python3 client/camera_manager_gui.py --heartbeat-port 7001
```

Then start the C++ agent on the camera computer:

```bash
./build/agent/camera-agent config/agent.conf
```

The agent sends a heartbeat every second. The GUI considers it offline after
three seconds without a heartbeat. Buttons request a remote photo, start/stop a
remote MP4 recording, or start/stop H.264/RTP video to the GUI. The control
protocol uses UDP port 7000, heartbeat uses UDP 7001, and video uses UDP 5000 by
default; allow these ports through the LAN firewall.

This minimal protocol has no authentication or encryption and must be used only
on a trusted LAN.

## Backend smoke test

Probe a real camera and optionally capture one JPEG:

```bash
./build/agent/camera-backend-smoke /dev/video0
./build/agent/camera-backend-smoke /dev/video0 --photo photo.jpg
./build/agent/camera-backend-smoke /dev/video0 --record video.mp4 5
./build/agent/camera-backend-smoke /dev/video0 --stream 192.168.1.20 5000 30
```

The GUI displays the stream inside its window. A standalone receiver is also
available:

```bash
python3 client/view_stream.py --port 5000
```

The standalone viewer uses PyGObject/GStreamer and opens a native video
window. Stop it with `Ctrl+C`. Its client-side requirements are `python3-gi`,
the GStreamer Python introspection package, and the RTP/H.264 decoder plugins.
The equivalent command-line receiver is:

```bash
gst-launch-1.0 -v udpsrc port=5000 \
  caps="application/x-rtp,media=video,encoding-name=H264,payload=96" \
  ! rtpjitterbuffer latency=50 ! rtph264depay ! avdec_h264 \
  ! videoconvert ! autovideosink sync=false
```

This iteration uses `OpenCvCameraBackend`: one capture thread stores photos and
recordings on the agent and sends live frames as H.264/RTP over UDP. A small
`V4L2Controls` class handles normalized zoom and focus reliably.

## CLion

Open the repository root as a CMake project. CLion will detect the top-level
`CMakeLists.txt`. Select the `camera-agent` target with program arguments
`config/agent.conf`, or select `camera-backend-smoke` with arguments such as
`/dev/video0 --photo photo.jpg`. No custom project generator or manually
maintained IDE project file is required.
