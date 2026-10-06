# Python HQ – Multi-Camera Manager Architecture

## Goal

The Python HQ should support multiple remote `CameraManager` agents.

Each CameraManager:

- Has a unique configured ID/name.
- Uses that same ID as its ZeroMQ `DEALER` `routing_id`.
- Connects to one Python HQ `ROUTER` socket.
- Sends a heartbeat every **1 second**.
- Includes its current state in every heartbeat.
- Can receive asynchronous commands with `request_id`.
- Can stream live video to a UDP port provided by HQ.

The Python HQ should:

- Discover CameraManagers automatically through heartbeat messages.
- Remove a camera after **3 seconds without heartbeat**.
- Display current camera state.
- Send asynchronous commands.
- Start multiple simultaneous live streams.
- Allocate a different UDP port for each active live stream.
- Use PySide6 / Qt6 for the GUI.

---

# 1. High-Level Architecture

Keep the Python HQ divided into four main responsibilities:

```text
Communication  → discover cameras + commands/responses
Registry       → current remote camera states
Streaming      → local receiver pipelines + UDP ports
Qt GUI         → display state + user actions
```

The important rule is:

> Do not mix these responsibilities together.

```text
                         Python HQ
┌───────────────────────────────────────────────────────────────┐
│                                                               │
│                    Qt Main Thread                             │
│                                                               │
│   ┌──────────────┐     ┌───────────────┐                     │
│   │ MainWindow   │────▶│ AppController │                     │
│   └──────┬───────┘     └───────┬───────┘                     │
│          │                     │                              │
│          ▼                     ▼                              │
│   ┌──────────────┐     ┌───────────────┐                     │
│   │ CameraModel  │◀────│ CameraRegistry│                     │
│   │   (Qt)       │     │               │                     │
│   └──────────────┘     └───────▲───────┘                     │
│                                │                              │
│                         heartbeat/state                       │
│                                │                              │
│ ───────────────────────────────┼───────────────────────────── │
│                                │                              │
│               Communication Thread                            │
│                                │                              │
│                        ┌───────┴────────┐                     │
│                        │ ZmqRouterWorker │                    │
│                        │                │                     │
│                        │ ROUTER socket  │                     │
│                        │ request IDs    │                     │
│                        │ responses      │                     │
│                        └───────┬────────┘                     │
│                                │                              │
│              tcp://*:5555      │                              │
└────────────────────────────────┼──────────────────────────────┘
                                 │
          ┌──────────────────────┼─────────────────────┐
          ▼                      ▼                     ▼
     CameraManager          CameraManager         CameraManager
     "nose_camera"          "rear_camera"         "zoom_camera"
       DEALER                 DEALER                DEALER
```

Streaming is separate:

```text
                AppController
                     │
                     ▼
                StreamManager
                     │
       ┌─────────────┼─────────────┐
       ▼             ▼             ▼
 StreamSession   StreamSession   StreamSession
 nose_camera     rear_camera     zoom_camera

 UDP :4000       UDP :4001       UDP :4002
     │               │               │
 GStreamer        GStreamer        GStreamer
 receiver         receiver         receiver
 window           window           window
```

This naturally supports multiple simultaneous independent live-view windows.

---

# 2. Core Camera Data Model

Start small:

```python
@dataclass
class CameraState:
    camera_id: str
    recording: bool
    streaming: bool
    stream_port: int | None
    last_heartbeat: float
```

Later it can grow naturally:

```python
@dataclass
class CameraState:
    camera_id: str

    recording: bool
    streaming: bool
    stream_port: int | None

    zoom: float | None = None
    fps: float | None = None

    last_heartbeat: float = 0.0
```

Do **not** put `receiver_active` here.

Why?

These are remote CameraManager state:

```text
recording
streaming
stream_port
zoom
fps
```

These are HQ-local state:

```text
receiver_active
UDP port allocation
GStreamer receiver process
```

Keep them separate.

---

# 3. CameraRegistry

`CameraRegistry` is the single source of truth for cameras currently known to HQ.

```python
class CameraRegistry:

    _cameras: dict[str, CameraState]

    def update_from_heartbeat(self, camera_id, heartbeat):
        ...

    def get(self, camera_id):
        ...

    def all(self):
        ...

    def remove(self, camera_id):
        ...

    def remove_stale(self, timeout_sec=3.0):
        ...
```

The ZeroMQ `routing_id` is authoritative.

Since:

```text
routing_id == camera_id
```

the camera ID does not need to be duplicated inside every JSON message.

This avoids contradictory messages such as:

```text
ZMQ routing_id = nose_camera
JSON camera_id = rear_camera
```

---

# 4. Heartbeat

Heartbeat frequency:

```text
1 Hz
```

Example:

```json
{
    "type": "heartbeat",
    "state": {
        "recording": true,
        "streaming": false,
        "stream_port": null
    }
}
```

Later:

```json
{
    "type": "heartbeat",
    "state": {
        "recording": true,
        "streaming": true,
        "stream_port": 4001,
        "zoom": 2.4,
        "fps": 30
    }
}
```

The CameraManager is the **source of truth for its own state**.

The heartbeat is therefore a state snapshot, not only an "I am alive" message.

---

# 5. Camera Discovery and Lifetime

Lifecycle:

```text
heartbeat received
      ↓
camera exists in registry
      ↓
heartbeat every 1 second
      ↓
no heartbeat for 3 seconds
      ↓
camera removed
```

The Qt main thread can periodically check for stale cameras:

```text
QTimer every ~500 ms / 1 sec
        ↓
registry.remove_stale()
```

When a camera disappears:

```text
CameraRegistry removes it
        ↓
CameraModel updates GUI
        ↓
StreamManager closes local receiver if needed
```

When it reconnects:

```text
CameraManager DEALER reconnects
        ↓
heartbeat received
        ↓
unknown routing_id
        ↓
new camera added
        ↓
GUI automatically shows it
```

This allows HQ to restart without knowing camera IP addresses.

Each CameraManager only needs:

```text
hq_ip
hq_port
camera_id
```

HQ only needs:

```text
listen on tcp://*:5555
```

---

# 6. ZMQ Communication Worker

The most important threading rule:

> The ROUTER socket belongs entirely to the communication thread.

Do not access the same ZeroMQ socket directly from the Qt main thread.

Conceptual class:

```python
class ZmqRouterWorker(QObject):

    heartbeat_received = Signal(str, dict)
    response_received = Signal(dict)

    def run(self):
        ...

    def send_command(
        self,
        camera_id: str,
        command: dict,
    ):
        ...
```

Outgoing:

```text
Qt thread                         ZMQ thread

Start Recording
      │
      └──── queued signal ───────▶ send_command()
                                        │
                                        ▼
                                    ROUTER.send()
```

Incoming:

```text
ROUTER.recv()
      │
      ▼
parse message
      │
      └──── Qt signal ─────────▶ AppController
                                      │
                                      ▼
                               CameraRegistry
```

A major advantage of this design:

> The registry can live entirely in the Qt thread, so very little mutex usage is required.

---

# 7. Message Protocol

Use one simple JSON envelope format.

## Heartbeat

```json
{
    "type": "heartbeat",
    "state": {
        "recording": true,
        "streaming": false,
        "stream_port": null
    }
}
```

The ROUTER envelope already identifies which camera sent the message.

---

## Command

Example:

```json
{
    "type": "command",
    "request_id": "3ba92...",
    "command": "start_recording",
    "params": {}
}
```

Start stream:

```json
{
    "type": "command",
    "request_id": "4afe1...",
    "command": "start_stream",
    "params": {
        "port": 4003
    }
}
```

---

## Response

Success:

```json
{
    "type": "response",
    "request_id": "4afe1...",
    "success": true
}
```

Failure:

```json
{
    "type": "response",
    "request_id": "4afe1...",
    "success": false,
    "error": "failed to start GStreamer pipeline"
}
```

Do not create a Python class for every command.

Keep it simple:

```text
CommandType
dict params
JSON
```

---

# 8. Responses vs Heartbeats

Use both.

They answer different questions.

```text
Response  = Did my command succeed?
Heartbeat = What is the camera's actual current state?
```

Example:

```text
HQ
 │
 │ start_recording
 ▼
CameraManager
 │
 │ response: success
 ▼
HQ

later...

Heartbeat:
recording = true
```

The GUI should trust the heartbeat for actual persistent state.

---

# 9. Request IDs

The communication is asynchronous, so every command should have a request ID.

Python can generate it:

```python
request_id = uuid.uuid4().hex
```

Maintain minimal pending request information:

```python
@dataclass
class PendingRequest:
    camera_id: str
    command: str
    sent_at: float
```

```python
pending_requests: dict[str, PendingRequest]
```

Later this can support:

```text
timeouts
callbacks
Future objects
retries
```

But do not overbuild that for the MVP.

Flow:

```text
GUI
 │
 │ start_recording("nose_camera")
 ▼
AppController
 │
 ▼
Communication
 │
 │ request_id = abc123
 ▼
CameraManager

CameraManager
 │
 │ response abc123
 ▼
Communication
 │
 ▼
AppController
```

---

# 10. StreamManager

Streaming is a separate responsibility.

```python
class StreamManager:

    sessions: dict[str, StreamSession]

    def start_stream(self, camera_id):
        ...

    def stop_stream(self, camera_id):
        ...

    def stop_all(self):
        ...

    def is_receiver_active(self, camera_id):
        ...
```

Each active stream has a session:

```python
@dataclass
class StreamSession:
    camera_id: str
    port: int
    receiver: GStreamerReceiver
```

The receiver abstraction:

```python
class GStreamerReceiver:

    def start(self, port: int):
        ...

    def stop(self):
        ...

    def is_running(self) -> bool:
        ...
```

---

# 11. Live Stream Windows

For the MVP, do not embed video inside the main GUI.

The main window can stay focused on camera control:

```text
┌──────────────────────────────────────────┐
│ Camera          Record   Stream          │
│                                          │
│ nose_camera       ●        ▶ Live        │
│ rear_camera       ○        ▶ Live        │
│ payload_camera    ●        ▶ Live        │
└──────────────────────────────────────────┘
```

Pressing:

```text
▶ Live nose_camera
```

opens a separate video window:

```text
┌──────────────────────────────┐
│ nose_camera                  │
│                              │
│         live video           │
│                              │
└──────────────────────────────┘
```

Several streams may be open simultaneously:

```text
┌───────────────┐    ┌───────────────┐
│ nose_camera   │    │ rear_camera   │
│               │    │               │
│ video         │    │ video         │
└───────────────┘    └───────────────┘
```

Advantages:

- Independent window resizing.
- Easy multi-monitor usage.
- User opens only streams they care about.
- Main GUI stays simple.
- No complicated video grid management.

---

# 12. GStreamer MVP

Since the Python HQ does not need `gi`, the initial implementation can wrap `gst-launch-1.0`.

Conceptually:

```text
Python
   ↓
subprocess.Popen(...)
   ↓
gst-launch-1.0
   ↓
udpsrc port=4001 ...
   ↓
autovideosink
```

Example abstraction:

```python
class GStreamerReceiver:

    def start(self, port):
        self.process = subprocess.Popen([
            "gst-launch-1.0",
            ...
            f"port={port}",
            ...
            "autovideosink",
        ])

    def is_running(self):
        return self.process is not None and self.process.poll() is None
```

This is sufficient for the MVP.

Later `GStreamerReceiver` can be replaced with a more integrated Qt solution without changing:

```text
CameraRegistry
Communication
AppController
Protocol
StreamManager
```

---

# 13. Port Management

HQ owns stream port allocation.

Do not simply increment a global number forever.

Use a reusable allocator:

```python
class PortAllocator:
    START_PORT = 4000
    END_PORT = 4099
```

Maintain:

```python
used_ports: set[int]
```

Example:

```text
nose_camera       4000
rear_camera       4001
zoom_camera       4002
```

If `rear_camera` stops:

```text
nose_camera       4000
zoom_camera       4002

free:
4001
```

The next stream may reuse `4001`.

Before allocating a port, verify that the OS port is actually available.

---

# 14. Start Stream Sequence

Recommended sequence:

```text
User clicks Live
      │
      ▼
StreamManager.allocate_port()
      │
      ▼
Start local GStreamer receiver
      │
      ▼
receiver active?
      │
      ├── no → error + release port
      │
      ▼
yes
      │
      ▼
send START_STREAM(port)
      │
      ▼
CameraManager starts sender
      │
      ▼
response(success)
      │
      ▼
heartbeat:
streaming = true
stream_port = 4000
```

Starting the receiver first is preferable.

Even though UDP packet loss at startup is not catastrophic, this makes stream startup deterministic.

If CameraManager responds with failure:

```text
success = false
```

then:

```text
stop local receiver
release port
```

---

# 15. Stop Stream Sequence

Recommended sequence:

```text
User closes live window
       │
       ▼
AppController
       │
       ▼
send STOP_STREAM
       │
       ▼
CameraManager stops sender
       │
       ▼
stop receiver
       │
       ▼
release port
```

If `STOP_STREAM` times out, HQ should still stop its local receiver because the user requested to close the live view.

The heartbeat will later reveal whether the remote sender is still active.

---

# 16. Remote Streaming State vs Local Receiver State

These are intentionally separate.

Remote state:

```text
CameraState.streaming
```

means:

> The CameraManager reports that its GStreamer sender pipeline is active.

Local state:

```text
StreamSession.receiver_active
```

means:

> HQ's local receiver process is active.

Possible combinations:

| CameraManager Sender | HQ Receiver | Meaning |
|---|---|---|
| false | false | No stream |
| true | true | Normal streaming |
| true | false | Sender active, HQ receiver unavailable |
| false | true | Receiver waiting / sender failed |

This separation is useful for diagnostics.

Later the GUI could show:

```text
Streaming
Receiver active
No frames
```

without changing the communication protocol.

---

# 17. AppController

`AppController` ties the subsystems together.

```python
class AppController(QObject):

    def __init__(
        self,
        registry,
        communication,
        stream_manager,
    ):
        ...
```

Example user actions:

```python
def start_recording(camera_id):
    communication.send_command(...)

def stop_recording(camera_id):
    communication.send_command(...)

def open_live_view(camera_id):
    stream_manager.start(...)
    communication.send_command(...)

def close_live_view(camera_id):
    communication.send_command(...)
    stream_manager.stop(...)
```

Incoming events:

```python
def on_heartbeat(camera_id, message):
    registry.update_from_heartbeat(camera_id, message)

def on_response(message):
    ...
```

This prevents `MainWindow` from becoming a large god class.

---

# 18. Qt GUI Structure

Keep the GUI simple:

```text
MainWindow
   │
   └── CameraTableView
             │
             ▼
       CameraTableModel
             │
             ▼
       CameraRegistry
```

MVP table:

| Camera | Recording | Streaming | Actions |
|---|---|---|---|
| nose_camera | Yes | No | Record / Live |
| rear_camera | No | Yes | Record / Stop Live |

Later:

| Camera | Record | Stream | Zoom | FPS | Focus | ... |
|---|---|---|---:|---:|---|---|

Adding new camera properties should not require redesigning the communication architecture.

---

# 19. Suggested Project Layout

Keep the project small.

```text
camera_hq/
│
├── main.py
├── controller.py
├── models.py
├── registry.py
├── protocol.py
│
├── communication/
│   └── zmq_router.py
│
├── streaming/
│   ├── stream_manager.py
│   ├── gstreamer_receiver.py
│   └── port_allocator.py
│
└── gui/
    ├── main_window.py
    └── camera_table_model.py
```

Do **not** add unnecessary architecture such as:

```text
repositories
factories
service locators
command classes
custom observer frameworks
```

Qt signals already provide the observer-style behavior needed here.

KISS should remain the leading design principle.

---

# 20. Thread / Process Model

The runtime structure stays small:

```text
THREAD 1 - Qt Main Thread
─────────────────────────
MainWindow
AppController
CameraRegistry
CameraTableModel
QTimer for stale cameras


THREAD 2 - Communication
─────────────────────────
ZMQ ROUTER
receive heartbeat
receive responses
send commands


PROCESS / PIPELINE PER ACTIVE STREAM
────────────────────────────────────
GStreamer Receiver camera A :4000
GStreamer Receiver camera B :4001
GStreamer Receiver camera C :4002
```

There is no need to start with:

```text
one Python StreamThread per camera
```

if `gst-launch-1.0` is already running the GStreamer pipeline in its own process.

---

# 21. HQ Restart Behavior

This architecture handles HQ restart naturally.

```text
HQ crashes
    ↓
CameraManagers stay alive
    ↓
DEALER sockets keep reconnecting
    ↓
HQ starts again
    ↓
ROUTER starts listening
    ↓
CameraManagers reconnect
    ↓
heartbeats arrive
    ↓
CameraRegistry is rebuilt automatically
```

HQ requires no camera IP configuration.

That is one of the strongest properties of using:

```text
HQ             = ROUTER
CameraManagers = DEALER
```

with the CameraManagers initiating the connections.

---

# 22. MVP Scope

The first implementation should support exactly this.

## Discovery

- Heartbeat every 1 second.
- Automatic camera discovery.
- Camera removed after 3 seconds without heartbeat.
- Automatic rediscovery after reconnect.

## State

- `recording`
- `streaming`
- `stream_port`

## Commands

- `start_recording`
- `stop_recording`
- `start_stream(port)`
- `stop_stream`
- `request_id`
- asynchronous responses

## Streaming

- HQ-side dynamic port allocation.
- Start ports around `4000`.
- Reuse released ports.
- One receiver per active camera stream.
- Multiple simultaneous streams.
- Separate GStreamer video windows.

## GUI

- Camera list.
- Recording indicator.
- Streaming indicator.
- Start/stop recording.
- Open/close live view.

Future properties such as:

```text
zoom
fps
focus
exposure
storage
temperature
```

should fit naturally into the existing architecture.

---

# 23. Final Dependency Direction

Keep this dependency direction:

```text
                 GUI
                  │
                  ▼
            AppController
             /          \
            ▼            ▼
   CameraRegistry    StreamManager
            ▲
            │
      ZmqRouterWorker
            │
            ▼
      CameraManagers
```

The most important rule:

> **GUI never talks directly to ZMQ or GStreamer.**

That separation will keep the Python HQ simple as the application grows.
