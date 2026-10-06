# Search-tree performance comparisons

Each figure compares BST, AVL, Splay, RedBlack, and BPlus using a white background, thin lines, muted colors, and distinct markers. No fitted curves replace the measurements.

| File prefix | Workload |
|---|---|
| `case1` | Ascending insertion, ascending deletion |
| `case2` | Ascending insertion, descending deletion |
| `case3` | Random insertion, random deletion |

Each case has an `insert`, `delete`, and `total` figure. `overview` combines all nine panels. Every figure is available as a 240 dpi PNG and a scalable SVG.

The horizontal axis is the number of keys N; the vertical axis is elapsed time in seconds. Both axes use logarithmic scales. Panels have independent vertical ranges, so compare the axis values rather than visual line heights across panels. Points are medians of three trials at N = 1,000, 3,000, 10,000, 30,000, and 100,000. Total-time medians are calculated from each trial's insertion-plus-deletion time.

All figures use the **225 measurements** in `../results/benchmark.csv`, copied here as `source_data.csv`. The run finished at **2026-10-06 13:47:08 (Asia/Shanghai)**, using source revision `734a463` on Windows, an AMD Ryzen 9 8945HX, and GCC 14.2.0 with `-O2`. The benchmark uses bottom-up Splay, ordinary red-black repair, and order-four B+. Height maintenance is limited to AVL.

`chart_values.csv` stores each point's median, minimum, maximum, and sample count. See [the validation record](../results/VALIDATION.md) and `../results/run_metadata.json` for provenance and measurement limits.

To regenerate from the repository root:

```powershell
python tools/plot_case_comparisons.py results/benchmark.csv --output-dir performance_charts
```
