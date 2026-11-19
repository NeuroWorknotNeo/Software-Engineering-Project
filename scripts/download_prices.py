#!/usr/bin/env python3
"""Выгрузка дневных цен закрытия акций с Московской биржи (MOEX ISS) в CSV.

Результат — широкий CSV ``date,TICKER1,TICKER2,...``, который читает ``mpt``.
Пустая ячейка означает, что в этот день бумага не торговалась.

Нужен только стандартный Python 3.8+, без сторонних библиотек.

Пример:
    python3 scripts/download_prices.py --out data/prices.csv
    python3 scripts/download_prices.py --tickers SBER GAZP --from 2023-01-01 --till 2023-12-31
"""

import argparse
import csv
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ISS_URL = ("https://iss.moex.com/iss/history/engines/stock/markets/shares/"
           "boards/{board}/securities/{ticker}.json")

DEFAULT_TICKERS = [
    "SBER", "GAZP", "LKOH", "ROSN", "NVTK", "TATN", "SNGS", "GMKN", "PLZL", "CHMF",
    "NLMK", "MAGN", "ALRS", "PHOR", "MTSS", "RTKM", "MGNT", "MOEX", "AFLT", "IRAO",
]


def fetch_json(url, retries=3, timeout=30):
    """Скачивает JSON, повторяя запрос при сетевых ошибках."""
    for attempt in range(1, retries + 1):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as response:
                return json.load(response)
        except OSError as error:
            if attempt == retries:
                raise
            print(f"  retry {attempt}/{retries - 1} after error: {error}", file=sys.stderr)
            time.sleep(2 * attempt)
    raise AssertionError("unreachable")


def parse_history_page(payload):
    """Разбирает одну страницу ответа ISS.

    Возвращает (строки, размер страницы, total): строки — [(дата, цена закрытия)],
    размер страницы — число строк в ответе с учётом пропущенных,
    total — общее число строк из блока history.cursor или None, если блока нет.
    Строки без цены закрытия (дни без сделок) пропускаются.
    """
    history = payload["history"]
    columns = history["columns"]
    date_col = columns.index("TRADEDATE")
    close_col = columns.index("CLOSE")
    rows = [(row[date_col], float(row[close_col]))
            for row in history["data"] if row[close_col] is not None]

    total = None
    cursor = payload.get("history.cursor")
    if cursor and cursor.get("data"):
        total = int(cursor["data"][0][cursor["columns"].index("TOTAL")])
    return rows, len(history["data"]), total


def download_ticker(ticker, date_from, date_till, board, fetch=fetch_json):
    """Скачивает все страницы истории одной бумаги. Возвращает {дата: цена}."""
    prices = {}
    start = 0
    while True:
        query = urllib.parse.urlencode({
            "from": date_from,
            "till": date_till,
            "start": start,
            "iss.meta": "off",
            "history.columns": "TRADEDATE,CLOSE",
        })
        url = ISS_URL.format(board=board, ticker=ticker) + "?" + query
        rows, page_size, total = parse_history_page(fetch(url))
        prices.update(rows)
        start += page_size
        if page_size == 0 or (total is not None and start >= total):
            return prices


def write_wide_csv(path, series):
    """Пишет {тикер: {дата: цена}} в широкий CSV по объединению дат."""
    tickers = list(series)
    dates = sorted(set().union(*(s.keys() for s in series.values())))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(["date"] + tickers)
        for date in dates:
            row = [date]
            for ticker in tickers:
                price = series[ticker].get(date)
                row.append("" if price is None else repr(price))
            writer.writerow(row)
    return len(dates)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--tickers", nargs="+", default=DEFAULT_TICKERS,
                        help="тикеры (по умолчанию — 20 акций из docs/REQUIREMENTS.md)")
    parser.add_argument("--from", dest="date_from", default="2019-01-01")
    parser.add_argument("--till", dest="date_till", default="2025-12-31")
    parser.add_argument("--board", default="TQBR", help="режим торгов (по умолчанию TQBR)")
    parser.add_argument("--out", type=Path, default=Path("data/prices.csv"))
    args = parser.parse_args(argv)

    series = {}
    for ticker in args.tickers:
        print(f"{ticker}: downloading...", file=sys.stderr)
        prices = download_ticker(ticker, args.date_from, args.date_till, args.board)
        if not prices:
            print(f"error: no data for {ticker} on board {args.board}", file=sys.stderr)
            return 1
        first, last = min(prices), max(prices)
        print(f"{ticker}: {len(prices)} days, {first} .. {last}", file=sys.stderr)
        series[ticker] = prices

    rows = write_wide_csv(args.out, series)
    print(f"Saved {rows} dates x {len(series)} tickers to {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
