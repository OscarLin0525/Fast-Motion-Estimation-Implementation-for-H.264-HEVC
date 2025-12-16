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

- `lencod/src/ads_search.c`: ADS search and SAD helpers.
- `lencod/inc/ads_search.h`: Interfaces and basic types.
- `lencod/src/ads_harness.c`: Harness that tiles the frame, runs both full search and ADS, and prints timing/quality stats. The harness owns the main function; JM's `ldecod` is unused in the demo.

### 3.3 Run Flow for Testing Algo Logic

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
docker run -it --name vc -v "${PWD}:/vc" fastme-jm:latest /bin/bash
# inside container
./bin/lencod
```

### Windows (Visual Studio)

Create a simple console project or use Make; add `lencod/src/ads_harness.c` and `lencod/src/ads_search.c`, include `lencod/inc`, and build the `lencod` target. The legacy JM `.sln` files are available in `JM/` if you plan to integrate ADS into the full encoder.

## 7. Integrate with Full JM (Optional)

If you want ADS inside the official JM encoder:

1. Copy `lencod/inc/ads_search.h` to `JM/lencod/inc/` and `lencod/src/ads_search.c` to `JM/lencod/src/`.
2. Include `ads_search.h` in `JM/lencod/src/mv-search.c` and replace the integer-pel search call with `xDiamondSearch` (preserving JM buffers and search params).
3. Add `ads_search.c` to the JM Makefile or Visual Studio project.
4. Build JM (`lencod`) with its existing scripts, run with your `encoder.cfg`, and validate with `ldecod`.

TODO
JM:

1. 做圖放在 report 裡面(python)，visualization and table content
2. SIMD 的實作加速目前的 sad_block 是用 C 語言迴圈一個一個 pixel 減，這就像單線道。 我需要你用 SIMD (AVX2/SSE) 指令集改一下。具體來說，我們要針對16x16 的 Block 做特化加速，目標是讓 SAD 計算時間快 
3. 拆出並優化 DS / FS：以 JM 型別重寫或封裝 `ads_search.c/.h`，便於替換整數點搜尋。
   在 `JM/lencod/src/mv-search.c` 引入 `ads_search.h`，提供開關切換原生 FS/ADS 與優化版 DS/FS，維持 JM buffer/參數傳遞。
   將 `ads_search.c` 加入 JM 的 Makefile/VS 專案，編譯 `lencod`。
   完整驗證：用 `encoder.cfg` 指向 YUV（可放在 `data/`），跑 `lencod` 產生 bitstream，再用 `ldecod` 解碼檢查碼流與 RD。
   若需要新增輔助檔或腳本以利測試/切換，直接加入。

Reminder
Git:
有人 push 到 origin 的時候自己先拉一次，避免要進 code 的時候衝突一堆，不拉也沒關係之後也可以解
git pull origin main
**若是自己的檔案還沒修改完成，但又需要 pull 時先把已經 commit 的檔案存起來，等 pull 完成 pop 出來解一下 confilct**
git stash
git stash pop
需要修改或是加入文件時，自己先拉一個 branch
git branch {$name}_{$feature}
commit 的時候，用 VScode 左邊的 source control 寫 commit message 比較好看
<類別> 你大概做了甚麼事情
Reviewer: 誰
EX: <doc(修改檔案), fix(重構或是修改 bug), feature(加入一些 component 或是有新增.c 之類的)> 修改了甚麼檔案的甚麼功能
一律都是 push 到自己的 origin branch，不能直接推 main，應該會發 PR 要給一個人 review 一下，反正就大概跟 reviewer 講一下改了甚麼，不要進沒看過的 code
git push origin {$name}_{$feature} -> MR -> approval -> merge into main
**進 code 前必先確定 compile 不會有問題，make clean && make -j8 執行一下那個編出來那個檔案能不能跑**
