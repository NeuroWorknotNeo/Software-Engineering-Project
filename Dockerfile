# Сборка и запуск mpt в чистом окружении.
#
#   docker build -t mpt .
#   docker run --rm mpt                       # mpt stats по данным из репозитория
#   docker run --rm mpt stats --prices data/prices.csv --cov sample
#   docker run --rm -v "$PWD/my_data:/data" mpt stats --prices /data/prices.csv

# --- Этап 1: сборка и тесты ---
FROM ubuntu:24.04 AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake libeigen3-dev libgtest-dev libbenchmark-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
COPY bench bench

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)" \
    && ctest --test-dir build --output-on-failure

# --- Этап 2: только утилита и данные ---
FROM ubuntu:24.04

WORKDIR /app
COPY --from=build /src/build/mpt /usr/local/bin/mpt
COPY data data

ENTRYPOINT ["mpt"]
CMD ["stats", "--prices", "data/prices.csv", "--splits", "data/splits.csv", "--cov", "ledoit-wolf"]
