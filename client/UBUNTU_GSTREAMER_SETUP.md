# Ubuntu Python GI and GStreamer setup

This guide fixes the common error:

```text
ModuleNotFoundError: No module named 'gi'
```

The most frequent cause is that Ubuntu installed `python3-gi` for the system
Python, but the application's virtual environment cannot see Ubuntu's system
Python packages.

## 1. Install the Ubuntu packages

Install the Python GI bindings, GStreamer introspection data, Python GStreamer
overrides, command-line tools, and the plugins used by the video receiver:

```bash
sudo apt update
sudo apt install \
  python3-gi \
  python3-gst-1.0 \
  gir1.2-gstreamer-1.0 \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-libav
```

References:

- [PyGObject installation guide](https://pygobject.gnome.org/getting_started.html)
- [Ubuntu GStreamer introspection package](https://packages.ubuntu.com/jammy/gir1.2-gstreamer-1.0)
- [GStreamer Linux installation guide](https://gstreamer.freedesktop.org/documentation/installing/on-linux.html)

## 2. Test the system Python

Test with Ubuntu's Python before creating or troubleshooting a virtual
environment:

```bash
/usr/bin/python3 -c 'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst; print("GI and GStreamer OK")'
```

Expected output:

```text
GI and GStreamer OK
```

If this succeeds but the same import fails inside a virtual environment, use
the next section.

## 3. Create a compatible virtual environment

An ordinary Python virtual environment hides `/usr/lib/python3/dist-packages`,
where Ubuntu installs `gi`. Create the environment with access to system
packages:

```bash
cd camera_manager
/usr/bin/python3 -m venv --system-site-packages .venv
source .venv/bin/activate
python -m pip install -r client/requirements.txt
```

Verify GI and GStreamer from inside the environment:

```bash
python -c 'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst; print("GI and GStreamer OK")'
```

Run the client:

```bash
python client/camera_client.py --host CAMERA_AGENT_IP
```

Replace `CAMERA_AGENT_IP` with the IP address of the C++ camera agent.

## 4. Diagnose a failed system-Python import

If `/usr/bin/python3` still cannot import `gi`, verify that the packages are
installed:

```bash
apt policy python3-gi python3-gst-1.0 gir1.2-gstreamer-1.0
dpkg -L python3-gi | grep '/gi/__init__.py'
```

Reinstall the binding packages if necessary:

```bash
sudo apt install --reinstall \
  python3-gi \
  python3-gst-1.0 \
  gir1.2-gstreamer-1.0
```

Then repeat the system-Python test:

```bash
/usr/bin/python3 -c 'import gi; gi.require_version("Gst", "1.0"); from gi.repository import Gst; print("GI and GStreamer OK")'
```

## 5. Verify the receiver plugins

The client requires these GStreamer elements:

- `udpsrc`
- `rtpjitterbuffer`
- `rtph264depay`
- `avdec_h264`
- `videoconvert`
- `appsink`

Check all of them with:

```bash
for element in udpsrc rtpjitterbuffer rtph264depay avdec_h264 videoconvert appsink; do
    gst-inspect-1.0 "$element" >/dev/null || echo "Missing: $element"
done
```

No output means that every required element was found.

For information about a specific element, run:

```bash
gst-inspect-1.0 avdec_h264
```

## 6. Check which Python is running

If imports behave differently between terminals or IDEs, compare the Python
executables:

```bash
which python
which python3
python --version
python3 --version
```

Inside the project environment, the expected executable is:

```text
.../camera_manager/.venv/bin/python
```

Configure the IDE to use that interpreter.

## 7. Avoid installing the wrong package

Do not run:

```bash
pip install gi
```

`gi` is not the correct PyPI package. The upstream package is named
`PyGObject`, but building it with pip requires additional native development
dependencies. On Ubuntu, the recommended and simpler setup for this project is:

1. Install `python3-gi` and the GStreamer packages with `apt`.
2. Create the venv with `--system-site-packages`.
3. Install PySide6 from `client/requirements.txt` with pip.

## Quick installation summary

```bash
sudo apt update
sudo apt install \
  python3-gi python3-gst-1.0 gir1.2-gstreamer-1.0 \
  gstreamer1.0-tools gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
  gstreamer1.0-libav

cd camera_manager
/usr/bin/python3 -m venv --system-site-packages .venv
source .venv/bin/activate
python -m pip install -r client/requirements.txt
python client/camera_client.py --host CAMERA_AGENT_IP
```
