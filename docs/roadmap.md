# Iteration plan

Each iteration should end with a runnable demonstration and automated tests.

## Iteration 1: camera backend

Status: implemented; real-camera validation is still required.

- one OpenCV capture thread;
- local JPEG photos and MP4 recordings;
- H.264/RTP/UDP output;
- V4L2 zoom/focus helper;
- smoke-test executable and hardware-independent tests.

Exit criterion: one selected USB camera must successfully capture a photo,
produce a playable recording, apply its supported controls, and send a
decodable stream.

## Iteration 2: minimal agent and GUI

Status: implemented; two-computer validation is still required.

- config-driven camera device, client host, paths, and ports;
- C++ UDP command server and periodic heartbeat;
- generated remote photo/video filenames;
- Tkinter online/offline and operation controls;
- embedded GStreamer video receiver;
- graceful recording/stream cleanup on agent shutdown.

Exit criterion: run the GUI and agent on separate LAN computers, verify offline
detection, and exercise photo, recording, and stream buttons repeatedly.

## Iteration 3: deterministic test backend

- `FakeCameraBackend` implementing `ICameraBackend`;
- agent integration tests without physical hardware;
- UDP heartbeat, command, error, and shutdown tests;
- simulated camera disconnect and recording failure.

This test seam should be added before expanding the network protocol.

## Iteration 4: reliable control and artifact download

Only if required by real usage, replace the UDP control server with a small
versioned gRPC service and add:

- deadlines and structured errors;
- idempotent request IDs;
- remote artifact listing and chunked download;
- TLS authentication.

The camera backend should not change.

## Iteration 5: multi-agent client

- GUI model for several agent heartbeats;
- camera/agent selector;
- bounded parallel commands and per-agent results;
- reconnect and agent-restart handling.

## Later: multi-viewer or internet streaming

If one-client LAN RTP is insufficient, evaluate RTSP for multiple LAN viewers or
WebRTC/SRTP for NAT traversal and encrypted internet streaming. Do not add both
preemptively.

