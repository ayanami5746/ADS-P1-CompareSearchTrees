# 实际验证记录

本次运行：2026-10-03 15:46:48 至 2026-10-03 15:51:37（Asia/Shanghai）。记录测试环境、验证过程与实验数据来源，供小组成员复现实验、分析结果和撰写报告时参考。

## 数据对应版本

本次从当前代码重新编译并完整运行，包含算法结构整理后的实现。被测源码提交为 [3a7bd97](https://github.com/ayanami5746/ADS-P1-CompareSearchTrees/commit/3a7bd97bb777650f470ffd32c4694ef55ef16d7e)。数据更新提交会在此之后产生；此次更新没有修改 C 算法及测试代码。

`run_metadata.json` 记录源码提交、各编译输入的 SHA-256、运行起止时间、环境、实验参数和原始测量文件 SHA-256。当前原始数据、汇总表和所有性能图均来自同一次新实验。

## 环境与编译

- CPU：AMD Ryzen 9 8945HX with Radeon Graphics。
- 系统：Microsoft Windows NT 10.0.26200.0。
- 编译器：gcc.exe (x86_64-win32-seh-rev2, Built by MinGW-Builds project) 14.2.0。
- 编译选项：`-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2 -Iinclude`，编译通过，无警告。
- 计时：Windows QueryPerformanceCounter，单位为秒。

## 正确性

在项目目录重新编译并执行 `--stress`，五种树全部通过，原始输出见 `correctness.log`。

每种树均执行边界测试、14400 对完整插入/删除排列、4 个固定种子共 120000 次混合操作、N=100000 的三种指定操作序列，以及含 20000 个有序键的非空树销毁。五种树合计 72000 对排列和 600000 次混合操作。布尔数组参考模型独立于树的实现。

每次混合操作检查返回值、数量及结构；定期逐键核对全域。大规模序列在全部插入后、删除过程中及清空后检查结构。性能计时批次也在插入、删除阶段之后检查操作成功数量及结构，检查耗时不计入操作时间。

## 完整性能实验

```powershell
./run_experiments.ps1 -Stress -Repeats 3 -MaxN 100000 -Seed 20261003
python tools/plot_case_comparisons.py results/benchmark.csv --output-dir performance_charts
```

- 五种树 × 三种场景 × 五种规模 × 三次重复，共 **225 条新测量**。
- N = 1000、3000、10000、30000、100000；每轮为 N 次插入加 N 次删除。
- 三种场景：递增插入/递增删除、递增插入/递减删除、随机插入/随机删除。
- 基础种子为 20261003；同一规模、场景和轮次下五种树使用相同输入。
- `benchmark.csv` 保存每轮原始耗时与输入种子，`benchmark.log` 保存实验进度。
- `summary.csv` 保存中位数、最小和最大耗时。
- `runtime_comparison.png` / `.svg` 为带重复范围阴影的总览图。
- `../performance_charts/` 中九张单图及一张总览（PNG/SVG）均已重新生成；该目录的 `source_data.csv` 与原始数据一致，`chart_values.csv` 保存对应绘图数值。

完成后核对了 225 条记录的组合完整性、唯一性、三次重复、各树输入种子一致性及总耗时。两套汇总表的中位数、最小值和最大值均从原始数据独立复算核对。

本次测量反映当前机器、编译选项和负载。未固定 CPU 频率或进程亲和性，未安排独立预热；短时测量会受到调度、分配器和缓存影响。插入与删除之间的结构检查会访问节点。报告应结合复杂度和重复波动解释结果，不应把单次耗时当作普遍排序。
