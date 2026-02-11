# Пошаговая сборка Bitcoin Core: локальные архивы, `depends`, виды линковки

Этот документ описывает полный процесс сборки узла с акцентом на:

- локальный кэш архивов зависимостей (`depends/offline-sources`),
- сборку без скачивания архивов во время `docker build`,
- разные режимы линковки (dynamic / mostly static / fully static),
- типовые ошибки и как их исправлять.

## 1. Что уже есть в репозитории

- Скрипт для загрузки архивов в одну папку: `depends/fetch_depends_sources.sh`
- Папка локального кэша: `depends/offline-sources`
- Docker-сборка (обновленная): `Dockerfile`
- Distroless-образ (не менялся): `Dockerfile.distroless`

## 2. Базовые понятия

- `depends` — система сборки и кэширования сторонних библиотек Bitcoin Core.
- `SOURCES_PATH` — директория, где лежат исходные архивы библиотек.
- `download-stamps` — служебные файлы `depends`, фиксирующие, какие архивы и с какими hash уже учтены.
- `toolchain.cmake` — файл, который заставляет CMake использовать библиотеки из `depends`.

## 3. Вариант A: скачать архивы автоматически (рекомендуется)

Запустить из корня репозитория:

```bash
NO_QT=1 NO_WALLET=1 NO_ZMQ=1 NO_USDT=1 NO_IPC=1 \
./depends/fetch_depends_sources.sh depends/offline-sources host
```

Что делает команда:

1. Создаёт/использует `depends/offline-sources`.
2. Скачивает все нужные архивы для выбранного набора опций.
3. Создаёт `download-stamps`, чтобы `depends` мог работать с этим кэшем.

Важно: если собираешь `depends` с флагами `NO_*`, при загрузке архивов используй те же флаги.

## 4. Вариант B: положить архивы вручную (без скачивания)

Если у тебя архивы уже есть локально:

1. Положи их в `depends/offline-sources`.
2. Имена файлов должны точно совпадать с ожидаемыми в `depends/packages/*.mk`.
3. Создай `download-stamps` через `download-one`:

```bash
HOST_TRIPLET=$(./depends/config.guess)

make -C depends download-one \
  HOST="$HOST_TRIPLET" \
  SOURCES_PATH="$(pwd)/depends/offline-sources" \
  FALLBACK_DOWNLOAD_PATH="file://$(pwd)/depends/offline-sources" \
  NO_QT=1 NO_WALLET=1 NO_ZMQ=1 NO_USDT=1 NO_IPC=1
```

Если архив кастомный (не официальный), checksum не совпадёт.
Тогда нужно править соответствующий `*_sha256_hash` в `depends/packages/*.mk`.

## 5. Сборка `depends` из локального кэша

```bash
HOST_TRIPLET=$(./depends/config.guess)

NO_QT=1 NO_WALLET=1 NO_ZMQ=1 NO_USDT=1 NO_IPC=1 \
make -C depends -j"$(nproc)" \
  HOST="$HOST_TRIPLET" \
  SOURCES_PATH="$(pwd)/depends/offline-sources"
```

Это создаст toolchain:

- `depends/<host-triplet>/toolchain.cmake`

## 6. Сборка самого узла через toolchain `depends`

```bash
HOST_TRIPLET=$(./depends/config.guess)

cmake -B build -G Ninja \
  --toolchain="depends/${HOST_TRIPLET}/toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_DAEMON=ON \
  -DBUILD_CLI=ON \
  -DBUILD_TX=OFF \
  -DBUILD_UTIL=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_BENCH=OFF \
  -DBUILD_FUZZ_BINARY=OFF \
  -DENABLE_WALLET=OFF \
  -DWITH_ZMQ=OFF \
  -DENABLE_IPC=OFF

cmake --build build -j"$(nproc)" --target bitcoind bitcoin-cli
```

## 7. Виды линковки

### 7.1 Dynamic linking

Смысл: бинарник ссылается на `.so`/`.dylib` в системе.

Признаки:

- Linux: `ldd build/bin/bitcoind` показывает список зависимостей.
- macOS: `otool -L build/bin/bitcoind` показывает `.dylib`.

Плюсы: проще сборка.
Минусы: зависимость от окружения рантайма.

### 7.2 Mostly static (частично статическая)

Смысл: часть библиотек берётся статически из `depends`, но glibc/системные части могут остаться динамическими.

Обычно получается при использовании `depends` без агрессивных `-static` флагов.

Плюсы: меньше внешних зависимостей.
Минусы: не всегда полностью автономный бинарник.

### 7.3 Fully static (полностью статическая, Linux)

Смысл: итоговый бинарник без `NEEDED`.

Типичный флаг:

- `-DCMAKE_EXE_LINKER_FLAGS="-static"`

Проверка:

```bash
readelf -d build/bin/bitcoind | grep NEEDED
```

Если вывода нет, динамических зависимостей нет.

Важно:

- На macOS полная статическая линковка не поддерживается (ошибки вида `crt0.o not found`).
- Полностью статический бинарник обычно собирают в Linux-контейнере (musl/Alpine).

## 8. Сборка через Dockerfile (текущий процесс)

Текущий `Dockerfile` ожидает, что архивы уже подготовлены заранее в `depends/offline-sources`.

### 8.1 Подготовить архивы до сборки образа

```bash
NO_QT=1 NO_WALLET=1 NO_ZMQ=1 NO_USDT=1 NO_IPC=1 \
./depends/fetch_depends_sources.sh depends/offline-sources host
```

### 8.2 Собрать образ

```bash
docker build -f Dockerfile -t bitcoin-builder .
```

Что происходит внутри:

1. Проверяется наличие `/src/depends/offline-sources` и `download-stamps`.
2. Собирается `depends` с `SOURCES_PATH=/src/depends/offline-sources`.
3. Сеть для скачивания архивов отключена (`build_linux_DOWNLOAD=false`).
4. Собираются `bitcoind` и `bitcoin-cli` через `depends` toolchain.

## 9. Очистка/пересоздание кэша архивов

Удалить целиком:

```bash
rm -rf depends/offline-sources
```

Удалить только файлы, оставить папку:

```bash
find depends/offline-sources -type f -delete
```

Полная очистка `depends`-артефактов:

```bash
make -C depends clean
```

## 10. Частые ошибки

### 10.1 "Почему начал собираться Qt, хотя я его отключал?"

Потому что флаги `NO_QT=1 ...` нужно передавать не только при загрузке архивов, но и при `make -C depends`.

### 10.2 "No such file or directory" при `SOURCES_PATH`

Причина обычно в относительном пути при `make -C depends`.
Используй абсолютный путь (скрипт `depends/fetch_depends_sources.sh` уже делает это автоматически).

### 10.3 Ошибка CMake про `-static` на macOS

Нельзя сделать fully static на macOS стандартным способом.
Собирай fully static в Linux-контейнере.

### 10.4 "CMake was unable to find Ninja"

Установи `ninja` или используй `-G "Unix Makefiles"`.

## 11. Минимальный рекомендуемый pipeline

1. Подготовить архивы:

```bash
NO_QT=1 NO_WALLET=1 NO_ZMQ=1 NO_USDT=1 NO_IPC=1 \
./depends/fetch_depends_sources.sh depends/offline-sources host
```

2. Собрать образ:

```bash
docker build -f Dockerfile -t bitcoin-builder .
```

3. Проверить линковку внутри контейнера (если нужен fully static):

```bash
readelf -d /src/build/bin/bitcoind | grep NEEDED
```

