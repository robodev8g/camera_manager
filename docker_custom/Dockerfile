FROM ubuntu:24.04

COPY my_ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        python3 \
        python3-pip \
        python3-venv \
        python3-dev \
    && rm -rf /var/lib/apt/lists/*

CMD ["bash"]
