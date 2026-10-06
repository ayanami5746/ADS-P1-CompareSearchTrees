# Validation record

Run: **2026-10-06 13:12:19 to 2026-10-06 13:17:32 (Asia/Shanghai)**.

## Source and environment

The programs were rebuilt from [ff86faa](https://github.com/ayanami5746/ADS-P1-CompareSearchTrees/commit/ff86faafa94fb8ab11bf964098c2ff4bdd77c8ba). This revision uses bottom-up splaying, ordinary red-black repair, AVL-only height updates, and an order-four B+ tree with the lecture's leaf capacity. Earlier measurements used different algorithm variants and should not be treated as measurements of this revision.

`run_metadata.json` records the source commit, SHA-256 hashes of the build inputs, run times, machine details, parameters, and raw-data hash. The source hashes refer to the local file bytes used for this build; Git line-ending conversion can change a file hash in another checkout.

| Item | Value |
|---|---|
| CPU | AMD Ryzen 9 8945HX with Radeon Graphics |
| OS | Microsoft Windows NT 10.0.26200.0 |
| Compiler | gcc.exe (x86_64-win32-seh-rev2, Built by MinGW-Builds project) 14.2.0 |
| Flags | `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2 -Iinclude` |
| Timer | Windows QueryPerformanceCounter; seconds |

## Correctness

The strict C11 build completed without warnings. All five trees passed the full `--stress` suite; see `correctness.log`. Each tree ran boundary and reuse cases, 14,400 insertion/deletion permutation pairs, 120,000 mixed operations checked against an independent boolean array, all three assignment workloads at N=100,000, and destruction of a populated 20,000-key tree.

The separate lecture checks also passed; see `lecture_checks.log`. They checked both directions of all Splay rotation shapes, access and deletion behavior, the red-black insertion example from slide 4, all mirrored red-black repair cases, the two/three rotation limits for insertion/deletion, and B+ capacity, splitting, borrowing, merging, and root collapse. Test markers confirmed that Splay and red-black rotations did not update AVL heights.

The benchmark itself checked operation counts and tree invariants after each insertion and deletion phase, outside the timed regions. The repository's CI also runs both test programs under AddressSanitizer and UndefinedBehaviorSanitizer with leak detection; CI status is recorded by GitHub for each commit.

## Performance run

```powershell
./run_experiments.ps1 -Stress -Repeats 3 -MaxN 100000 -Seed 20261003
```

The run produced **225 measurements**: five trees, three cases, five sizes, and three repetitions. Sizes were 1,000, 3,000, 10,000, 30,000, and 100,000. Each trial inserted all N keys, then deleted all N keys. Cases were ascending/ascending, ascending/descending, and random/random. The base seed was 20261003, with identical input arrays for all trees in a trial.

- `benchmark.csv`: raw times and seeds; `benchmark.log`: progress output.
- `summary.csv`: medians, minima, and maxima for each metric.
- `runtime_comparison.png` / `.svg`: nine panels with median curves and min/max bands.
- `../performance_charts/`: nine individual comparisons and an overview in PNG/SVG. `source_data.csv` is a copy of the raw measurements; `chart_values.csv` holds the plotted statistics.

Post-run checks confirmed the complete grid of 225 unique trials, three repetitions per combination, matching seeds across trees, finite positive times, and insertion-plus-deletion totals. Both summary tables were independently recomputed from the raw data. The source copy, source hashes, raw-data hash, and stress logs were checked as well.

## Interpreting the results

These times describe this machine, compiler, and run. CPU frequency and process affinity were not fixed, and no separate warmup was used. Scheduling, allocation, and cache state can affect short trials. Validation between phases also touches the tree. Use the repeat range and expected complexity when discussing differences; the data do not establish a hardware-independent ranking.
