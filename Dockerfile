# ═══════════════════════════════════════════════════════════
# Multi-stage Dockerfile — C/CMake build + test
# ═══════════════════════════════════════════════════════════
FROM ubuntu:22.04 AS builder
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    gcc-aarch64-linux-gnu \
    gcc-arm-none-eabi \
    qemu-system-arm \
    qemu-system-aarch64 \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja 2>/dev/null || true
RUN cmake --build build --parallel $(nproc) 2>/dev/null || true

FROM builder AS test-runner
RUN pip3 install --no-cache-dir pytest
CMD ["python3", "-m", "pytest", "tests/", "-v", "--tb=short"]

FROM ubuntu:22.04 AS production
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    qemu-system-aarch64 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=builder /src/build/ /app/build/
CMD ["/bin/bash"]
