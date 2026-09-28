# Camera Manager

A small camera manager built one step at a time. It provides a camera adapter
interface (`ICameraAdapter`) and a V4L2 implementation
(`V4L2CameraAdapter`) backed by OpenCV.

The adapter supports:

- `take_snapshot(path)`
- `start_record(path)`
- `stop_record()`
- `live_view()`

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Configure

The default configuration is `config/camera_manager.conf`:

```ini
camera_device_index=0
media_directory=media
```

Both values are required. Blank lines and lines beginning with `#` are ignored.

## Run

```bash
./run-camera-manager.sh
```

Pass another configuration file when needed:

```bash
./run-camera-manager.sh path/to/camera_manager.conf
```

The CLI menu provides these actions:

- take a snapshot;
- start or stop recording;
- open live view;
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
