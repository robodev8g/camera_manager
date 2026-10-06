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
   | request local view
   v
atomic<bool>
   |
   v
Main Thread
   |
   v
localLiveView()
```

- **CLI thread**: blocking CLI input.
- **Executor thread**: executes commands.
- **Main thread**: owns OpenCV GUI.
- `view` starts from CLI.
- Local view stops with `q` / `ESC`.
- No `GuiActionQueue` is needed.

## Synchronization — C++20

```cpp
std::atomic<bool> view_requested{false};
std::atomic<bool> running{true};
```

Executor:

```cpp
case Command::LocalView:
    view_requested.store(true);
    view_requested.notify_one();
    break;
```

Main thread:

```cpp
while (running.load()) {

    view_requested.wait(false);   // sleeps while value == false

    if (!running.load())
        break;

    view_requested.store(false);

    localLiveView();
}
```

Local view:

```cpp
void localLiveView()
{
    cv::VideoCapture capture(0);

    while (running.load()) {
        cv::Mat frame;

        if (!capture.read(frame))
            break;

        cv::imshow("Local View", frame);

        const int key = cv::waitKey(1);

        if (key == 'q' || key == 27)
            break;
    }

    cv::destroyWindow("Local View");
}
```

## Key Point

C++20 atomic waiting gives us:

```cpp
view_requested.wait(false);
view_requested.notify_one();
```

so the main thread **sleeps without busy waiting**, and no extra GUI mutex or `condition_variable` is required.

If a mutex is needed elsewhere, keep its lifetime explicit with a small scope:

```cpp
{
    std::lock_guard lock(mutex);
    // protected state
} // automatically unlocked here
```
