# GitLab CI for the C++ camera agent

This project already uses CMake, so the simplest GitLab pipeline is:

1. install the Ubuntu build dependencies
2. configure with CMake
3. build the `camera-manager` target
4. upload the binary as a CI artifact

## Minimal `.gitlab-ci.yml`

Create `.gitlab-ci.yml` in the repository root:

```yaml
stages:
  - build

image: ubuntu:24.04

before_script:
  - apt-get update
  - DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      pkg-config \
      libopencv-dev \
      libjsoncpp-dev \
      libzmq3-dev

build_camera_manager:
  stage: build
  script:
    - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    - cmake --build build --parallel
  artifacts:
    when: on_success
    paths:
      - build/camera-manager
    expire_in: 1 week
```

## Why this works

The root [CMakeLists.txt](../CMakeLists.txt) declares:

- `camera-manager` as the main executable
- `OpenCV` as a required dependency
- `JsonCpp` as a required dependency
- `libzmq` via `pkg-config`

That means a standard Ubuntu runner with those packages is enough to compile it.

## Local test command

Before pushing to GitLab, you can run the same steps locally:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/camera-manager --help
```

## Optional: test stage

If you add CTest coverage later:

```yaml
stages:
  - build
  - test

build_camera_manager:
  stage: build
  script:
    - cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    - cmake --build build --parallel

run_tests:
  stage: test
  script:
    - ctest --test-dir build --output-on-failure
```

## Notes

- Keep the runner Linux-based.
- `build/camera-manager` is the binary to keep as the CI artifact.
- If the project adds more dependencies later, install them in `before_script`.
