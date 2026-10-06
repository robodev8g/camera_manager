# Camera Manager Threading – Concise Design

## Architecture

```text
CLI Thread
   |
   v
CommandQueue
   |
   v
Executor Thread
   |
   | LOCAL_VIEW
   v
condition_variable
   |
   v
Main Thread
   |
   v
localLiveView()
```

- **CLI thread**: blocking `std::getline()`, parses commands.
- **Executor thread**: executes commands.
- **Main thread**: owns OpenCV GUI.
- `view` is started from CLI.
- Local view ends only on `q` / `ESC`.
- No `GuiActionQueue` is needed because there is only one GUI action.

## Synchronization

```cpp
std::mutex gui_mutex;
std::condition_variable gui_cv;

bool start_local_view = false;
bool running = true;
```

Executor:

```cpp
case Command::LocalView:
{
    std::lock_guard<std::mutex> lock(gui_mutex);
    start_local_view = true;
}

gui_cv.notify_one();
break;
```

Main thread:

```cpp
while (running) {
    std::unique_lock<std::mutex> lock(gui_mutex);

    gui_cv.wait(lock, [] {
        return start_local_view || !running;
    });

    if (!running)
        break;

    start_local_view = false;
    lock.unlock();

    localLiveView();
}
```

Local view:

```cpp
void localLiveView()
{
    cv::VideoCapture capture(0);

    while (running) {
        cv::Mat frame;

        if (!capture.read(frame))
            break;

        cv::imshow("Local View", frame);

        int key = cv::waitKey(1);

        if (key == 'q' || key == 27)
            break;
    }

    cv::destroyWindow("Local View");
}
```

## Key Point

The main thread sleeps on:

```cpp
gui_cv.wait(...)
```

so there is **no busy waiting**.

The mutex is released before `localLiveView()` so the GUI does not hold the synchronization lock while running.
