#!/usr/bin/env python3
"""Строит график эффективной границы по CSV, который пишет `mpt frontier --out`.

На график наносятся границы с короткими продажами и без них, а также (если передан
CSV от `mpt optimize --out`) портфели минимальной дисперсии и максимального Шарпа.

Нужен matplotlib: pip install matplotlib

Пример:
    ./build/mpt frontier --prices data/prices.csv --splits data/splits.csv --out frontier.csv
    ./build/mpt optimize --prices data/prices.csv --splits data/splits.csv --out portfolios.csv
    python3 scripts/plot_frontier.py frontier.csv --portfolios portfolios.csv --out frontier.png
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

PERCENT = 100.0
MODE_LABELS = {"short": "с короткими продажами", "long-only": "без коротких продаж"}
NAME_LABELS = {"min-variance": "мин. дисперсия", "max-sharpe": "макс. Шарп"}


def read_portfolios(path):
    """Читает CSV портфелей: {mode: [(волатильность, доходность, имя)]}."""
    by_mode = {}
    with Path(path).open(newline="") as f:
        for row in csv.DictReader(f):
            point = (float(row["volatility"]), float(row["expected_return"]), row["name"])
            by_mode.setdefault(row["mode"], []).append(point)
    return by_mode


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("frontier", type=Path, help="CSV от mpt frontier --out")
    parser.add_argument("--portfolios", type=Path, help="CSV от mpt optimize --out")
    parser.add_argument("--out", type=Path, default=Path("frontier.png"))
    parser.add_argument("--max-volatility", type=float, default=0.6,
                        help="правая граница оси риска (доля), чтобы экстремальные точки "
                             "не сжимали график")
    args = parser.parse_args(argv)

    figure, axes = plt.subplots(figsize=(9, 6))
    for mode, points in read_portfolios(args.frontier).items():
        volatilities = [point[0] * PERCENT for point in points]
        returns = [point[1] * PERCENT for point in points]
        axes.plot(volatilities, returns, marker=".", label="Граница " + MODE_LABELS.get(mode, mode))

    if args.portfolios:
        for mode, points in read_portfolios(args.portfolios).items():
            for volatility, expected_return, name in points:
                if volatility > args.max_volatility:
                    continue
                axes.scatter(volatility * PERCENT, expected_return * PERCENT, s=60, zorder=3)
                axes.annotate(f"{NAME_LABELS.get(name, name)}, {MODE_LABELS.get(mode, mode)}",
                              (volatility * PERCENT, expected_return * PERCENT),
                              textcoords="offset points", xytext=(6, -12), fontsize=8)

    axes.set_xlim(left=0, right=args.max_volatility * PERCENT)
    axes.set_xlabel("Волатильность, % годовых")
    axes.set_ylabel("Ожидаемая доходность, % годовых")
    axes.set_title("Эффективная граница Марковица")
    axes.grid(alpha=0.3)
    axes.legend()
    figure.tight_layout()
    figure.savefig(args.out, dpi=150)
    print(f"Saved {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
