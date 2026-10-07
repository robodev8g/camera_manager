#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "python" ] || [ "${1:-}" = "python3" ] || [ "${1:-}" = "/opt/venv/bin/python" ]; then
  shift
  exec /opt/venv/bin/python "$@"
fi

if [ "${1:-}" = "bash" ] || [ "${1:-}" = "sh" ]; then
  shift
  exec /bin/bash "$@"
fi

if [ -n "${DISPLAY:-}" ] && [ "${DISPLAY}" != ":99" ]; then
  exec /opt/venv/bin/python /app/client/camera_client.py "$@"
fi

exec xvfb-run -a --server-args='-screen 0 1280x720x24' /opt/venv/bin/python /app/client/camera_client.py "$@"
