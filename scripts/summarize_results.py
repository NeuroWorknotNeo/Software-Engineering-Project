#!/usr/bin/env python3
"""Сводит метрики всех вариантов бэктеста в одну таблицу и один график.

Читает каталоги `<results>/NN_<вариант>/` (их создаёт scripts/run_experiments.sh),
пишет `<results>/summary_all_variants.csv` и `<results>/summary_variants.png`.

Нужен matplotlib: pip install matplotlib

Пример:
    python3 scripts/summarize_results.py results
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

PERCENT = 100.0
BAR_WIDTH = 0.2
# Разорившаяся стратегия сжимает масштаб графика, поэтому в него не попадает.
PLOTTED_STRATEGIES = [
    "min-variance/short",
    "min-variance/long-only",
    "max-sharpe/long-only",
    "equal-weight",
]
PLOTTED_METRICS = [("annual_return", "CAGR, %"), ("max_drawdown", "Макс. просадка, %")]


def read_period(equity_path):
    """Первая и последняя дата кривой капитала."""
    with equity_path.open(newline="") as f:
        dates = [row["date"] for row in csv.DictReader(f)]
    return dates[0], dates[-1]


def collect_rows(results_dir):
    """Строки сводной таблицы: вариант, период и метрики каждой стратегии."""
    rows = []
    for variant_dir in sorted(results_dir.glob("[0-9][0-9]_*")):
        first_date, last_date = read_period(variant_dir / "equity.csv")
        with (variant_dir / "metrics.csv").open(newline="") as f:
            for metrics in csv.DictReader(f):
                rows.append({"variant": variant_dir.name, "first_date": first_date,
                             "last_date": last_date, **metrics})
    return rows


def write_summary_csv(rows, path):
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def plot_summary(rows, path):
    variants = sorted({row["variant"] for row in rows})
    values = {(row["variant"], row["strategy"]): row for row in rows}
    figure, axes = plt.subplots(1, len(PLOTTED_METRICS), figsize=(13, 5))
    for axis, (metric, title) in zip(axes, PLOTTED_METRICS):
        for index, strategy in enumerate(PLOTTED_STRATEGIES):
            offset = (index - (len(PLOTTED_STRATEGIES) - 1) / 2) * BAR_WIDTH
            heights = [float(values[(variant, strategy)][metric]) * PERCENT
                       for variant in variants]
            positions = [position + offset for position in range(len(variants))]
            axis.bar(positions, heights, BAR_WIDTH, label=strategy)
        axis.set_xticks(range(len(variants)))
        axis.set_xticklabels([variant.split("_", 1)[1] for variant in variants],
                             rotation=30, ha="right")
        axis.set_title(title)
        axis.grid(axis="y", alpha=0.3)
        axis.axhline(0, color="black", linewidth=0.8)
    axes[0].legend(fontsize=8)
    figure.suptitle("Чувствительность результатов бэктеста "
                    "(max-sharpe/short не показан — разорение во всех вариантах)")
    figure.tight_layout()
    figure.savefig(path, dpi=150)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("results_dir", type=Path, help="каталог с результатами")
    args = parser.parse_args(argv)

    rows = collect_rows(args.results_dir)
    if not rows:
        print(f"error: no backtest variants found in {args.results_dir}", file=sys.stderr)
        return 1
    write_summary_csv(rows, args.results_dir / "summary_all_variants.csv")
    plot_summary(rows, args.results_dir / "summary_variants.png")
    print(f"Summary written to {args.results_dir}/summary_all_variants.csv and "
          f"summary_variants.png", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
