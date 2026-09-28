# Camera Manager

A small camera manager built one step at a time. It provides a camera adapter
interface (`ICameraAdapter`) and a V4L2 implementation
(`V4L2CameraAdapter`) backed by OpenCV.

Commands are represented independently of their input transport. The
`CommandHandler` executes camera and media operations, while an
`ICommandSource` supplies commands and receives results. The current source is
`CliCommandSource`; a UDP source can be added later without duplicating command
handling.

The adapter supports:

- `take_snapshot(path)`
- `start_record(path)`
- `stop_record()`
- `live_view()`

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
can be entered at the same time.

Photos and videos are saved under the configured media directory using
timestamps:

- `photo_YYYYMMDD_HHMMSS.png`
- `video_YYYYMMDD_HHMMSS.mp4`

The launcher selects Qt's simple input method before OpenCV loads its Qt
window backend. This avoids a harmless Qt5 Wayland/IBus initialization warning.
When configuring an IDE run target directly, set the environment variable
`QT_IM_MODULE=compose` and run `build/camera-manager`.
