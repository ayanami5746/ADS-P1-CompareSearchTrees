# ADS-P1-CompareSearchTrees

A shared workspace for our search-tree course project: C implementations, tests, benchmark data, figures, and report materials. The project compares an unbalanced BST, an AVL tree, a splay tree, a red-black tree, and a B+ tree under the same insertion and deletion workloads.

## Build and run

On Windows, install GCC and use PowerShell:

```powershell
./build.ps1 -Test
./build.ps1 -Stress
```

The build uses C11, `-O2`, and strict warnings treated as errors. Regular tests use 5,000 keys for each assignment workload; stress tests use 100,000. Both commands also run the structural regression checks. Ordered operations on the unbalanced BST can take quadratic time, so a full run takes several minutes.

To rebuild, test, benchmark, and regenerate both sets of performance charts:

```powershell
python -m pip install -r requirements.txt
./run_experiments.ps1 -Stress -Repeats 3 -MaxN 100000 -Seed 20261003
```

For a shorter run, use `-MaxN 1000 -Repeats 1`. Add `-SkipPlots` to skip chart generation. These commands overwrite the corresponding result files; copy any results you want to keep first. The checked-in `results/run_metadata.json` and `results/VALIDATION.md` describe the published full run; the reproduction script does not update those two provenance records automatically.

On Linux or macOS, use `make test`, `make stress`, and `make all`. Where the compiler supports them, `make sanitize` runs AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection on both test programs. GitHub Actions runs the regular tests, sanitizer checks, and a small benchmark.

## Code structure

`src/trees.c` is the algorithm library and has no `main()`. The set-model tests, structural regression checks, and benchmark have separate entry points. Start with `include/trees.h`, then read the public dispatch functions in section 8 of `src/trees.c`.

| Section | Responsibility |
|---|---|
| 1. Data structures | Binary nodes, B+ pages, and the owning tree object |
| 2. Shared binary-tree primitives | Allocation, rotations, and ordinary lookup |
| 3. Unbalanced BST | Iterative lookup, insertion, and successor-based deletion |
| 4. AVL tree | Height updates and single or double rotations |
| 5. Splay tree | Bottom-up splaying, insertion, and deletion by joining subtrees |
| 6. Red-black tree | Insertion and deletion repair through recoloring and rotations |
| 7. B+ tree | Routing, splitting, borrowing, merging, and root changes |
| 8. Public API and operation dispatch | Select the algorithm and update the element count |
| 9. Structural validation | Check ordering, counts, and each tree's invariants |
| 10. Memory cleanup | Release nodes and pages |

For example, BST insertion follows `tree_insert -> bst_insert -> bst_find_slot/new_node`. B+ deletion follows `tree_delete -> bplus_delete -> bp_delete -> repair_child`. Algorithm helpers are private to the source file. The public layer changes `size` once for each successful insertion or deletion.

## Tree implementations

The five implementations share the same set interface, with balancing handled inside each tree module.

| Tree | Implementation |
|---|---|
| BST | Iterative updates, with successor replacement for a node with two children. No shortcuts for sorted input. |
| AVL | Empty height is -1 and leaf height is 0. Single and double rotations restore a balance factor in {-1, 0, 1}. Deletion repairs the path back to the root. |
| Splay | Bottom-up **zig, zig-zig, and zig-zag**. Insert as in a BST and splay the inserted node. Lookup splays the accessed node; a miss splays the last visited node. Deletion proceeds as follows: splay the target, remove it, splay the maximum of the left subtree, then attach the right subtree. |
| Red-black | Ordinary red-black trees. Insert a red leaf, then handle a red uncle or inner/outer child. Deletion handles the four sibling/nephew cases and their mirror images. A two-child deletion copies the successor key while retaining the destination node's color. NULL children represent black NIL leaves. |
| B+ | **Order 4**. Nonroot internal pages have 2-4 children; nonroot leaves have 2-4 records. Separators copy the smallest key in each child except the first. All records are in linked leaves at the same depth. |

B+ insertion splits a page when it overflows: five records or child pointers split into three on the left and two on the right, with splits propagated upward. Deletion borrows from a sibling when possible, otherwise merges pages and collapses a root with one remaining child. The validator checks root occupancy separately from other pages.

## API behavior

All five trees store distinct `int` keys, including `INT_MIN` and `INT_MAX`. Duplicate insertion and deletion of an absent key return `false`. Splay queries and duplicate insertions may change the shape without changing the set. `tree_size()` counts records, not B+ separator copies.

Create handles with `tree_create()` and release them with `tree_destroy()`. An invalid tree kind returns NULL. `tree_destroy(NULL)` is allowed; other operations require a valid, non-NULL handle. Allocation failure prints an error and exits. The algorithms and their tests use C11; Python is used for tooling and figures.

## Test cases and their purpose

[tests/test_trees.c](tests/test_trees.c) runs the same set-model suite against all five trees. Cases 1-3 are the assignment workloads. The other cases check correctness under boundaries, repeated operations, and mixed updates.

| Test | What happens | Data or scale | Purpose |
|---|---|---|---|
| Empty tree | Look up and delete a missing key | Key `42` | Return false without accessing a missing node |
| Duplicates | Insert each key twice, then delete it twice | Boundary-test keys | Preserve set semantics and the element count |
| Integer boundaries | Insert, find, and delete special values | `0`, `+/-1`, `+/-100`, `INT_MIN`, `INT_MAX` | Check the full integer range without reserving a sentinel key |
| Reuse after clearing | Delete every key, then insert and delete another key in the same tree | One reused tree object | Catch stale roots or leaf links |
| Exhaustive small orders | Try every insertion order with every deletion order | Five keys; 120 x 120 = **14,400 pairs per tree** | Exercise different shapes, root changes, successor replacement, and the first B+ leaf split |
| Random mixed operations | Interleave insertion, deletion, and lookup; compare with a boolean array | Keys -1024..1023; four seeds x 30,000 steps = **120,000 operations per tree** | Detect lost keys, incorrect return values, and count or structural errors |
| **Case 1: ascending / ascending** | Insert `0 1 2 3 4`, then delete `0 1 2 3 4` | N = 5,000 or 100,000 | Check sorted input and repeated removal from the smallest end |
| **Case 2: ascending / descending** | Insert `0 1 2 3 4`, then delete `4 3 2 1 0` | N = 5,000 or 100,000 | Check reverse deletion and the cost of repeatedly reaching deep nodes in a degenerate BST |
| **Case 3: random / random** | Shuffle insertion and deletion orders separately | Same key set; N = 5,000 or 100,000 | Check updates under unordered input |
| Destroy a populated tree | Insert sorted keys and destroy the tree directly | **20,000 keys per tree** | Exercise cleanup, including a long BST chain |
| Special API arguments | Destroy NULL; create an invalid tree kind | `tree_destroy(NULL)`, `tree_create(TREE_COUNT)` | Check the documented interface behavior |

In Cases 1-3, all insertions finish before deletion begins. In the mixed test, insertion, deletion, and lookup are interleaved. A **seed** is the random generator's initial value: the same generator, seed, and call order produce the same sequence, making failures reproducible.

The exhaustive suite validates after every update. The mixed suite checks results, size, and structure after every operation, and scans the full key range every 1,000 steps. Large workloads validate after insertion, every 1,024 deletion steps, and after the final deletion.

[tests/test_lecture.c](tests/test_lecture.c) adds direct checks of the private structure. It includes the implementation in its own executable, so no test-only interface is exposed to callers and its counters are absent from the benchmark.

| Structural check | Purpose |
|---|---|
| Both directions of zig, zig-zig, and zig-zag | Check the resulting links, root, and exact rotation count |
| Splay access to `1` after inserting `1..7`, deletion, misses, and duplicates | Check access-to-root behavior and the predecessor-root join |
| Red-black insertion of `4` into a fixed eight-key tree | Check the expected root, links, and colors after recoloring and rotations |
| 30,000 mixed red-black updates | Require all three insertion cases and all four deletion cases on both sides; enforce at most two insertion rotations and three deletion rotations |
| B+ leaf capacity, split, borrow, merge, and root collapse | Check the order-four occupancy rules and separator update |

Structural validation checks more than whether keys can be found:

| Tree | Invariants checked |
|---|---|
| BST / Splay | Global key order and count; Splay also checks parent links |
| AVL | Key order, count, cached heights, and a height difference of at most one |
| Red-black | Key order, count, parent links, black root, no red-red edge, and equal black height; red children may be on either side |
| B+ | Page occupancy, separator minima, equal leaf depth, leaf links, key order, and record count |

Sanitizer runs supplement these checks by detecting invalid memory access, undefined behavior, and leaks.

## Performance experiment

The independent program [src/benchmark.c](src/benchmark.c) measures Cases 1-3.

| Setting | Value | Reason |
|---|---|---|
| Input sizes | 1,000 / 3,000 / 10,000 / 30,000 / 100,000 | Observe how runtime changes with N |
| Key set | Distinct integers `0..N-1` | Change operation order while keeping the set fixed |
| Repetitions | Three per tree, case, and size | Report the median and observed range |
| Shared inputs | Identical arrays for all trees within a repetition | Keep input differences out of the comparison |
| Timing | Insertion, deletion, and their sum | Compare the two phases separately |
| Full dataset | 5 trees x 3 cases x 5 sizes x 3 repetitions = **225 rows** | Cover every combination |

Fisher-Yates shuffling uses fixed-width xorshift32 with rejection sampling. The CSV records the seed of each trial; repetitions change the seed and rotate the tree execution order. The timer is QueryPerformanceCounter on Windows and CLOCK_MONOTONIC on POSIX systems.

Timing includes normal node allocation/freeing and accumulation of operation return values. Input generation, validation, CSV output, and final tree destruction are outside the timed regions. Validation between insertion and deletion touches the nodes and can affect cache state. There is no separate warmup or fixed CPU affinity. Short measurements are sensitive to scheduling, allocation, and cache behavior; interpret them alongside the repeat range and algorithmic complexity.

CSV columns: `tree,scenario,n,repeat,seed,insert_seconds,delete_seconds,total_seconds`.

```powershell
./build/benchmark.exe --output results/benchmark.csv --repeats 3 --seed 20261003
python tools/plot_results.py results/benchmark.csv --output-dir results
python tools/plot_case_comparisons.py results/benchmark.csv --output-dir performance_charts
```

`--max-n` selects an upper limit from the fixed size list; it does not add a new size. A quick smoke run is `./build/benchmark.exe --max-n 1000 --repeats 1 --output build/smoke.csv`.

## Data and figures

- `results/benchmark.csv`: raw measurements; `benchmark.log`: progress log.
- `results/summary.csv`: median, minimum, and maximum for each metric.
- `results/correctness.log` and `results/lecture_checks.log`: test output.
- [results/VALIDATION.md](results/VALIDATION.md) and `results/run_metadata.json`: the published run's environment, source revision, and hashes.
- `results/runtime_comparison.png` / `.svg`: a nine-panel plot with median curves and min/max bands.
- [performance_charts/](performance_charts/README.md): nine individual comparisons and one overview, each in PNG and SVG, plus the source data and plotted values.
- `project_diagrams/`: the two diagrams below, in PNG and SVG.
- `.github/workflows/ci.yml`: automated build, correctness, and memory checks.

Both performance-chart sets use logarithmic axes and measured medians, without fitted replacement curves. The total-time median is calculated from each trial's insertion-plus-deletion time.

## Code and test diagrams

![Code structure](project_diagrams/01_code_structure.png)

![Test cases and validation](project_diagrams/02_test_cases.png)

Regenerate the diagrams with `python tools/draw_project_diagrams.py`. Their blue, white, and gray layout is intended for the README and the team report.
