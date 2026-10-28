# Software-Engineering-Project

[![CI](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/ci.yml)
[![Docs](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml/badge.svg)](https://github.com/NeuroWorknotNeo/Software-Engineering-Project/actions/workflows/docs.yml)

Заготовка C++-проекта для курса «Программная инженерия и C++ для количественного
анализа и алгоритмической торговли». Пока программа просто печатает `Hello World!`,
но вокруг неё уже настроена вся инфраструктура: сборка через CMake, unit-тесты,
бенчмарки, документация и CI/CD.

**Документация:** https://neuroworknotneo.github.io/Software-Engineering-Project/

## Что делает программа

`hello_app` выводит в stdout строку `Hello World!`. Строку формирует функция
`hello::greeting()` из библиотеки `hello` — её же проверяют тесты и измеряет бенчмарк.

## Структура проекта

```
.
├── include/hello/      # публичные заголовки библиотеки
├── src/                # исходный код библиотеки и main.cpp
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
./build/hello_app
```

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

## Бенчмарки

```bash
./build/bench/hello_bench
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
