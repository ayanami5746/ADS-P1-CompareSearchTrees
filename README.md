# ADS-P1-CompareSearchTrees

本仓库用于小组协作完成 **搜索树实现与性能比较** 课程项目，统一管理代码、测试、实验数据、图表及报告材料。

当前已提供五种搜索树的 C 语言实现、自动化测试和性能实验工具。后续小组成员可在此基础上共同完善实现、分析实验结果并撰写项目报告。

## 代码结构与阅读顺序

项目采用算法库、独立正确性测试和独立性能实验程序的结构。`main()` 仅保留在 `tests/test_trees.c` 和 `src/benchmark.c` 两个可执行程序中。

算法集中在 `src/trees.c`，按以下十个明确分区组织，便于课程阅读与提交：

| 分区 | 职责 |
|---|---|
| 1. Data structures | 二叉节点、B+ 页、Tree 的数据结构及字段含义 |
| 2. Shared binary-tree primitives | 内存分配、旋转、普通二叉查找 |
| 3. Unbalanced BST | BST 定位、插入、删除 |
| 4. AVL tree | 高度维护、平衡修复、递归更新 |
| 5. Splay tree | 伸展、插入和删除后的子树拼接 |
| 6. Left-leaning red-black tree | 旋转、变色、插入、删除及根颜色处理 |
| 7. B+ tree | 页内操作、分裂、借位、合并和根变化 |
| 8. Public API and operation dispatch | 公共接口、按树类型分派、统一更新 size |
| 9. Structural validation | 各种树的不变量与数量检查 |
| 10. Memory cleanup | 释放二叉树和 B+ 页 |

建议先读 `include/trees.h` 了解接口，再读第 8 区的 `tree_insert()`、`tree_delete()`，最后进入感兴趣的树模块。公共接口使用清晰的 `switch`，每个分支只调用对应算法；具体的节点操作不再混在大型条件分支内。

例如 BST 的插入调用链为 `tree_insert → bst_insert → bst_find_slot/new_node`，成功后由 `tree_insert` 统一增加 size。B+ 的删除调用链为 `tree_delete → bplus_delete → bp_delete → repair_child`，页修复和根收缩分别在对应层完成。

所有算法函数保持文件内 `static`，外部仍只使用 `trees.h` 中的公共接口。BST、Splay 及 B+ 的包装函数负责结构变化；元素总数只由公共更新接口修改，避免重复计数。

## 实现范围

| 搜索树 | 实现选择 |
|---|---|
| 非平衡 BST | 迭代插入、删除、查询、销毁；不针对有序输入做捷径优化 |
| AVL | 高度维护、单旋与双旋、插入和删除后恢复平衡 |
| Splay | 自顶向下伸展；查找失败也进行伸展 |
| Red-black | 左倾红黑树（LLRB），属于红黑树的一种实现 |
| B+ | 16 阶：内部节点最多 16 个孩子，叶子最多 15 条记录，叶子链连接 |

统一接口位于 `include/trees.h`。集合接受完整 `int` 范围；重复插入与不存在的删除返回 `false`，不改变元素集合。有效树对象必须由 `tree_create` 创建；除 `tree_destroy(NULL)` 外，接口要求传入非空有效对象。内存不足打印错误并退出。底层算法和测试均为 C11；Python 用于辅助工具和绘图。实现覆盖课程要求的五种搜索树和三种插入、删除序列。

## Windows 编译与测试

需要 GCC（本地使用 MinGW-w64 GCC 14.2.0）和 PowerShell。

```powershell
./build.ps1 -Test
./build.ps1 -Stress
```

开启 `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2`，警告按错误处理。`--stress` 对每种树执行 N=100000 的全部三种序列；普通测试使用 N=5000。非平衡 BST 的有序插入是 O(N²)，逆序删除也可能是 O(N²)，因此压力测试和完整基准需要等待。

正确性测试完整保存在独立文件 `tests/test_trees.c`，性能测试完整保存在独立文件 `src/benchmark.c`，均与 `src/trees.c` 分离。也可以直接编译测试：

```powershell
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude src/trees.c tests/test_trees.c -o build/test_trees.exe
./build/test_trees.exe
```

首次复现先运行 `python -m pip install -r requirements.txt`，然后运行 `./run_experiments.ps1 -Stress`，一次完成编译、压力测试、完整性能实验和绘图。快速复现可用 `./run_experiments.ps1 -MaxN 1000 -Repeats 1`；不需要绘图时附加 `-SkipPlots`。每次运行会更新 `results/` 中同名数据和日志，需保留的旧结果请先复制。

Linux/macOS 可使用 `make test`、`make stress`。支持的 GCC/Clang 环境可运行 `make sanitize`；GitHub Actions 在 Linux 上执行 AddressSanitizer、UndefinedBehaviorSanitizer 和泄漏检测。不要把尚未执行的 CI 配置当作已经通过的测试结果。

## 正确性验证与测试 Case

正确性测试位于 [tests/test_trees.c](tests/test_trees.c)，BST、AVL、Splay、RedBlack、BPlus 均运行相同的测试套件。**Case 1、2、3 是题目要求的三种操作序列，其余测试用于检查实现是否正确、可靠。**

| 测试类别 | 具体操作 | 测试规模或数据 | 测试目的 |
|---|---|---|---|
| 空树操作 | 在空树中查询、删除不存在的数字 | 查询、删除 `42` | 正确返回“不存在”，避免空指针访问或崩溃 |
| 重复插入与删除 | 同一数字插入两次，再删除两次 | 对边界测试中的每个键执行 | 拒绝重复记录，保证重复删除安全且不错误修改元素数量 |
| 特殊数值 | 插入、查询、删除零、正负数和整数极值 | `0`、`±1`、`±100`、`INT_MIN`、`INT_MAX` | 检查完整整数范围内的边界处理，避免误用特殊键作为哨兵 |
| 空树复用 | 删除所有元素后，在同一个树对象中重新插入并删除一个元素 | 不重新创建树对象 | 检查清空后内部状态是否恢复正确，能否继续使用 |
| 小规模排列穷举 | 组合五个数字的全部插入顺序和删除顺序 | `{0,1,2,3,4}`；120 × 120 = **14,400 组 / 树** | 覆盖不同树形和删除顺序，发现旋转、根替换、后继替换等错误 |
| 随机混合操作 | 随机插入、删除、查询，与独立布尔数组对照 | 键范围 `−1024～1023`；4 个种子 × 30,000 步 = **120,000 次 / 树** | 检查连续复杂操作下是否丢失元素、返回错误结果或数量不一致 |
| **Case 1：递增插入、递增删除** | 例如先插入 `0→1→2→3→4`，再删除 `0→1→2→3→4` | 普通 N=5,000；压力 N=100,000 | 验证有序输入及从最小端连续删除的正确性，比较各树在此场景下的性能 |
| **Case 2：递增插入、递减删除** | 例如先插入 `0→1→2→3→4`，再删除 `4→3→2→1→0` | 普通 N=5,000；压力 N=100,000 | 检查反向删除时的结构修复，观察普通 BST 退化后反复访问深层节点的代价 |
| **Case 3：随机插入、随机删除** | 对同一组数字分别打乱插入和删除顺序 | 普通 N=5,000；压力 N=100,000 | 验证无序输入下的连续更新，并比较随机序列中的性能 |
| 非空树直接销毁 | 递增插入数字后，不逐个删除，直接释放整棵树 | **20,000 个元素 / 树** | 检查非空树清理路径，尤其是普通 BST 长链结构能否安全销毁 |
| 特殊接口参数 | 销毁空指针，使用无效树类型创建对象 | `tree_destroy(NULL)`、`tree_create(TREE_COUNT)` | 检查这些特殊参数是否按接口约定安全处理 |

三类指定 Case 都是**先全部插入，再开始删除**。它们与随机混合测试不同：后者会把插入、删除和查询交错执行。固定种子是随机数生成器的初始值；相同算法和调用顺序下，相同种子会生成相同操作序列，便于复现错误。

穷举测试在每次更新后检查结构；随机混合测试每步核对返回值、元素数量和结构，并每隔 1000 步逐键核对整个取值范围。大规模 Case 在全部插入后、删除过程中每隔 1024 步及清空后检查结构。五个键的穷举不会触发当前 B+ 树的叶子分裂，多页操作由随机测试和大规模 Case 覆盖。

测试不仅检查数字能否查到，还通过 `tree_validate()` 检查内部结构：

| 树类型 | 主要结构检查 |
|---|---|
| BST、Splay | 全局键序、实际节点数量 |
| AVL | 键序、数量、缓存高度与实际高度一致、左右高度差不超过 1 |
| RedBlack | 键序、数量、根为黑色、无连续红节点、黑高相等、红链接左倾 |
| BPlus | 节点占用率、分隔键、全部叶子等深、叶子链及实际记录数量 |

非空销毁等内存行为由 GitHub Actions 中的 AddressSanitizer 和泄漏检测辅助检查；UndefinedBehaviorSanitizer 用于检查未定义行为。

## 可复现实验

```powershell
New-Item -ItemType Directory -Force results
./build/benchmark.exe --output results/benchmark.csv --repeats 3 --seed 20261003
python -m pip install -r requirements.txt
python tools/plot_results.py results/benchmark.csv --output-dir results
```

性能实验位于 [src/benchmark.c](src/benchmark.c)，使用上述 Case 1、2、3，测量设置如下：

| 项目 | 默认设置 | 目的 |
|---|---|---|
| 输入规模 | N = `1,000、3,000、10,000、30,000、100,000` | 观察耗时随数据规模的变化 |
| 数据集合 | 每轮使用 `0..N-1` 的互异整数 | 保持元素集合一致，仅改变操作顺序 |
| 重复次数 | 每种树、Case、规模组合运行 **3 次** | 观察波动，并计算中位数及最小、最大值 |
| 公平性 | 同一规模、Case、轮次下，五种树使用完全相同的输入序列 | 避免输入差异干扰比较 |
| 测量内容 | 插入耗时、删除耗时及二者之和 | 分别观察两个更新阶段的代价 |
| 测量总量 | 5 种树 × 3 类 Case × 5 种规模 × 3 次重复 = **225 条记录** | 覆盖全部指定组合 |

Fisher–Yates 洗牌采用固定宽度 xorshift32 和拒绝采样；CSV 记录各轮种子，重复轮次间改变种子并轮换树的运行顺序。

计时器在 Windows 为 QueryPerformanceCounter，POSIX 为 CLOCK_MONOTONIC。分别计时全部插入、全部删除，同时记录二者之和。计时包含算法正常的节点分配/释放及操作返回值累计；不包含输入生成、结构验证、CSV 输出、最终树销毁。插入后的校验会访问节点、影响缓存状态，这是所有树一致采用的实验约定。没有额外预热；短耗时结果需要结合重复波动解释，不能作为硬件无关的结论。

CSV 字段：`tree,scenario,n,repeat,seed,insert_seconds,delete_seconds,total_seconds`。绘图脚本生成 `summary.csv` 和 PNG/SVG 九宫格曲线，行对应三种序列，列对应插入/删除/总时间；双对数坐标，中位数曲线与 min/max 阴影。原始测量不做拟合替换。小组成员分析结果或撰写报告时，可直接复用数据和图片，并参考 `results/VALIDATION.md` 中的实际运行环境与验证记录。

快速检查：`./build/benchmark.exe --max-n 1000 --repeats 1 --output build/smoke.csv`。`--max-n` 是默认规模列表的上界，不会额外创建新规模。

## 文件组织

- `src/trees.c`、`include/trees.h`：五种树及统一接口。
- `src/benchmark.c`：确定性实验输入与计时输出。
- `tests/test_trees.c`：正确性与压力测试。
- `tools/`：辅助工具与绘图；`results/`：共享实验数据、图表及验证记录。
- `.github/workflows/ci.yml`：持续编译、测试和内存检查。

## 独立性能对比图

[performance_charts](performance_charts/README.md) 单独保存三种 case 的插入、删除和总耗时对比图，共九张独立图和一张总览，均提供 PNG 与 SVG。采用现有实测数据的三次重复中位数；原始数据、绘图数值和复现方法也保存在该文件夹。

## 代码与测试说明图

代码结构图展示程序入口、统一接口、五种树实现及运行产物。

![代码整体结构](project_diagrams/01_code_structure.png)

测试说明图展示正确性测试、三类操作序列和性能实验规模。

![测试 Case 与验证流程](project_diagrams/02_test_cases.png)

两张图均在 `project_diagrams/` 中提供 PNG 和 SVG，可用于小组讨论与报告。
