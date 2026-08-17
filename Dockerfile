# Multi-stage Docker build for UUV-Simulation-Analysis
# Stage 1: Build on Ubuntu with all dependencies
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libsfml-dev \
    libgdal-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY CMakeLists.txt .
COPY src/ src/
COPY tests/ tests/
COPY windows_build/ windows_build/

RUN cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=windows_build/vcpkg.cmake \
    && cmake --build build --config Release \
    && ctest --test-dir build --output-on-failure

# Stage 2: Runtime image
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libsfml-graphics-3 \
    libsfml-window-3 \
    libsfml-system-3 \
    libgdal30 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/Release/uuv_sim ./uuv_sim
COPY scenarios/ scenarios/
COPY scripts/ scripts/

ENTRYPOINT ["/app/uuv_sim"]
CMD ["--help"]
