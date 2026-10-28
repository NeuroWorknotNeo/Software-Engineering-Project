# Software-Engineering-Project

[![CI](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml)
[![Docs](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml)

Итоговый проект курса «Программная инженерия и C++ для количественного анализа
и алгоритмической торговли».

> [!NOTE]
> **Тема итогового проекта ещё не утверждена.** Сейчас репозиторий — только настройка
> инфраструктуры: сборка через CMake, unit-тесты, бенчмарки, документация и CI/CD.
> Расчёт опционов по Блэку–Шоулзу — временный пример кода, чтобы инфраструктуре было что
> собирать, тестировать и измерять. После утверждения темы он будет заменён.

**Документация:** https://neuroworknotneo.github.io/Software-Engineering-Project/

## Что делает программа

`bs_app` считает справедливые цены европейских опционов Call и Put по формуле
Блэка–Шоулза:

- C = S·N(d1) − K·e^(−rT)·N(d2)
- P = K·e^(−rT)·N(−d2) − S·N(−d1)
- d1 = (ln(S/K) + (r + σ²/2)·T) / (σ·√T), d2 = d1 − σ·√T

где S — цена базового актива, K — страйк, r — безрисковая ставка, σ — волатильность,
T — время до экспирации в годах, N — функция распределения стандартного нормального закона.

```bash
./build/bs_app                       # пример: S=100 K=100 r=0.05 sigma=0.2 T=1
./build/bs_app 42 40 0.1 0.2 0.5     # свои параметры: S K r sigma T
```

```
S=42.0000 K=40.0000 r=0.1000 sigma=0.2000 T=0.5000
Call: 4.7594
Put:  0.8086
```

## Структура проекта

```
.
├── include/quant/      # публичные заголовки библиотеки
├── src/                # реализация библиотеки и main.cpp
├── tests/              # unit-тесты (GoogleTest)
├── bench/              # бенчмарки (Google Benchmark)
├── docs/Doxyfile       # конфигурация Doxygen
├── .github/workflows/  # CI/CD (GitHub Actions)
├── CMakeLists.txt
├── AGENTS.md           # инструкции для ИИ-агентов
└── CLAUDE.md           # подключает AGENTS.md для Claude Code
```

## Сборка

Нужны CMake ≥ 3.20 и компилятор с поддержкой C++20 (GCC ≥ 10 или Clang ≥ 12).
GoogleTest и Google Benchmark берутся из системы, если установлены, иначе CMake
скачает их сам.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bs_app
```

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

Тесты сверяют цены с эталонными значениями (в том числе с примером из учебника Hull),
проверяют паритет Put–Call и обработку некорректных параметров.

## Бенчмарки

```bash
./build/bench/bs_bench
```

## Документация

```bash
doxygen docs/Doxyfile   # результат в docs/html/index.html
```

## CI/CD

- **CI** (`.github/workflows/ci.yml`) — на каждый push и pull request: сборка,
  запуск тестов и бенчмарков; результаты бенчмарка видны в логе и в summary запуска.
- **Docs** (`.github/workflows/docs.yml`) — на каждый push в `main`: генерация
  документации Doxygen и публикация на GitHub Pages (ветка `gh-pages`).
