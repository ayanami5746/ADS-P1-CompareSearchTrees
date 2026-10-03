# ADS-P1-CompareSearchTrees

本仓库负责项目的 **C 语言实现、正确性测试及性能实验**，不包含课程报告正文。

## 实现范围

| 搜索树 | 实现选择 |
|---|---|
| 非平衡 BST | 迭代插入、删除、查询、销毁；不针对有序输入做捷径优化 |
| AVL | 高度维护、单旋与双旋、插入和删除后恢复平衡 |
| Splay | 自顶向下伸展；查找失败也进行伸展 |
| Red-black | 左倾红黑树（LLRB），属于红黑树的一种实现 |
| B+ | 16 阶：内部节点最多 16 个孩子，叶子最多 15 条记录，叶子链连接 |

统一接口位于 `include/trees.h`。集合接受完整 `int` 范围；重复插入与不存在的删除返回 `false`，不改变元素集合。有效树对象必须由 `tree_create` 创建；除 `tree_destroy(NULL)` 外，接口要求传入非空有效对象。内存不足打印错误并退出。底层算法和测试均为 C11；Python 仅用于注释审计和绘图。实现以题目截图的五种树和三种序列要求为依据；未提供的课程专用命名、文件结构或评分规则不作假设。

## Windows 编译与测试

需要 GCC（本地使用 MinGW-w64 GCC 14.2.0）和 PowerShell。

```powershell
./build.ps1 -Test
./build.ps1 -Stress
python tools/check_comments.py --output results/comment_coverage.json
```

开启 `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2`，警告按错误处理。`--stress` 对每种树执行 N=100000 的全部三种序列；普通测试使用 N=5000。非平衡 BST 的有序插入是 O(N²)，逆序删除也可能是 O(N²)，因此压力测试和完整基准需要等待。

Linux/macOS 可使用 `make test`、`make stress`、`make comments`。支持的 GCC/Clang 环境可运行 `make sanitize`；GitHub Actions 在 Linux 上执行 AddressSanitizer、UndefinedBehaviorSanitizer 和泄漏检测。不要把尚未执行的 CI 配置当作已经通过的测试结果。

## 正确性验证

- 空树、单节点、重复键、不存在的键、`INT_MIN`、`INT_MAX`、清空后复用。
- 每种树遍历 5 个键全部 120 种插入排列 × 120 种删除排列，共 14400 对排列，每步检查结构。
- 每种树 4 个固定种子、共 120000 次混合操作，与独立布尔数组集合对照；每次操作检查元素数和结构，定期逐键核对全域。
- 三种指定序列，以及非空退化树直接销毁。
- BST/Splay：严格键序和节点数；AVL：额外核对实际高度和平衡因子；红黑树：根黑、无红红边、黑高相等、左倾；B+：占用率、分隔键、所有叶子等深、叶子链和记录数。

## 可复现实验

```powershell
New-Item -ItemType Directory -Force results
./build/benchmark.exe --output results/benchmark.csv --repeats 3 --seed 20261003
python -m pip install -r requirements.txt
python tools/plot_results.py results/benchmark.csv --output-dir results
```

默认 N 为 1000、3000、10000、30000、100000。三个场景分别是递增插入/递增删除、递增插入/递减删除、随机插入/独立随机删除。每个场景都使用 0..N-1 的互异整数。相同 N、场景、重复轮次下，所有树使用完全相同的操作序列。Fisher–Yates 洗牌采用固定宽度 xorshift32 和拒绝采样；CSV 记录各轮种子，重复轮次间改变种子并轮换树的运行顺序。

计时器在 Windows 为 QueryPerformanceCounter，POSIX 为 CLOCK_MONOTONIC。分别计时全部插入、全部删除，同时记录二者之和。计时包含算法正常的节点分配/释放及操作返回值累计；不包含输入生成、结构验证、CSV 输出、最终树销毁。插入后的校验会访问节点、影响缓存状态，这是所有树一致采用的实验约定。没有额外预热；短耗时结果需要结合重复波动解释，不能作为硬件无关的结论。

CSV 字段：`tree,scenario,n,repeat,seed,insert_seconds,delete_seconds,total_seconds`。绘图脚本生成 `summary.csv` 和 PNG/SVG 九宫格曲线，行对应三种序列，列对应插入/删除/总时间；双对数坐标，中位数曲线与 min/max 阴影。原始测量不做拟合替换。报告同学可直接复用数据和图片，并参考 `results/VALIDATION.md` 中的实际运行环境与验证记录。

快速检查：`./build/benchmark.exe --max-n 1000 --repeats 1 --output build/smoke.csv`。`--max-n` 是默认规模列表的上界，不会额外创建新规模。

## 注释比例

`tools/check_comments.py` 对 `include/`、`src/`、`tests/` 的每个 `.c/.h` 文件分别强制 ≥35%。采用更严格的口径：**有实质文本且不混有代码的注释物理行 / 所有物理行（含空白行）**。独立注释起止符、空白行、代码行末注释均不计入分子，字符串内的 `//` 或 `/*` 也不算注释。注释解释算法不变量、前置条件、边界情况和测试意图。

## 文件组织

- `src/trees.c`、`include/trees.h`：五种树及统一接口。
- `src/benchmark.c`：确定性实验输入与计时输出。
- `tests/test_trees.c`：正确性与压力测试。
- `tools/`：注释审计、绘图；`results/`：实测数据及交付给报告同学的材料。
- `.github/workflows/ci.yml`：持续编译、测试、注释比例和内存检查。
