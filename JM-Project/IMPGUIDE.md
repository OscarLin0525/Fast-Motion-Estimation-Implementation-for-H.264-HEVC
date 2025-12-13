# Fast Motion Estimation Implementation for H.264/AVC

![Language](https://img.shields.io/badge/language-C%2FC%2B%2B-blue.svg)
![Platform](https://img.shields.io/badge/platform-GP--CPU-green.svg)
![Status](https://img.shields.io/badge/status-Active-orange.svg)

## 1. Project Overview

This repository contains a lightweight JM-style harness that demonstrates Adaptive Diamond Search (ADS) for **integer-pel** motion estimation. It compares ADS against a full-search baseline on synthetic data (a gradient frame shifted by `(3, -2)`) and reports time savings and quality loss. A full copy of the JM reference tree is provided under `JM/` for users who want to integrate ADS into the official encoder, but the default build runs the self-contained harness in `lencod/` (no sub-pel refinement).

## 2. Technical Specifications

- **Base**: Minimal JM-like harness (C99) with reference JM source available in `JM/`.
- **Algorithm**: Adaptive Diamond Search (LDSP + SDSP) with simple block-size adaptation.
- **Scope**: Integer-pel only; sub-pel refinement is intentionally omitted in the demo harness.
- **Platform**: General-purpose CPU, single-threaded.

## 3. Implementation Details

### 3.1 Algorithm: Adaptive Diamond Search (ADS)

- **Large Diamond Search Pattern (LDSP)**: Step size 2, checks center + 8 neighbors. If best is not center, move and repeat; otherwise switch to SDSP.
- **Small Diamond Search Pattern (SDSP)**: Step size 1, checks center + 4 neighbors. Terminates when center is best.
- **Adaptation**: Smaller blocks clamp search range and iteration count to avoid redundant checks.

### 3.2 Integration Points

- `lencod/src/my_fast_me.c`: ADS search and SAD helpers.
- `lencod/inc/my_fast_me.h`: Interfaces and basic types.
- `lencod/src/mv-search.c`: Harness that tiles the frame, runs both full search and ADS, and prints timing/quality stats. The harness owns the main function; JM's `ldecod` is unused in the demo.

### 3.3 Run Flow

1. Build (Make or Docker).
2. Run `./bin/lencod`.
3. Observe printed MVs (expected `(3, -2)` for each block), per-block loss vs. full search, and summary timing/sad averages.

## 4. Directory Structure

```
JM-Project/
├─ bin/            # Built binaries (lencod)
├─ data/           # Put your YUV test assets here
├─ lencod/         # Demo harness + ADS implementation
├─ JM/             # Full JM reference tree (optional integration target)
├─ Dockerfile      # Builds and runs the Makefile-based harness
├─ Makefile        # Default Unix-like build for the harness
└─ README.md
```

## 5. Testing and Evaluation

- **Built-in demo**: Uses synthetic frames; success = MVs recover `(3, -2)`, average loss stays low, and ADS shows time savings vs. full search.
- **Custom evaluation**: After integrating into full JM, encode standard sequences (e.g., Foreman, Mobile) and compare RD curves (BD-PSNR/BD-BR) and motion-estimation time against JM baseline.

## 6. Build and Run

### Docker (recommended)

```powershell
cd JM-Project
docker build -t fastme-jm:latest .
docker run --rm -it -v "${PWD}":/vc fastme-jm:latest /bin/bash
# inside container
./bin/lencod
```

### Windows (Visual Studio)

Create a simple console project or use CMake; add `lencod/src/mv-search.c` and `lencod/src/my_fast_me.c`, include `lencod/inc`, and build the `lencod` target. The legacy JM `.sln` files are available in `JM/` if you plan to integrate ADS into the full encoder.

## 7. Optional: Integrate with Full JM

If you want ADS inside the official JM encoder:

1. Copy `lencod/inc/my_fast_me.h` to `JM/lencod/inc/` and `lencod/src/my_fast_me.c` to `JM/lencod/src/`.
2. Include `my_fast_me.h` in `JM/lencod/src/mv-search.c` and replace the integer-pel search call with `xDiamondSearch` (preserving JM buffers and search params).
3. Add `my_fast_me.c` to the JM Makefile or Visual Studio project.
4. Build JM (`lencod`) with its existing scripts, run with your `encoder.cfg`, and validate with `ldecod`.

TODO
複製 lencod/src/my_fast_me.c、lencod/inc/my_fast_me.h 到 JM/lencod/src/、JM/lencod/inc/。
修改 JM/lencod/src/mv-search.c：#include "my_fast_me.h"，在整數點搜尋位置呼叫 xDiamondSearch 取代原本 FS；保持 JM 原有 buffer/參數。
把 my_fast_me.c 加入 JM 的 Makefile/VS 專案。
用 JM 的 encoder.cfg 指向你的 YUV（可放在 data/），編譯 lencod，再用 ldecod 驗證。
