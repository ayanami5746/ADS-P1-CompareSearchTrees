# ADS-P1-CompareSearchTrees

本仓库负责项目的 **C 语言实现、正确性测试及性能实验**，不包含课程报告正文。

## 手动运行：带 main() 的交互程序

`src/main.c` 是清晰独立的程序入口，流程是创建五棵空树 → 选择树 → 菜单操作 → 释放内存。算法保留在 `src/trees.c`，便于阅读与独立测试。

在项目目录运行：

```powershell
./build.ps1
./build/search_trees.exe
```

也可以直接编译，不需要 Python：

```powershell
New-Item -ItemType Directory -Force build
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/main.c src/trees.c -o build/search_trees.exe
./build/search_trees.exe
```

Linux/macOS 使用 `make all` 后运行 `./build/search_trees`。

启动时输入树编号：1=BST、2=AVL、3=Splay、4=RedBlack、5=BPlus。随后菜单为：1=插入、2=删除、3=查询、4=元素数量、5=检查结构、6=切换树、0=退出。插入、删除、查询会另外提示输入键；**每行输入一个整数**，不要把命令和键写在同一行。

例如依次输入 `2`、`1`、`10`、`3`、`10`、`0`（每项一行），就是选择 AVL、插入 10、查询 10、退出。五棵树各自维护独立集合，切换后数据仍保留。重复键、删除不存在的键、非法文本、越界整数、超长行都有提示；输入结束时自动释放所有树并退出。控制台使用英文提示，避免不同 Windows 终端编码导致乱码。

交互回归检查：`python tests/test_interactive.py`，覆盖所有树的菜单操作、独立集合切换、异常输入、整数边界、EOF，以及注释统计口径。

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

正确性测试完整保存在独立文件 `tests/test_trees.c`，性能测试完整保存在独立文件 `src/benchmark.c`，均与 `src/trees.c` 分离。也可以直接编译测试：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/trees.c tests/test_trees.c -o build/test_trees.exe
./build/test_trees.exe
```

首次复现先运行 `python -m pip install -r requirements.txt`，然后运行 `./run_experiments.ps1 -Stress`，一次完成编译、压力测试、注释审计、完整性能实验和绘图。快速复现可用 `./run_experiments.ps1 -MaxN 1000 -Repeats 1`；不需要绘图时附加 `-SkipPlots`。每次运行会更新 `results/` 中同名数据和日志，需保留的旧结果请先复制。

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

`tools/check_comments.py` 对 `include/`、`src/`、`tests/` 的每个 `.c/.h` 文件分别强制 ≥35%。按用户指定口径：**包含真实 C 注释的物理行 / 所有物理行（含空白行）**。独立注释、行尾注释、块注释起止符和块内行都计入，每行最多计一次；字符串或字符字面量里的 `//`、`/*` 不算注释。注释解释算法不变量、前置条件、边界情况和测试意图。

## 文件组织

- `src/trees.c`、`include/trees.h`：五种树及统一接口。
- `src/main.c`：带 `main()` 的交互入口，生成 `build/search_trees.exe`。
- `src/benchmark.c`：确定性实验输入与计时输出。
- `tests/test_trees.c`：正确性与压力测试。
- `tools/`：注释审计、绘图；`results/`：实测数据及交付给报告同学的材料。
- `.github/workflows/ci.yml`：持续编译、测试、注释比例和内存检查。
