# Fast Motion Estimation for H.264/AVC (ADS)

This project accelerates integer-pel motion estimation by replacing JM’s full search with Adaptive Diamond Search (ADS). It includes:

- A self-contained harness (`JM-Project/lencod`) with optimized ADS/FS for quick A/B testing (synthetic or YUV input, search-point/time stats).
- Baseline JM-style ADS/FS variants for fair comparison.
- The full JM tree (`JM-Project/JM`) for end-to-end encode/decode; use `ldecod` to verify bitstream/RD.

## Quick Start (Docker)

```powershell
cd JM-Project
docker build -t fastme-jm:latest .
docker run -it --name vc -v "${PWD}:/vc" fastme-jm:latest /bin/bash
docker attach vc
# inside container
./bin/lencod                                          # unit test using synthetic data
./bin/lencod -i ./data/akiyo_qcif.yuv -w 176 -h 144   # using yuv file
```

## Components

- `JM-Project/lencod/src/ads_search.c`: optimized plus baseline JM-style ADS/FS.
- `JM-Project/lencod/src/ads_harness.c`: CLI harness for synthetic/YUV testing, timing, and search-point counts.
- `JM-Project/JM`: official JM source for integration; build `lencod` and `ldecod` to validate bitstreams/RD.

## Integrate with JM

1. Copy/port `ads_search.c/.h` into `JM/lencod/src` and `JM/lencod/inc`, adapting to JM types.
2. In `JM/lencod/src/mv-search.c`, include your header and switch integer-pel search to your ADS/FS (keep JM buffers/params).
3. Add `ads_search.c` to JM’s build (Makefile/VS), build `lencod`.
4. Run `lencod` with `encoder.cfg` to produce a bitstream; decode with `ldecod` to verify correctness/RD.

## Notes

- Sub-pel refinement is not modified; ADS covers integer-pel only.
- Use `--ads-only` to skip FS timing, `--verbose` to print per-block logs.
- For full RD verification, always decode the generated bitstream with `ldecod`.

## Resource

- https://media.xiph.org/video/derf/
