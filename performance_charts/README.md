# 搜索树性能对比图

每张图比较 BST、AVL、Splay、RedBlack、BPlus 五种树。白底、细线、低饱和配色，并用不同标记与线型辅助区分；不添加装饰和拟合曲线。

| 文件前缀 | 测试场景 |
|---|---|
| `case1` | 递增插入、递增删除 |
| `case2` | 递增插入、递减删除 |
| `case3` | 随机插入、随机删除 |

每个 case 包含 `insert`（插入）、`delete`（删除）、`total`（插入与删除总耗时）三张图。`overview` 是全部九张图的总览。每张图均提供 PNG（240 dpi）和可缩放 SVG 两种格式。

横轴是整数数量 N，纵轴是运行时间，单位秒。两轴均使用对数刻度，因为不同树的耗时差异较大；各面板的纵轴范围独立，应根据坐标数值比较。每个点为三次实测的中位数，N 为 1000、3000、10000、30000、100000。总耗时曲线使用每轮插入加删除的总时间取中位数。

数据来源为项目现有 `results/benchmark.csv`，共 225 条测量，复制保存在本目录的 `source_data.csv`。这些测量于 2026-10-03 15:51:37（Asia/Shanghai）完成，使用结构整理后的源码版本 `3a7bd97` 重新编译、计时取得，环境为 Windows / AMD Ryzen 9 8945HX / GCC 14.2.0 -O2。全部图表由本次新数据重新生成；详细版本与环境记录见 `../results/run_metadata.json` 和 `../results/VALIDATION.md`。`chart_values.csv` 保留各点的中位数及最小、最大值，便于核对。短时测量会受到系统负载和缓存等影响。

从项目根目录重新生成：

```powershell
python tools/plot_case_comparisons.py results/benchmark.csv --output-dir performance_charts
```
