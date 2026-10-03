"""Render two blue/white/gray project diagrams as PNG and portable SVG."""
import argparse
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

BLUE = '#245B88'
DARK = '#22364A'
GRAY = '#617080'
LINE = '#CED8E1'
PALE = '#EDF4FA'
WHITE = '#FFFFFF'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path('project_diagrams'))
    parser.add_argument('--font', type=Path, default=Path('C:/Windows/Fonts/msyh.ttc'))
    args = parser.parse_args()
    if not args.font.exists():
        raise SystemExit('Supply a Chinese-capable font with --font PATH.')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    font = font_manager.FontProperties(fname=str(args.font))
    plt.rcParams['svg.fonttype'] = 'path'

    def canvas(title, subtitle):
        fig, ax = plt.subplots(figsize=(16, 11.2))
        fig.subplots_adjust(left=0, right=1, top=1, bottom=0)
        ax.set(xlim=(0, 1600), ylim=(1120, 0))
        ax.axis('off')
        ax.plot([60, 1540], [113, 113], color=LINE, linewidth=1)
        text(ax, 60, 38, title, 25, BLUE)
        text(ax, 60, 84, subtitle, 12, GRAY)
        return fig, ax

    def text(ax, x, y, value, size=13, color=DARK, ha='left'):
        return ax.text(x, y, value, fontproperties=font, fontsize=size, color=color,
                       ha=ha, va='top', linespacing=1.6)

    def box(ax, x, y, w, h, title, body='', fill=WHITE, title_size=14, body_size=12):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0,rounding_size=7',
                                   linewidth=1, edgecolor=LINE, facecolor=fill))
        text(ax, x+20, y+16, title, title_size, BLUE)
        if body:
            text(ax, x+20, y+51, body, body_size)

    def arrow(ax, start, end):
        ax.add_patch(FancyArrowPatch(start, end, arrowstyle='-|>', mutation_scale=12,
                                     linewidth=1.15, color=BLUE))

    def save(fig, stem):
        fig.savefig(args.output_dir / f'{stem}.png', dpi=200, facecolor=WHITE)
        fig.savefig(args.output_dir / f'{stem}.svg', facecolor=WHITE)
        plt.close(fig)

    fig, ax = canvas('代码整体结构', '统一接口连接五种搜索树；正确性测试与性能实验分别提供 main() 入口')
    box(ax, 60, 140, 720, 125, '正确性测试入口  ·  tests/test_trees.c',
        '生成测试序列 → 调用树接口 → 对照预期结果\n检查返回值、元素数量与结构不变量', PALE)
    box(ax, 820, 140, 720, 125, '性能实验入口  ·  src/benchmark.c',
        '生成相同输入 → 分别计时插入 / 删除\n验证在计时区间之外执行，输出每轮实测记录', PALE)
    arrow(ax, (420, 265), (420, 302))
    arrow(ax, (1180, 265), (1180, 302))
    box(ax, 60, 305, 1480, 127, '统一接口  ·  include/trees.h',
        'tree_create  /  tree_insert  /  tree_delete  /  tree_contains  /  tree_size  /  tree_validate  /  tree_destroy\n实现位于 src/trees.c：按 TreeKind 分派操作，插入 / 删除成功后统一更新 size。', PALE, body_size=12)
    arrow(ax, (800, 432), (800, 462))
    text(ax, 60, 463, '算法实现层  ·  src/trees.c', 15, BLUE)
    trees = [
        ('BST', '迭代查找与更新\n双孩子删除使用后继\n不主动调整平衡'),
        ('AVL', '维护节点高度\n单旋 / 双旋修复失衡\n更新后沿路径回溯'),
        ('Splay', '自顶向下伸展\n将访问位置移到根\n删除后拼接左右子树'),
        ('RedBlack', '左倾红黑树 LLRB\n旋转与颜色翻转\n维护黑高与红链接约束'),
        ('BPlus', '16 阶多路树\n分裂、借位与合并\n记录在叶子，叶子成链'),
    ]
    for i, (name, body) in enumerate(trees):
        box(ax, 60+i*300, 505, 280, 160, name, body)
    box(ax, 60, 693, 470, 137, '基础数据结构',
        'Node：四种二叉树共用节点\nPage：B+ 页；Tree：类型、根、数量', '#F6F8FA')
    box(ax, 550, 693, 470, 137, '公共基础操作',
        '内存分配、左右旋转、二叉查找\n算法辅助函数保持文件内 static', '#F6F8FA')
    box(ax, 1040, 693, 500, 137, '验证与资源释放',
        'tree_validate：检查各树的结构约束\ntree_destroy：释放全部节点或页', '#F6F8FA')
    text(ax, 60, 855, '运行产物与复现', 15, BLUE)
    box(ax, 60, 892, 720, 117, '测试结果',
        'PASS / FAIL 与验证记录\n本地构建脚本及 GitHub Actions 执行检查')
    box(ax, 820, 892, 720, 117, '实验数据与图表',
        'benchmark.csv → 绘图脚本 → PNG / SVG\nresults/ 保存实验记录；performance_charts/ 保存对比图')
    text(ax, 60, 1044, '阅读顺序：公共接口 → 按树类型分派 → 对应算法 → 结构验证与内存清理', 12, GRAY)
    save(fig, '01_code_structure')

    fig, ax = canvas('测试 Case 与验证流程', '五种树运行相同的测试套件；正确性验证与性能计时分别进行')
    text(ax, 60, 136, 'A  正确性测试  ·  tests/test_trees.c', 16, BLUE)
    cards = [
        ('01  边界与集合语义', '输入：0、INT_MIN、INT_MAX、±1、±100\n空树查询 / 删除；重复插入 / 删除\n清空后再插入 17，确认可以复用'),
        ('02  小规模排列穷举', '键集合：{0, 1, 2, 3, 4}\n120 种插入 × 120 种删除 = 14,400 对 / 树\n每次更新后检查结构；覆盖多种操作顺序'),
        ('03  随机混合交叉验证', '键范围：−1024～1023；插入 / 删除 / 查询\n4 个种子 × 30,000 步 = 120,000 次 / 树\n对照布尔数组；每步查结构，定期查全域'),
        ('04  非空销毁与特殊接口', '递增插入 20,000 个键后直接销毁\n检查长链清理；允许 tree_destroy(NULL)\n无效树类型创建时返回 NULL'),
    ]
    for i, (title, body) in enumerate(cards):
        box(ax, 60+(i%2)*750, 180+(i//2)*174, 730, 154, title, body, PALE, body_size=11.5)
    text(ax, 60, 547, 'B  题目要求的三种序列  ·  正确性测试与性能实验均覆盖', 16, BLUE)
    cases = [
        ('Case 1', '递增插入 → 递增删除\n示例：插 0 1 2 3 4\n           删 0 1 2 3 4'),
        ('Case 2', '递增插入 → 递减删除\n示例：插 0 1 2 3 4\n           删 4 3 2 1 0'),
        ('Case 3', '随机插入 → 随机删除\n同一集合，分别打乱两种顺序\n固定种子，可重复生成'),
    ]
    for i, (title, body) in enumerate(cases):
        box(ax, 60+i*500, 588, 480, 150, title, body, body_size=12)
    text(ax, 60, 755, '正确性规模：普通 N = 5,000；--stress 时 N = 100,000。全部插入后、删除过程中及清空后检查结构。', 11.5, GRAY)
    text(ax, 60, 797, 'C  性能实验  ·  src/benchmark.c', 16, BLUE)
    box(ax, 60, 837, 1480, 120, '5 种树 × 3 类 Case × 5 种规模 × 3 次重复 = 225 条实测记录',
        'N = 1,000 / 3,000 / 10,000 / 30,000 / 100,000；同轮所有树使用相同输入。\n分别记录插入、删除和总耗时；输入生成、结构验证和文件输出不计入计时。', PALE, body_size=12)
    box(ax, 60, 980, 1480, 100, '共同检查标准',
        '键序与数量；AVL 高度和平衡；红黑树颜色与黑高；B+ 占用率、分隔键、叶子等深及叶子链。', '#F6F8FA', body_size=11.5)
    save(fig, '02_test_cases')
    print('Created two diagrams, each in PNG and SVG format.')


if __name__ == '__main__':
    main()
