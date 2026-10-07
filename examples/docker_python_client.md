# Docker setup for the Python HQ client

This is the practical setup that worked for the desktop client in this project.

## 1) Give your user access to Docker

On Ubuntu/Linux, add your user to the `docker` group:

```bash
sudo usermod -aG docker $USER
```

Then log out and back in, or open a new shell session before running Docker commands.

---

## 2) Build the image

From the project root:

```bash
cd /home/user/projects/camera_manager
sudo docker build -t camera-manager-client .
```

If you want a clean rebuild after code changes:

```bash
sudo docker rmi camera-manager-client
sudo docker build -t camera-manager-client .
```

---

## 3) Local same-machine run

This project’s backend is configured for ZMQ transport in `config/camera_manager.json`, so the client must use the same transport.

Use the local loopback addresses when both the camera agent and the desktop client are running on the same machine:

```bash
sudo docker run --rm -it \
  --network host \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  camera-manager-client \
  --host 127.0.0.1 \
  --client-ip 127.0.0.1 \
  --control-transport zmq \
  --control-endpoint tcp://127.0.0.1:7000
```

This is the important configuration that made the live stream request reach the agent reliably.

---

## 4) Native local run without Docker

The same behavior works directly in the project venv:

```bash
cd /home/user/projects/camera_manager
. .venv/bin/activate
python client/camera_client.py \
  --host 127.0.0.1 \
  --control-port 7000 \
  --stream-port 5000 \
  --client-ip 127.0.0.1 \
  --control-transport zmq \
  --control-endpoint tcp://127.0.0.1:7000
```

---

## 5) Why the client IP matters

The camera agent sends the UDP/RTP stream to the client address that it receives in `start_stream`.

If the value is wrong, the control request may succeed but the live stream never arrives. That is why `--client-ip` must match the actual client machine interface.

For local testing on one machine:

- `--host` = `127.0.0.1`
- `--client-ip` = `127.0.0.1`

For two machines on the same LAN:

- `--host` = server LAN IP, for example `192.168.1.50`
- `--client-ip` = client LAN IP, for example `192.168.1.40`

---

## 6) GI and GStreamer notes

If you hit `ModuleNotFoundError: No module named 'gi'`, recreate the venv with system site packages enabled:

```bash
cd /home/user/projects/camera_manager
rm -rf .venv
python3 -m venv --system-site-packages .venv
. .venv/bin/activate
python -m pip install -r client/requirements.txt
python -c "import gi; gi.require_version('Gst', '1.0'); from gi.repository import Gst; print('GI and GStreamer OK')"
```

This is required on Ubuntu because the GObject bindings are provided by the system Python packages, not PyPI.

---

## 7) Minimal troubleshooting checklist

- Rebuild the Docker image after pulling source changes.
- Ensure the user is in the `docker` group.
- Use `--network host` for same-machine local runs.
- Use `--control-transport zmq` and `--control-endpoint tcp://127.0.0.1:7000` for this project config.
- Use the correct `--client-ip` for the machine receiving the UDP stream.
- Make sure the project venv has system site packages enabled when running natively.
