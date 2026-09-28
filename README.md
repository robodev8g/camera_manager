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

## Live view

```bash
./build/camera-manager
```

Press `q` or `Esc` to close the window.

`V4L2CameraAdapter` opens camera index `0` by default. Another index can be
selected in code:

```cpp
camera_manager::V4L2CameraAdapter camera(1);
```
