# Оптимизатор портфеля по Марковицу

[![CI](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml)
[![Docs](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml)

Итоговый проект курса «Программная инженерия и C++ для количественного анализа
и алгоритмической торговли»: **«Оптимизатор портфеля по Марковицу: эффективная граница
и бэктест с ребалансировкой»**.

**Документация API:** https://neuroworknotneo.github.io/Software-Engineering-Project/
· **Требования:** [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md)
· **Архитектура:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

## Что делает программа

Консольная утилита `mpt` по историческим ценам акций Московской биржи:

| Команда | Что делает | Статус |
|---|---|---|
| `stats` | Годовые средние доходности, волатильности и ковариационная матрица (выборочная или Ledoit–Wolf); корректировка на сплиты, поиск аномалий в данных | ✅ готово |
| `optimize` | Портфели минимальной дисперсии и максимального Шарпа, с короткими продажами и без | ✅ готово |
| `frontier` | Эффективная граница из N точек, с короткими продажами и без | ✅ готово |
| `backtest` | Скользящее окно, ежемесячная ребалансировка, сравнение с портфелем 1/N | ✅ готово |

Данные: 20 ликвидных акций из индекса IMOEX за 2019–2025 годы (список —
в [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md)).

## Сборка

Нужны CMake ≥ 3.20 и компилятор C++20 (GCC ≥ 11 или Clang ≥ 14).
Eigen 3, GoogleTest и Google Benchmark берутся из системы, если установлены,
иначе CMake скачает их сам.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Данные

Цены выгружаются с MOEX ISS скриптом на Python (только стандартная библиотека):

```bash
python3 scripts/download_prices.py --out data/prices.csv
```

Формат — широкий CSV: `date,SBER,GAZP,...`, пустая ячейка — день без торгов.
Сплиты перечисляются в `data/splits.csv` в формате `date,ticker,ratio`, где ratio —
число новых акций на одну старую.

## Запуск

```bash
./build/mpt stats --prices data/prices.csv --splits data/splits.csv --cov ledoit-wolf
./build/mpt optimize --prices data/prices.csv --splits data/splits.csv --risk-free 0.10 --out portfolios.csv
./build/mpt frontier --prices data/prices.csv --splits data/splits.csv --points 50 --out frontier.csv
./build/mpt backtest --prices data/prices.csv --splits data/splits.csv --window 252 --cost 0.001 \
    --out equity.csv --weights-out weights.csv --metrics-out metrics.csv
./build/mpt --help
```

Без коротких продаж оптимальные портфели ищет собственный метод активного множества
(задача квадратичного программирования); с короткими продажами — точные формулы через
множители Лагранжа. Безрисковая ставка по умолчанию — 10% годовых.

Бэктест каждый месяц оценивает μ и Σ по последним 252 дням (только прошлые данные)
и сравнивает пять стратегий: минимальная дисперсия и максимальный Шарп с короткими
продажами и без них, а также равновзвешенный портфель 1/N. Метрики: среднегодовая
доходность, волатильность, коэффициент Шарпа, максимальная просадка, оборот.

Если в данных есть дневное изменение цены больше 50% (обычно неучтённый сплит),
утилита выводит предупреждение с датой и тикером.

## Запуск в Docker

Образ собирает проект, прогоняет тесты и содержит утилиту вместе с данными из `data/`:

```bash
docker build -t mpt .
docker run --rm mpt                                   # stats по data/prices.csv, Ledoit–Wolf
docker run --rm mpt stats --prices data/prices.csv --cov sample
docker run --rm -v "$PWD/my_data:/data" mpt stats --prices /data/prices.csv   # свои данные
```

## Графики

Скриптам нужен matplotlib (`pip install matplotlib`):

```bash
python3 scripts/plot_frontier.py frontier.csv --portfolios portfolios.csv --out frontier.png
python3 scripts/plot_backtest.py equity.csv --exclude max-sharpe/short --log --out backtest.png
```

## Тесты и бенчмарки

```bash
ctest --test-dir build --output-on-failure
./build/bench/mpt_bench
```

Тесты сверяют результаты с эталонами из numpy и scikit-learn и проверяют обработку
некорректных данных. Бенчмарки измеряют скорость оценки ковариации.

## Стиль кода

Код следует [стайлгайду курса](https://github.com/yandexdataschool/algorithms/blob/master/src/styleguide.md)
(Google C++ Style Guide с отступом 4 пробела и строками до 100 символов). Проверка:

```bash
clang-format --dry-run --Werror src/*.cpp src/*/*.cpp include/quant/*/*.hpp tests/*.cpp bench/*.cpp
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/*.cpp src/*/*.cpp
```

## Документация

```bash
doxygen docs/Doxyfile   # результат в docs/html/index.html
```

## Структура проекта

```
.
├── include/quant/      # публичные заголовки: data/, stats/, optim/, backtest/, io/
├── src/                # реализация библиотеки mpt_core и утилита mpt (main.cpp)
├── tests/              # unit-тесты (GoogleTest) и тестовые CSV
├── bench/              # бенчмарки (Google Benchmark)
├── scripts/            # Python: загрузка данных с MOEX, графики
├── data/               # цены и сплиты
├── docs/               # требования, архитектура, Doxyfile
├── .github/workflows/  # CI/CD (GitHub Actions)
├── Dockerfile          # сборка и запуск в контейнере
├── .clang-format       # форматирование кода
├── .clang-tidy         # статический анализ и проверка имён
├── AGENTS.md           # инструкции для ИИ-агентов
└── CLAUDE.md           # подключает AGENTS.md для Claude Code
```

## CI/CD

- **CI** (`.github/workflows/ci.yml`) — на каждый push и pull request три задачи:
  `style` — проверка форматирования (clang-format) и статический анализ (clang-tidy);
  `build-test-bench` — сборка, запуск утилиты, тесты, бенчмарки (результаты — в логе
  и в summary запуска); `docker` — сборка Docker-образа (с прогоном тестов) и запуск контейнера.
- **Docs** (`.github/workflows/docs.yml`) — на каждый push в `main`: документация
  Doxygen публикуется на GitHub Pages.
