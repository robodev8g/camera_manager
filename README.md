# Camera Manager

A small camera manager built one step at a time. The first step provides a
camera adapter interface (`ICameraAdapter`) and a V4L2 implementation
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

## Run

```bash
./run-camera-manager.sh
```

The CLI menu provides these actions:

- take a snapshot;
- start or stop recording;
- open live view;
- exit.

Press `q` or `Esc` to close the live-view window and return to the menu.
The CLI remains active while live view is open, so snapshots and recording
commands can be entered at the same time.

Photos and videos are saved automatically under `media/` using timestamps:

- `media/photo_YYYYMMDD_HHMMSS.png`
- `media/video_YYYYMMDD_HHMMSS.mp4`

The output directory is configured by `media_dir_path` in `main.cpp` and
injected into `CameraCli`.

The launcher selects Qt's simple input method before OpenCV loads its Qt
window backend. This avoids a harmless Qt5 Wayland/IBus initialization warning.
When configuring an IDE run target directly, set the environment variable
`QT_IM_MODULE=compose` and run `build/camera-manager`.

`V4L2CameraAdapter` opens camera index `0` by default. Another index can be
selected in code:

```cpp
camera_manager::V4L2CameraAdapter camera(1);
```
