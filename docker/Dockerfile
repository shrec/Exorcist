FROM ubuntu:24.04

# ── System packages ──────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    gcc \
    g++ \
    git \
    python3 \
    python3-pip \
    libgl1-mesa-dev \
    libglib2.0-dev \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# ── aqtinstall ───────────────────────────────────────────────────────────────
RUN pip3 install aqtinstall --break-system-packages

# ── Qt 6.7.3 (gcc_64) ────────────────────────────────────────────────────────
ARG QT_VERSION=6.7.3
RUN aqt install-qt linux desktop ${QT_VERSION} gcc_64 \
    --outputdir /opt/qt

# ── Qt environment ────────────────────────────────────────────────────────────
ENV Qt6_DIR=/opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6
ENV CMAKE_PREFIX_PATH=/opt/qt/${QT_VERSION}/gcc_64
ENV PATH=/opt/qt/${QT_VERSION}/gcc_64/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/qt/${QT_VERSION}/gcc_64/lib:$LD_LIBRARY_PATH
ENV QT_QPA_PLATFORM=offscreen

WORKDIR /workspace
