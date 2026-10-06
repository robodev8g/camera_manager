# Camera Manager – Main Thread, CLI Thread, and Executor Thread Design

## Goal

The Camera Manager has three responsibilities that should not block each other:

1. **CLI input**
   - Reads commands from the local terminal.
   - Uses blocking `std::getline()`.
   - Must therefore run in its own thread.

2. **Command execution**
   - Receives parsed commands from the CLI.
   - Executes normal Camera Manager actions.
   - Runs in a dedicated executor thread.

3. **Local live view GUI**
   - Uses OpenCV GUI functions such as:
     - `cv::namedWindow`
     - `cv::imshow`
     - `cv::waitKey`
   - Should run on the **main thread**.

The important requirement is:

> Starting the local live view must not block the CLI.

The local live view is the **only GUI-related command** in the current design.

The user starts it from the CLI and stops it directly from the GUI by pressing:

- `q`
- `ESC`

Because there is only one GUI action, there is no need for a generic `GuiActionQueue`.

---

# Final Architecture

```text
                    Camera Manager Process

        ┌────────────────────────────────┐
        │          MAIN THREAD           │
        │                                │
        │ waits for local-view request   │
        │                                │
        │ when requested:                │
        │   localLiveView()              │
        │   cv::imshow()                 │
        │   cv::waitKey()                │
        │                                │
        │ stops on q / ESC               │
        └───────────────▲────────────────┘
                        │
                condition_variable
                        │
                        │ notify
                        │
        ┌───────────────┴────────────────┐
        │        EXECUTOR THREAD         │
        │                                │
        │ receives commands              │
        │ executes camera actions        │
        │                                │
        │ LOCAL_VIEW command:            │
        │   set request flag             │
        │   notify main thread           │
        └───────────────▲────────────────┘
                        │
                  CommandQueue
                        │
        ┌───────────────┴────────────────┐
        │          CLI THREAD            │
        │                                │
        │ std::getline()                 │
        │ parse command                  │
        │ push to CommandQueue           │
        └────────────────────────────────┘
```

---

# Why the CLI Must Not Be the Main Thread

`std::getline()` is blocking:

```cpp
std::getline(std::cin, input);
```

If the main thread were also the CLI thread, the main thread could be blocked waiting for user input while GUI work needs to run.

Instead:

```text
CLI thread      -> waits for terminal input
Executor thread -> executes commands
Main thread     -> owns local GUI
```

The three responsibilities stay independent.

---

# Why We Do Not Need a GuiActionQueue

A GUI command queue would make sense if the application had several GUI operations, for example:

```text
OPEN_VIEW
CLOSE_VIEW
SHOW_DIALOG
SWITCH_CAMERA
CHANGE_LAYOUT
```

But the current Camera Manager has only one GUI request:

```text
LOCAL_VIEW
```

And stopping the view is handled directly inside the GUI:

```text
q / ESC
```

Therefore a generic GUI queue would add unnecessary abstraction.

Following KISS, the executor only needs to notify the main thread that a local view was requested.

---

# Synchronization

We use:

```cpp
std::mutex
std::condition_variable
bool start_local_view
```

The main thread sleeps efficiently while there is no GUI work.

It does **not** poll continuously.

---

# Shared State

```cpp
std::mutex gui_mutex;
std::condition_variable gui_cv;

bool start_local_view = false;
bool running = true;
```

---

# Executor Side

When the executor receives the local-view command:

```cpp
case Command::LocalView:
{
    std::lock_guard<std::mutex> lock(gui_mutex);
    start_local_view = true;
}

gui_cv.notify_one();
break;
```

This does two things:

1. Sets the pending local-view request.
2. Wakes the main thread.

---

# Main Thread

The main thread waits:

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

The important part is:

```cpp
gui_cv.wait(...)
```

The main thread is sleeping while no GUI action is required.

There is no busy waiting.

---

# Local Live View

The main thread may block inside `localLiveView()`.

That is acceptable because local live view is currently the only GUI responsibility.

```cpp
void localLiveView()
{
    cv::VideoCapture capture(0);

    if (!capture.isOpened()) {
        std::cerr << "Failed to open camera\n";
        return;
    }

    cv::namedWindow("Local View");

    while (running) {

        cv::Mat frame;

        if (!capture.read(frame))
            break;

        cv::imshow("Local View", frame);

        const int key = cv::waitKey(1);

        if (key == 'q' || key == 27) {
            break;
        }
    }

    capture.release();
    cv::destroyWindow("Local View");
}
```

The live view terminates only when:

```text
q
ESC
camera failure
application shutdown
```

---

# Complete Minimal Example

```cpp
#include <opencv2/opencv.hpp>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>


enum class Command {
    LocalView,
    Status,
    Quit
};


template<typename T>
class BlockingQueue
{
public:
    void push(T value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }

        cv_.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [&] {
            return !queue_.empty();
        });

        T value = std::move(queue_.front());
        queue_.pop();

        return value;
    }

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};


BlockingQueue<Command> command_queue;


// GUI synchronization
std::mutex gui_mutex;
std::condition_variable gui_cv;

bool start_local_view = false;
bool running = true;


// --------------------------------------------------
// CLI THREAD
// --------------------------------------------------

void cliThread()
{
    std::string input;

    while (running) {

        std::cout << "> ";

        if (!std::getline(std::cin, input))
            break;

        if (input == "view") {
            command_queue.push(Command::LocalView);
        }
        else if (input == "status") {
            command_queue.push(Command::Status);
        }
        else if (input == "quit") {
            command_queue.push(Command::Quit);
            break;
        }
    }
}


// --------------------------------------------------
// EXECUTOR THREAD
// --------------------------------------------------

void executorThread()
{
    while (running) {

        Command command = command_queue.pop();

        switch (command) {

        case Command::LocalView:
        {
            std::lock_guard<std::mutex> lock(gui_mutex);
            start_local_view = true;

            gui_cv.notify_one();

            break;
        }

        case Command::Status:

            std::cout << "[executor] Camera Manager OK\n";

            break;


        case Command::Quit:
        {
            {
                std::lock_guard<std::mutex> lock(gui_mutex);
                running = false;
            }

            gui_cv.notify_one();

            return;
        }
        }
    }
}


// --------------------------------------------------
// MAIN THREAD GUI FUNCTION
// --------------------------------------------------

void localLiveView()
{
    cv::VideoCapture capture(0);

    if (!capture.isOpened()) {
        std::cerr << "Failed to open camera\n";
        return;
    }

    cv::namedWindow("Local View");

    std::cout
        << "[main] Local view started. "
        << "Press q or ESC to stop.\n";

    while (running) {

        cv::Mat frame;

        if (!capture.read(frame))
            break;

        cv::imshow("Local View", frame);

        const int key = cv::waitKey(1);

        if (key == 'q' || key == 27) {
            break;
        }
    }

    capture.release();
    cv::destroyWindow("Local View");

    std::cout << "[main] Local view stopped\n";
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main()
{
    std::thread cli(cliThread);
    std::thread executor(executorThread);


    while (true) {

        std::unique_lock<std::mutex> lock(gui_mutex);

        gui_cv.wait(lock, [] {
            return start_local_view || !running;
        });


        if (!running)
            break;


        start_local_view = false;


        // Do not hold the mutex while the GUI is running.
        lock.unlock();


        localLiveView();
    }


    if (cli.joinable())
        cli.join();

    if (executor.joinable())
        executor.join();


    cv::destroyAllWindows();

    return 0;
}
```

---

# Command Flow Example

User enters:

```text
> view
```

Flow:

```text
CLI thread
    |
    | Command::LocalView
    v
CommandQueue
    |
    v
Executor thread
    |
    | start_local_view = true
    | gui_cv.notify_one()
    v
Main thread wakes
    |
    v
localLiveView()
    |
    +--> capture.read()
    +--> cv::imshow()
    +--> cv::waitKey()
    +--> capture.read()
    +--> ...
```

Meanwhile the CLI thread is already free again:

```text
> status
```

The executor may continue processing non-GUI commands while the main thread displays the local view.

The view closes when the user presses:

```text
q
```

or:

```text
ESC
```

Then:

```text
localLiveView()
    |
    | returns
    v
main thread
    |
    | gui_cv.wait(...)
    v
sleeping until next local-view request
```

---

# Important Detail: Unlock Before Entering Live View

This is important:

```cpp
start_local_view = false;

lock.unlock();

localLiveView();
```

Do **not** keep `gui_mutex` locked while `localLiveView()` is running.

Otherwise another thread trying to access the GUI synchronization state could remain blocked for the entire duration of the live view.

The mutex protects only the shared synchronization state:

```text
start_local_view
running
```

It does not protect the whole live-view operation.

---

# Why `condition_variable` Instead of Polling

An alternative implementation could use:

```cpp
std::atomic<bool> start_local_view;
```

and:

```cpp
while (running) {

    if (start_local_view.exchange(false)) {
        localLiveView();
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(10));
}
```

This works, but it is polling.

The main thread wakes periodically just to ask:

```text
Was a view requested?
Was a view requested?
Was a view requested?
...
```

With a condition variable:

```cpp
gui_cv.wait(...)
```

the thread sleeps until another thread explicitly wakes it.

For this Camera Manager, the condition-variable design is preferable.

---

# Design Decision Summary

## Threads

### Main thread

Owns:

```text
OpenCV GUI
local live view
```

Does not handle CLI input.

---

### CLI thread

Owns:

```text
std::getline()
command parsing
```

Pushes commands into:

```text
CommandQueue
```

---

### Executor thread

Owns:

```text
command execution
camera-manager operations
```

For `LOCAL_VIEW`, it does not run OpenCV GUI directly.

Instead it signals the main thread.

---

# KISS Decision

Current design:

```text
CommandQueue
condition_variable
one local-view request flag
```

No:

```text
GuiActionQueue
GUI dispatcher
GUI command hierarchy
Observer pattern
event bus
```

Those abstractions are unnecessary for the current requirements.

If the number of main-thread GUI actions grows later, the design can be upgraded to:

```text
GuiActionQueue
```

without changing the CLI or command-executor architecture.

---

# Recommended Final Structure

```text
main.cpp

    create CLI thread
    create executor thread

    main thread:
        wait on gui_cv
        run localLiveView when requested


Cli
    |
    v
CommandQueue
    |
    v
CommandExecutor
    |
    | LOCAL_VIEW
    |
    +------> local-view request flag
                  +
            condition_variable
                  |
                  v
             Main Thread
                  |
                  v
           localLiveView()
```

This keeps the design simple, thread-safe, and consistent with the project's preference for **KISS first, SOLID second**.
