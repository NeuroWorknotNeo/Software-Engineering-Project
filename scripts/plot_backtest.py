#!/usr/bin/env python3
"""Строит графики по результатам `mpt backtest`: кривые капитала и просадки.

Нужен matplotlib: pip install matplotlib

Пример:
    ./build/mpt backtest --prices data/prices.csv --splits data/splits.csv --out equity.csv
    python3 scripts/plot_backtest.py equity.csv --out backtest.png --log
"""

import argparse
import csv
import sys
from datetime import date
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

PERCENT = 100.0


def read_equity(path):
    """Читает CSV кривых капитала: (даты, {стратегия: [стоимость]})."""
    with Path(path).open(newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        strategies = header[1:]
        dates, curves = [], {name: [] for name in strategies}
        for row in reader:
            dates.append(date.fromisoformat(row[0]))
            for name, value in zip(strategies, row[1:]):
                curves[name].append(float(value))
    return dates, curves


def compute_drawdown(values):
    """Просадка от предыдущего максимума в каждый день (доля, ≤ 0)."""
    peak, drawdowns = 1.0, []
    for value in values:
        peak = max(peak, value)
        drawdowns.append(value / peak - 1.0)
    return drawdowns


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("equity", type=Path, help="CSV от mpt backtest --out")
    parser.add_argument("--out", type=Path, default=Path("backtest.png"))
    parser.add_argument("--exclude", nargs="*", default=[],
                        help="стратегии, которые не нужно рисовать")
    parser.add_argument("--log", action="store_true", help="логарифмическая шкала стоимости")
    args = parser.parse_args(argv)

    dates, curves = read_equity(args.equity)
    curves = {name: values for name, values in curves.items() if name not in args.exclude}

    figure, (top, bottom) = plt.subplots(2, 1, figsize=(10, 8), sharex=True,
                                         gridspec_kw={"height_ratios": [2, 1]})
    for name, values in curves.items():
        top.plot(dates, values, label=name)
        bottom.plot(dates, [d * PERCENT for d in compute_drawdown(values)], label=name)
    if args.log:
        top.set_yscale("log")
    top.set_ylabel("Стоимость портфеля (начало = 1)")
    top.set_title("Бэктест: ежемесячная ребалансировка, окно оценки 252 дня")
    top.grid(alpha=0.3)
    top.legend()
    bottom.set_ylabel("Просадка, %")
    bottom.grid(alpha=0.3)
    figure.tight_layout()
    figure.savefig(args.out, dpi=150)
    print(f"Saved {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
