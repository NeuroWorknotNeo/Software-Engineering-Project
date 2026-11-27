# AGENTS.md

Инструкции для ИИ-агентов (Claude Code, Codex, Cursor и т. п.), работающих с этим репозиторием.

## Назначение проекта

Итоговый C++-проект для курса по количественному анализу и алгоритмической торговле.

**Тема (утверждена 16.11.2026):** «Оптимизатор портфеля по Марковицу: эффективная
граница и бэктест с ребалансировкой». Требования — `docs/REQUIREMENTS.md`,
архитектура — `docs/ARCHITECTURE.md`. При реализации
сверяйся с ними и обновляй матрицу трассировки в `docs/ARCHITECTURE.md`.

## Структура

- `include/quant/<модуль>/` — публичные заголовки (`.hpp`); модули: `data`, `stats`, `optim`,
  `io`, далее `backtest` (см. `docs/ARCHITECTURE.md`).
- `src/<модуль>/` — реализация библиотеки `mpt_core`; `src/main.cpp` — утилита `mpt`.
- `tests/` — unit-тесты на GoogleTest, по файлу `test_<модуль>.cpp`; тестовые CSV — в `tests/data/`.
- `bench/` — бенчмарки на Google Benchmark, по файлу `bench_<модуль>.cpp`.
- `scripts/` — Python-скрипты (загрузка данных с MOEX, графики), только стандартная библиотека
  для загрузки.
- `data/` — цены (`prices.csv`) и сплиты (`splits.csv`).
- `docs/` — требования, архитектура, `Doxyfile`.
- `.github/workflows/` — CI (`ci.yml`) и публикация документации (`docs.yml`).

## Команды

```bash
# Сборка
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Тесты
ctest --test-dir build --output-on-failure

# Запуск
./build/mpt stats --prices data/prices.csv --splits data/splits.csv
./build/mpt optimize --prices data/prices.csv --splits data/splits.csv
./build/mpt frontier --prices data/prices.csv --splits data/splits.csv --points 50

# Бенчмарки
./build/bench/mpt_bench

# Документация
doxygen docs/Doxyfile
```

## Правила оформления кода

Стиль — [стайлгайд курса](https://github.com/yandexdataschool/algorithms/blob/master/src/styleguide.md):
полный [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) с правками курса.
Форматирование задаёт `.clang-format`, проверки — `.clang-tidy`.

- Стандарт C++20, без расширений компилятора; сборка без предупреждений с `-Wall -Wextra -Wpedantic`.
- Весь код библиотеки — в пространстве имён `quant`; заголовки начинаются с `#pragma once`.
- Отступ 4 пробела, строки до 100 символов; `if`/`for`/`while` — всегда с фигурными скобками
  и не на одной строке с телом.
- Имена (Google): типы и функции — `PascalCase` (`ReadPriceCsv`, `CovarianceEstimator`);
  переменные, параметры и поля структур — `snake_case` (`daily_return`); приватные поля
  классов — `snake_case_`; константы — `kPascalCase`; файлы — `snake_case`.
- Имена длинные и понятные, без сокращений (`index`, а не `idx`); однобуквенные — только счётчик
  в коротком цикле. В имени функции — глагол, одно действие.
- Никаких «магических» чисел — именованные константы.
- Входные параметры — по константной ссылке (примитивы — по значению); результат — возвращаемым
  значением. Если функция меняет данные, она принимает их по значению и возвращает новые.
- `struct` — только данные без логики; `class` — логика, все поля приватные.
- `const` везде, где возможно; преинкремент `++i`; `std::fabs` для вещественных чисел.
- Без `using namespace std;`, без C-массивов, C-строк и `printf`/`scanf`, без `new`/`malloc`.
- Комментарии — над кодом, а не в конце строки; к функциям и классам — Doxygen-комментарии
  (`/** ... */`, `@brief`, `@param`, `@return`) в заголовке.
- Новая функциональность сопровождается unit-тестом; критичный по скорости код — бенчмарком.

Проверка стиля перед коммитом:

```bash
clang-format -i src/*.cpp src/*/*.cpp include/quant/*/*.hpp tests/*.cpp bench/*.cpp
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/*.cpp src/*/*.cpp
```

## Ограничения для агента

- Перед завершением задачи собери проект и запусти тесты — всё должно проходить.
- Не меняй CI/CD (`.github/workflows/`) без явного запроса.
- Не удаляй и не отключай существующие тесты, чтобы «починить» сборку.
- Не добавляй новые внешние зависимости без согласования (сейчас: Eigen 3, GoogleTest,
  Google Benchmark).
- Математические модули (`stats`, `optim`) не работают с файлами и консолью; ввод-вывод —
  только в `data`, `io` и `main.cpp`.
- Не коммить артефакты сборки (`build/`, `docs/html/`).
