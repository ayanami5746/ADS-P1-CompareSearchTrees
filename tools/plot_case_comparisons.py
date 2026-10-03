"""Create restrained, report-ready charts from measured search-tree timings."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from statistics import median
import shutil

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, FuncFormatter, NullLocator

TREES = ['BST', 'AVL', 'Splay', 'RedBlack', 'BPlus']
CASES = [
    ('ascending_ascending', 'case1', 'Ascending insert / ascending delete'),
    ('ascending_descending', 'case2', 'Ascending insert / descending delete'),
    ('random_random', 'case3', 'Random insert / random delete'),
]
METRICS = [('insert_seconds', 'insert', 'Insertion'),
           ('delete_seconds', 'delete', 'Deletion'),
           ('total_seconds', 'total', 'Insertion + deletion')]
COLORS = ['#303030', '#406B91', '#49836A', '#9A6850', '#796C91']
MARKERS = ['o', 's', '^', 'D', 'v']
STYLES = ['-', '--', '-.', ':', '-']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--output-dir', type=Path, default=Path('performance_charts'))
    args = parser.parse_args()
    with args.csv.open(encoding='utf-8', newline='') as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit('The input contains no measurements.')
    groups = defaultdict(list)
    seen = set()
    for row in rows:
        key = (row['scenario'], row['tree'], int(row['n']))
        identity = (*key, int(row['repeat']))
        if identity in seen:
            raise SystemExit(f'Duplicate measurement: {identity}')
        seen.add(identity)
        if key[0] not in {c[0] for c in CASES} or key[1] not in TREES or key[2] <= 0:
            raise SystemExit(f'Unexpected measurement: {key}')
        for metric, _, _ in METRICS:
            value = float(row[metric])
            if not math.isfinite(value) or value <= 0:
                raise SystemExit('Logarithmic plots require finite positive timings.')
            groups[*key, metric].append(value)
    sizes = sorted({int(row['n']) for row in rows})
    sample_counts = set()
    for scenario, _, _ in CASES:
        for tree in TREES:
            for n in sizes:
                for metric, _, _ in METRICS:
                    values = groups[scenario, tree, n, metric]
                    if not values:
                        raise SystemExit(f'Missing measurements: {scenario}/{tree}/{n}')
                    sample_counts.add(len(values))
    if len(sample_counts) != 1:
        raise SystemExit('All groups must contain the same number of repetitions.')
    repeats = sample_counts.pop()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10,
                         'axes.spines.top': False, 'axes.spines.right': False,
                         'axes.titleweight': 'normal', 'svg.fonttype': 'none'})

    def panel(ax, scenario, metric, title):
        for i, tree in enumerate(TREES):
            ys = [median(groups[scenario, tree, n, metric]) for n in sizes]
            ax.plot(sizes, ys, color=COLORS[i], marker=MARKERS[i], linestyle=STYLES[i],
                    linewidth=1.45, markersize=4, label=tree)
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.xaxis.set_major_locator(FixedLocator(sizes))
        ax.xaxis.set_major_formatter(FuncFormatter(lambda n, _: f'{n / 1000:g}k'))
        ax.xaxis.set_minor_locator(NullLocator())
        ax.set_xlabel('Number of keys N (log scale)')
        ax.set_ylabel('Time (s, log scale)')
        ax.set_title(title, fontsize=11, pad=12)
        ax.grid(axis='y', which='major', color='#e3e3e3', linewidth=0.6)
        ax.set_axisbelow(True)

    def save(fig, name):
        fig.savefig(args.output_dir / f'{name}.png', dpi=240, facecolor='white')
        fig.savefig(args.output_dir / f'{name}.svg', facecolor='white')
        plt.close(fig)

    for scenario, case, description in CASES:
        for metric, short, title in METRICS:
            fig, ax = plt.subplots(figsize=(7.6, 5.2))
            fig.subplots_adjust(left=0.12, right=0.97, bottom=0.16, top=0.75)
            panel(ax, scenario, metric, title)
            fig.suptitle(f'{case.replace("case", "Case ")}: {description}', y=0.97, fontsize=12)
            handles, labels = ax.get_legend_handles_labels()
            fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.91),
                       ncol=5, frameon=False, fontsize=9)
            fig.text(0.12, 0.035, f'Median of {repeats} runs; measured data, no fitted curves.',
                     fontsize=8, color='#666666')
            save(fig, f'{case}_{short}')

    fig, axes = plt.subplots(3, 3, figsize=(15, 12))
    fig.subplots_adjust(left=0.075, right=0.985, bottom=0.075, top=0.865,
                        wspace=0.34, hspace=0.52)
    for row, (scenario, case, description) in enumerate(CASES):
        for col, (metric, _, title) in enumerate(METRICS):
            panel(axes[row, col], scenario, metric, f'{case.replace("case", "Case ")} - {title}')
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.suptitle('Search-tree performance comparison', y=0.982, fontsize=16)
    fig.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, 0.955), ncol=5, frameon=False)
    fig.text(0.5, 0.91, 'Case 1: ascending / ascending     Case 2: ascending / descending     Case 3: random / random',
             ha='center', fontsize=10)
    fig.text(0.075, 0.018, f'Median of {repeats} runs | Both axes logarithmic | Each panel uses its own time range',
             fontsize=9, color='#666666')
    save(fig, 'overview')

    with (args.output_dir / 'chart_values.csv').open('w', encoding='utf-8', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(['scenario', 'tree', 'n', 'metric', 'runs', 'median_seconds', 'min_seconds', 'max_seconds'])
        for key, values in sorted(groups.items()):
            writer.writerow([*key, len(values), median(values), min(values), max(values)])
    destination = args.output_dir / 'source_data.csv'
    if destination.resolve() != args.csv.resolve():
        shutil.copyfile(args.csv, destination)
    print(f'Generated 9 individual charts and 1 overview, each as PNG and SVG; {len(rows)} source records.')


if __name__ == '__main__':
    main()
