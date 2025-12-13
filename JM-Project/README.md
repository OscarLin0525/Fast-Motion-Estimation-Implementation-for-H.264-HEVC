# Fast Motion Estimation Implementation for H.264/AVC (ADS)

This workspace provides a runnable Adaptive Diamond Search (ADS) demo in a JM-like harness. It replaces the integer-pel full search with ADS, compares against a full-search baseline on synthetic data, and prints motion vectors, cost, and timing summaries. A full JM tree is available under `JM/` for users who want to integrate ADS into the official encoder, but the default build runs the self-contained harness in `lencod/` (no sub-pel refinement).

**Expected output:** each block recovers MV `(3, -2)` for the synthetic test, with ADS logs showing cost/time comparison against full search.

## Quick Start (Docker - Recommended)

### Windows

1. Start Docker Desktop and wait for "Docker is running".
2. In PowerShell:

```powershell
cd Fast-Motion-Estimation-Implementation-for-H.265-HEVC\JM-Project
docker build -t fastme-jm:latest .
docker run --rm -it -v "${PWD}":/app fastme-jm:latest /bin/bash
# inside container
./bin/lencod
```

### Windows (Visual Studio toolchain)

Create a console project or use CMake; add `lencod/src/mv-search.c` and `lencod/src/my_fast_me.c`, include `lencod/inc`, and build an executable named `lencod`. Legacy JM `.sln/.vcxproj` files live in `JM/` if you plan to integrate ADS into the full encoder.

## Structure

- `lencod/src/my_fast_me.c`: ADS (LDSP + SDSP) with adaptive limits.
- `lencod/src/mv-search.c`: Integration and synthetic/YUV harness.
- `lencod/inc/my_fast_me.h`: Interface and types.
- `lencod/inc/defines.h`: Basic types/macros.
- `data/`: Place your YUV test assets here (e.g., akiyo_qcif.yuv).
- `Makefile`: Default build path (used by Dockerfile).
- `JM/`: Full JM source tree for optional integration.

## Notes

- Sub-pel refinement is intentionally omitted; ADS outputs integer-pel MVs only.
- The harness uses a gradient image shifted by `(3, -2)` to validate correctness; with `-i` you can feed your own YUV.
- For full JM integration (with encoder configs and ldecod), copy `my_fast_me.*` into the JM tree and wire `xDiamondSearch` into `JM/lencod/src/mv-search.c`, then build/run with JM's existing scripts.
