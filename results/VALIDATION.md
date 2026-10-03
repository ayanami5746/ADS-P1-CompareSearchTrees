# 实际验证记录

运行日期：2026-10-03（Asia/Shanghai）。本文是测试交接记录，不是课程报告正文。

## 环境与编译

- CPU：AMD Ryzen 9 8945HX with Radeon Graphics。
- 系统：Windows NT 10.0.26200.0，x86-64。
- 编译器：MinGW-w64 GCC 14.2.0，C11，`-O2`。
- 严格检查：`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`，编译通过，无警告。
- 另以 GCC `-fanalyzer` 检查 `src/trees.c`，未产生诊断。
- 绘图：Python 3.11 / Matplotlib 3.10.8。

## 正确性

在指定项目目录重新编译并运行 `./build.ps1 -Stress`，全部通过。原始输出见 `correctness.log`。

每种树均执行：边界测试、14400 对完整插入/删除排列、4 个种子共 120000 次混合随机操作、N=100000 的三种指定操作序列，以及含 20000 个有序键的非空树销毁。五种树合计 72000 对排列和 600000 次混合操作。布尔数组参考模型独立于树的实现。

普通测试和压力测试均已通过。一键复现脚本另以 `-MaxN 1000 -Repeats 1` 完成编译、普通正确性测试、注释审计、15 条基准测量及绘图验证。

GitHub Actions 初次运行 [37095035374](https://github.com/ayanami5746/ADS-P1-CompareSearchTrees/actions/runs/37095035374) 已通过：Linux 严格编译、正确性测试、注释检查、AddressSanitizer、UndefinedBehaviorSanitizer、泄漏检测以及基准 smoke test。CI 未运行 100000 规模压力测试；该项在本机执行。

## 注释审计

独立且有实质文本的注释行 / 全部物理行（包括空白行），各文件分别检查：

| 文件 | 注释行 / 总行数 | 比例 |
|---|---:|---:|
| `include/trees.h` | 9 / 24 | 37.50% |
| `src/trees.c` | 316 / 852 | 37.09% |
| `src/benchmark.c` | 85 / 215 | 39.53% |
| `tests/test_trees.c` | 94 / 253 | 37.15% |
| 总计 | 504 / 1344 | 37.50% |

机器可读明细见 `comment_coverage.json`，审计程序为 `tools/check_comments.py`。

## 完整性能实验

执行命令：

```powershell
./build/benchmark.exe --output results/benchmark.csv --repeats 3 --seed 20261003
python tools/plot_results.py results/benchmark.csv --output-dir results
```

- 五种树 × 三种场景 × 五种规模 × 三次重复，共 **225 条原始测量**。
- N = 1000、3000、10000、30000、100000；每轮为 N 次插入加 N 次删除。
- `benchmark.csv` 保留全部原始测量、轮次和实际输入种子。
- `summary.csv` 按树、场景、N、阶段提供三次重复的中位数及最小/最大值。
- `runtime_comparison.png` / `.svg` 分别提供位图和矢量图，九个面板覆盖三种场景与插入/删除/总耗时。
- 全部计时批次在插入后、删除后都检查成功操作数量、元素数和结构不变量。

本次测量反映这台机器、当前编译选项和当前负载。未固定 CPU 频率或进程亲和性，也未安排独立预热；短时测量会受到调度、分配器和缓存的影响。插入与删除阶段之间的验证会访问树节点。报告应结合算法复杂度及重复结果解释数据，不应将单次耗时当作普遍排序。
