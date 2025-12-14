# syntax=docker/dockerfile:1
FROM ubuntu:22.04

# Install build tools needed by JM Makefiles
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    dos2unix \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /vc
COPY . /vc

# Normalize line endings and build using Makefile flow
RUN find . -type f -name "*.sh" -exec dos2unix {} + && \
    chmod +x unixprep.sh && \
    ./unixprep.sh && \
    make

# Default run: demo harness (no external config needed)
CMD ["./bin/lencod"]
