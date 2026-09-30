# Camera Manager client

The client connects to one camera agent over TCP and receives its H.264 video
stream over RTP/UDP.

Python requirements:

```bash
python3 -m pip install -r client/requirements.txt
```

GStreamer, its H.264 decoder plugins, and the Python GObject bindings must also
be installed through the operating system.

Run the client with:

```bash
python3 client/camera_client.py --host 192.168.1.50
```

The control and stream ports default to `7000` and `5000`. Override them with
`--control-port` and `--stream-port` when needed.
