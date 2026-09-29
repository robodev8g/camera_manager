# Camera Manager

A small camera manager built one step at a time. It provides a camera adapter
interface (`ICameraAdapter`) and a V4L2 implementation
(`V4L2CameraAdapter`) backed by OpenCV.

Commands are represented independently of their input transport. Every
`ICommandSource` pushes commands and response callbacks into the same
thread-safe FIFO `CommandQueue`. A single `CommandExecutor` consumes that queue,
so commands from the CLI and future network sources share one ordering point.
`CommandHandler` performs the camera and media operations without depending on
the command transport.

Local live view is the special main-thread operation. The executor changes the
preview state and notifies the main thread, which owns the OpenCV window loop.
The executor remains available for snapshots and recording commands while the
preview is open. A shutdown command sets an atomic flag as well as notifying the
main thread, allowing an active preview to close before the process exits.

The adapter supports:

- `take_snapshot(path)`
- `start_record(path)`
- `stop_record()`
- `live_view(stop_requested)`

## Build

Requirements include OpenCV 4 and JsonCpp development packages.

```bash
cmake -S . -B build
cmake --build build
```

## Configure

The default configuration is `config/camera_manager.json`:

```json
{
  "camera_device_index": 0,
  "media_directory": "media"
}
```

Both values are required. Unknown fields and values with the wrong JSON type
are rejected.

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
