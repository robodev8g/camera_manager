# Camera Manager

A small camera manager built one step at a time. It provides a camera adapter
interface (`ICameraAdapter`) and a V4L2 implementation
(`V4L2CameraAdapter`) backed by OpenCV.

Commands are represented independently of their input transport. Every
`ICommandSource` pushes commands and response callbacks into the same
thread-safe FIFO `CommandQueue`. A single `CommandExecutor` consumes that queue,
so commands from the CLI and TCP source share one ordering point.
`CommandHandler` performs the camera and media operations without depending on
the command transport.

The agent also listens for one persistent TCP controller. TCP commands enter
the same queue as CLI commands, and responses are routed back to the connection
that submitted them. Disconnecting the controller does not stop an active
recording; the agent waits for it to reconnect.

Local live view is the special main-thread operation. The executor changes the
preview state and notifies the main thread, which owns the OpenCV window loop.
The executor remains available for snapshots and recording commands while the
preview is open. A shutdown command sets an atomic flag as well as notifying the
main thread, allowing an active preview to close before the process exits.

The adapter supports:

- `take_snapshot(path)`
- `start_record(path)`
- `stop_record()`
- `start_stream(destination, port)`
- `stop_stream()`
- `live_view(stop_requested)`

## Build

Requirements include OpenCV 4 with GStreamer support, JsonCpp, and the
GStreamer OpenH264, RTP, and UDP plugins.

```bash
cmake -S . -B build
cmake --build build
```

## Configure

The default configuration is `config/camera_manager.json`:

```json
{
  "camera_device_index": 0,
  "media_directory": "media",
  "control_port": 7000
}
```

All three values are required. Unknown fields and values with the wrong JSON type
are rejected.

## TCP control protocol

Each message contains a four-byte unsigned payload length in network byte order,
followed by a UTF-8 JSON object. A request has this form:

```json
{"id": 1, "command": "take_snapshot", "arguments": {}}
```

The corresponding response is:

```json
{"id": 1, "success": true, "message": "Snapshot saved to ..."}
```

Remote commands are `take_snapshot`, `start_recording`, `stop_recording`,
`start_stream`, `stop_stream`, `list_media`, and `remove_media`.
`start_stream` requires `{"arguments":{"udp_port":5000}}`; the destination IP
is taken from the TCP peer. `remove_media` requires
`{"arguments":{"filename":"..."}}`. Process shutdown and local preview are
intentionally available only through the local CLI.

## Docker

A Docker image is provided for the Python GUI client. It includes the Qt and
GStreamer runtime dependencies needed by `client/camera_client.py`.

Build:

```bash
docker build -t camera-manager-client .
```

Run in a Linux desktop environment with X11 forwarding:

```bash
xhost +si:localuser:root
sudo docker run --rm -it \
  --network host \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  camera-manager-client --host 192.168.1.50
```

Alternatively, you can run the GUI inside a virtual display if a local desktop is
not available:

```bash
docker run --rm -it --network host camera-manager-client --host 192.168.1.50
```

The image starts the client with `xvfb-run`, so the app can launch even without a
real display attached.

## Run

```bash
./run-camera-manager.sh
```

Pass another configuration file when needed:

```bash
./run-camera-manager.sh path/to/camera_manager.json
```

The CLI menu provides these actions:

- take a snapshot;
- start or stop recording;
- open live view;
- list files in the configured media directory;
- remove a media file by filename;
- exit.

Press `q` or `Esc` to close the live-view window and return to the menu. The
CLI remains active while live view is open, so snapshots and recording commands
can be entered at the same time. Selecting Exit also closes an active live-view
window and terminates the program.

Photos and videos are saved under the configured media directory using
timestamps:

- `photo_YYYYMMDD_HHMMSS.png`
- `video_YYYYMMDD_HHMMSS.mp4`

The launcher selects Qt's simple input method before OpenCV loads its Qt
window backend. This avoids a harmless Qt5 Wayland/IBus initialization warning.
When configuring an IDE run target directly, set the environment variable
`QT_IM_MODULE=compose` and run `build/camera-manager`.
