# Graceful Shutdown with a Blocking CLI

## Problem

The CLI thread blocks on:

```cpp
std::getline(std::cin, command);
```

If a remote `SHUTDOWN` command is received, setting `running = false` is not enough because the CLI thread may still be blocked waiting for input.

## Solution

Use `poll()` on `STDIN_FILENO` with a short timeout.

```cpp
#include <poll.h>
#include <unistd.h>
#include <atomic>
#include <iostream>
#include <string>

void cli_thread(std::atomic<bool>& running)
{
    pollfd pfd{};
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    while (running) {
        int result = poll(&pfd, 1, 100);

        // Shutdown wins over pending CLI input
        if (!running)
            break;

        if (result > 0 && (pfd.revents & POLLIN)) {
            std::string command;

            if (!std::getline(std::cin, command))
                break;

            handle_command(command);
        }
    }
}
```

The remote shutdown handler only needs to do:

```cpp
running = false;
```

## Why It Works

- `poll()` waits for CLI input for up to 100 ms.
- If no input arrives, the loop checks `running` again.
- CLI input is not lost if it arrives between two `poll()` calls; it remains buffered until read.
- Checking `running` before `getline()` ensures remote shutdown wins over pending CLI input.
- The CLI thread exits naturally, so `main()` can `join()` it and shut down gracefully.

```text
Remote SHUTDOWN
      |
      v
running = false
      |
      +--> CLI thread exits
      +--> other worker threads exit
      |
      v
main() joins threads
      |
      v
return 0
```
