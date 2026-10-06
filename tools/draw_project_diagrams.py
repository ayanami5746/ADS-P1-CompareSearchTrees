"""Render the code and test diagrams in blue, white, and gray."""
import argparse
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

BLUE, DARK, GRAY = '#245B88', '#22364A', '#617080'
LINE, PALE, WHITE = '#CED8E1', '#EDF4FA', '#FFFFFF'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=Path('project_diagrams'))
    parser.add_argument('--font', type=Path, help='Optional font file')
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    font = (font_manager.FontProperties(fname=str(args.font)) if args.font
            else font_manager.FontProperties(family='DejaVu Sans'))
    plt.rcParams['svg.fonttype'] = 'path'
    checks = []

    def text(ax, x, y, value, size=12, color=DARK):
        return ax.text(x, y, value, fontproperties=font, fontsize=size,
                       color=color, va='top', linespacing=1.5)

    def canvas(title, subtitle):
        checks.clear()
        fig, ax = plt.subplots(figsize=(16, 11.2))
        fig.subplots_adjust(left=0, right=1, top=1, bottom=0)
        ax.set(xlim=(0, 1600), ylim=(1120, 0))
        ax.axis('off')
        ax.plot([60, 1540], [113, 113], color=LINE, linewidth=1)
        text(ax, 60, 38, title, 25, BLUE)
        text(ax, 60, 84, subtitle, 12, GRAY)
        return fig, ax

    def box(ax, x, y, w, h, title, body, fill=WHITE, body_size=11.5):
        ax.add_patch(FancyBboxPatch((x, y), w, h,
                     boxstyle='round,pad=0,rounding_size=7', linewidth=1,
                     edgecolor=LINE, facecolor=fill))
        title_artist = text(ax, x+20, y+16, title, 14, BLUE)
        body_artist = text(ax, x+20, y+51, body, body_size)
        checks.extend([(title_artist, (x, y, w, h)), (body_artist, (x, y, w, h))])

    def arrow(ax, start, end):
        ax.add_patch(FancyArrowPatch(start, end, arrowstyle='-|>',
                     mutation_scale=12, linewidth=1.15, color=BLUE))

    def save(fig, stem):
        fig.canvas.draw()
        renderer = fig.canvas.get_renderer()
        for artist, (x, y, w, h) in checks:
            bounds = artist.get_window_extent(renderer).transformed(artist.axes.transData.inverted())
            if bounds.x1 > x+w-10 or max(bounds.y0, bounds.y1) > y+h-8:
                raise ValueError(f'Text exceeds its box: {artist.get_text()}')
        fig.savefig(args.output_dir / f'{stem}.png', dpi=200, facecolor=WHITE)
        fig.savefig(args.output_dir / f'{stem}.svg', facecolor=WHITE)
        plt.close(fig)

    fig, ax = canvas('Code structure', 'Five search trees behind one C interface; tests and benchmarks have separate entry points.')
    box(ax, 60, 140, 720, 125, 'Correctness tests',
        'test_trees.c: compare operations with a reference set.\n'
        'test_lecture.c: check tree shapes, colors, and rotation limits.', PALE)
    box(ax, 820, 140, 720, 125, 'Performance experiment  |  src/benchmark.c',
        'Give each tree the same input; time insertion and deletion.\n'
        'Validate outside the timed regions and write one row per trial.', PALE)
    arrow(ax, (420, 265), (420, 302))
    arrow(ax, (1180, 265), (1180, 302))
    box(ax, 60, 305, 1480, 127, 'Public interface  |  include/trees.h',
        'tree_create  /  tree_insert  /  tree_delete  /  tree_contains  /  tree_size  /  tree_validate  /  tree_destroy\n'
        'src/trees.c selects the algorithm by TreeKind and changes size once per successful update.', PALE)
    arrow(ax, (800, 432), (800, 462))
    text(ax, 60, 463, 'Algorithms  |  src/trees.c', 15, BLUE)
    trees = [
        ('BST', 'Iterative updates\nSuccessor replacement\nNo balancing'),
        ('AVL', 'Cached heights\nSingle / double rotations\nRepair on the way up'),
        ('Splay', 'Bottom-up splaying\nZig / zig-zig / zig-zag\nDelete, then join subtrees'),
        ('Red-black', 'Ordinary red-black rules\nUncle / sibling cases\nRecolor and rotate'),
        ('B+', 'Order 4; linked leaves\nSplit / borrow / merge\nRecords only in leaves'),
    ]
    for i, (name, body) in enumerate(trees):
        box(ax, 60+i*300, 505, 280, 160, name, body, body_size=10.5)
    box(ax, 60, 693, 470, 137, 'Data structures',
        'Node: binary-tree key and links\nPage: B+ storage; Tree: kind, roots, size', '#F6F8FA')
    box(ax, 550, 693, 470, 137, 'Shared helpers',
        'Memory allocation and binary lookup\nRotations preserve key order and links.', '#F6F8FA')
    box(ax, 1040, 693, 500, 137, 'Validation and cleanup',
        'tree_validate: check each tree\'s invariants\ntree_destroy: release all nodes or pages', '#F6F8FA')
    text(ax, 60, 855, 'Outputs and reproduction', 15, BLUE)
    box(ax, 60, 892, 720, 117, 'Test results',
        'PASS / FAIL logs and structural checks\nLocal build scripts and GitHub Actions run the checks.')
    box(ax, 820, 892, 720, 117, 'Measurements and figures',
        'benchmark.csv -> plotting tools -> PNG / SVG\nresults/ holds the data; performance_charts/ holds comparisons.')
    text(ax, 60, 1044, 'Reading order: public interface -> dispatch -> tree algorithm -> validation and cleanup.', 12, GRAY)
    save(fig, '01_code_structure')

    fig, ax = canvas('Test cases and validation', 'Set behavior, structural regression checks, and the three assignment workloads.')
    text(ax, 60, 136, 'A  Correctness checks', 16, BLUE)
    cards = [
        ('Boundaries and lifecycle', 'Empty trees, duplicates, signed integer limits\n'
         'Clear and reuse the same tree; destroy 20,000 stored keys\nNULL cleanup and invalid tree-kind handling'),
        ('Exhaustive small orders', 'Keys: {0, 1, 2, 3, 4}\n'
         '120 insertion orders x 120 deletion orders per tree\nValidate every update; includes the first B+ leaf split'),
        ('Random mixed operations', 'Insert / delete / find; keys from -1024 to 1023\n'
         'Four seeds x 30,000 steps per tree; boolean-array reference\nCheck each step and scan the full range periodically'),
        ('Structural regression checks', 'Splay: all rotation shapes and access-to-root behavior\n'
         'Red-black: fixed insertion case, all repair cases, rotation limits\nB+: capacity, split, borrow, merge, and root collapse'),
    ]
    for i, (title, body) in enumerate(cards):
        box(ax, 60+(i%2)*750, 180+(i//2)*174, 730, 154, title, body, PALE, body_size=11)
    text(ax, 60, 547, 'B  Assignment workloads  |  Used for correctness and performance', 16, BLUE)
    cases = [
        ('Case 1', 'Ascending insert / ascending delete\nInsert: 0 1 2 3 4\nDelete: 0 1 2 3 4'),
        ('Case 2', 'Ascending insert / descending delete\nInsert: 0 1 2 3 4\nDelete: 4 3 2 1 0'),
        ('Case 3', 'Random insert / random delete\nShuffle the same set separately.\nFixed seeds reproduce each sequence.'),
    ]
    for i, (title, body) in enumerate(cases):
        box(ax, 60+i*500, 588, 480, 150, title, body, body_size=11)
    text(ax, 60, 755, 'Correctness size: N = 5,000 normally, or 100,000 with --stress. Insert all keys before deleting any.', 11.5, GRAY)
    text(ax, 60, 797, 'C  Performance experiment', 16, BLUE)
    box(ax, 60, 837, 1480, 120, '5 trees x 3 cases x 5 sizes x 3 repetitions = 225 measurements',
        'N = 1,000 / 3,000 / 10,000 / 30,000 / 100,000. Each tree receives the same input within a trial.\n'
        'Record insertion, deletion, and total time. Input generation, validation, and file output are outside timing.', PALE)
    box(ax, 60, 980, 1480, 100, 'Structural checks',
        'Key order and count; AVL heights; Splay / RB parent links; RB colors and black height; B+ occupancy, separators, and leaf links.',
        '#F6F8FA', body_size=10.5)
    save(fig, '02_test_cases')
    print('Created two diagrams, each in PNG and SVG format.')


if __name__ == '__main__':
    main()
