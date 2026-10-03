"""Plot measured medians and min/max ranges; never fabricate benchmark values."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
from statistics import median
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("results"))
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with args.csv.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise SystemExit("Input CSV contains no measurements")
    names = ["BST", "AVL", "Splay", "RedBlack", "BPlus"]
    scenarios = ["ascending_ascending", "ascending_descending", "random_random"]
    metrics = ["insert_seconds", "delete_seconds", "total_seconds"]
    groups = defaultdict(list)
    for r in rows:
        for metric in metrics:
            value = float(r[metric])
            if value <= 0:
                raise SystemExit("Nonpositive measurement cannot be shown on a log axis")
            groups[r["scenario"], r["tree"], int(r["n"]), metric].append(value)
    with (args.output_dir / "summary.csv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.writer(out)
        writer.writerow(["scenario", "tree", "n", "metric", "samples", "median_seconds", "min_seconds", "max_seconds"])
        for key, values in sorted(groups.items()):
            writer.writerow([*key, len(values), median(values), min(values), max(values)])
    # Shared colors make each implementation identifiable in all nine panels.
    colors = dict(zip(names, plt.get_cmap("tab10").colors[:5]))
    fig, axes = plt.subplots(3, 3, figsize=(16, 12))
    fig.subplots_adjust(left=0.07, right=0.985, bottom=0.055, top=0.90, wspace=0.28, hspace=0.40)
    for row, scenario in enumerate(scenarios):
        for col, metric in enumerate(metrics):
            ax = axes[row, col]
            for name in names:
                xs = sorted(k[2] for k in groups if k[0] == scenario and k[1] == name and k[3] == metric)
                if not xs:
                    raise SystemExit(f"Missing data for {scenario}/{name}/{metric}")
                samples = [groups[scenario, name, n, metric] for n in xs]
                ax.plot(xs, [median(v) for v in samples], "o-", label=name, color=colors[name], markersize=4)
                ax.fill_between(xs, [min(v) for v in samples], [max(v) for v in samples], color=colors[name], alpha=0.12)
            ax.set(xscale="log", yscale="log", xlabel="N (distinct keys)", ylabel="Elapsed wall time (seconds)",
                   title=f'{scenario.replace("_", " ")}\n{metric.replace("_seconds", "")}')
            ax.grid(True, which="both", alpha=0.2)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(0.5, 0.955), ncol=5)
    fig.suptitle("Search trees: median runtime; shaded range = min/max of repetitions", y=0.985)
    for suffix in ("png", "svg"):
        fig.savefig(args.output_dir / f"runtime_comparison.{suffix}", dpi=180, bbox_inches="tight")
    plt.close(fig)
    print(f"Wrote summary.csv and runtime_comparison.png/.svg to {args.output_dir}")


if __name__ == "__main__":
    main()
