#!/usr/bin/env bash
# Пересчитывает все результаты для отчёта: бэктесты с разными параметрами,
# эффективные границы, оптимальные портфели, графики и сводную таблицу.
#
# Использование (из любого каталога):
#   scripts/run_experiments.sh            # результаты в results/
#   scripts/run_experiments.sh my_results # в другой каталог
#
# Нужны: CMake, компилятор C++20, Eigen 3, Python 3 с matplotlib.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
output_dir="${1:-results}"

if ! python3 -c "import matplotlib" 2>/dev/null; then
    echo "error: matplotlib is required for plots: pip install matplotlib" >&2
    exit 1
fi

# Отдельный каталог сборки, чтобы не менять настройки основного build/ (там нужны тесты).
build_dir="build/experiments"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DMPT_BUILD_TESTS=OFF \
    -DMPT_BUILD_BENCHMARKS=OFF > /dev/null
cmake --build "$build_dir" -j > /dev/null
mpt="$build_dir/mpt"
data_options=(--prices data/prices.csv --splits data/splits.csv)

rm -rf "$output_dir"
mkdir -p "$output_dir"

# Бэктест одного варианта параметров: CSV, текстовая сводка и график.
run_backtest() {
    local name="$1"
    shift
    local dir="$output_dir/$name"
    mkdir -p "$dir"
    "$mpt" backtest "${data_options[@]}" "$@" --out "$dir/equity.csv" \
        --weights-out "$dir/weights.csv" --metrics-out "$dir/metrics.csv" > "$dir/summary.txt"
    # Стратегия max-sharpe/short разоряется и сжимает масштаб графика, поэтому не рисуется.
    python3 scripts/plot_backtest.py "$dir/equity.csv" --exclude max-sharpe/short \
        --out "$dir/backtest.png" 2> /dev/null
    echo "backtest $name: done"
}

# Оптимальные портфели и эффективная граница для одной оценки ковариации.
run_frontier() {
    local covariance="$1"
    local dir="$output_dir/frontier_$covariance"
    mkdir -p "$dir"
    "$mpt" optimize "${data_options[@]}" --cov "$covariance" --out "$dir/portfolios.csv" \
        > "$dir/optimize.txt"
    "$mpt" frontier "${data_options[@]}" --cov "$covariance" --out "$dir/frontier.csv" \
        > "$dir/frontier.txt"
    python3 scripts/plot_frontier.py "$dir/frontier.csv" --portfolios "$dir/portfolios.csv" \
        --out "$dir/frontier.png" 2> /dev/null
    echo "frontier $covariance: done"
}

"$mpt" stats "${data_options[@]}" --cov ledoit-wolf \
    --out "$output_dir/covariance_ledoit_wolf.csv" > "$output_dir/stats.txt"

run_frontier sample
run_frontier ledoit-wolf

run_backtest 01_baseline
run_backtest 02_ledoit_wolf --cov ledoit-wolf
run_backtest 03_cost_0.1pct --cost 0.001
run_backtest 04_riskfree_5pct --risk-free 0.05
run_backtest 05_riskfree_15pct --risk-free 0.15
run_backtest 06_window_126 --window 126
run_backtest 07_window_504 --window 504

python3 scripts/summarize_results.py "$output_dir"
echo "All results written to $output_dir/"
