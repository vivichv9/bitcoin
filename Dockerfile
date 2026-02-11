FROM alpine:3.21

ARG ALPINE_REPO_BASE=https://mirrors.edge.kernel.org/alpine
ARG ALPINE_VERSION=v3.21
ARG ALPINE_REPO_TOKEN=
ARG JOBS=0

WORKDIR /src

COPY certs/ /usr/local/share/ca-certificates/

RUN set -eux; \
    if [ -n "${ALPINE_REPO_TOKEN}" ]; then \
      AUTH_REPO_BASE="$(echo "${ALPINE_REPO_BASE}" | sed "s#^https://#https://token:${ALPINE_REPO_TOKEN}@#")"; \
    else \
      AUTH_REPO_BASE="${ALPINE_REPO_BASE}"; \
    fi; \
    printf '%s\n' \
      "${AUTH_REPO_BASE}/${ALPINE_VERSION}/main" \
      "${AUTH_REPO_BASE}/${ALPINE_VERSION}/community" \
      > /etc/apk/repositories; \
    apk add --no-cache \
    bash \
    bison \
    build-base \
    ca-certificates \
    cmake \
    curl \
    linux-headers \
    make \
    patch \
    pkgconf \
    python3 \
    samurai \
    xz; \
    update-ca-certificates; \
    printf '%s\n' \
      "${ALPINE_REPO_BASE}/${ALPINE_VERSION}/main" \
      "${ALPINE_REPO_BASE}/${ALPINE_VERSION}/community" \
      > /etc/apk/repositories

COPY . .

RUN set -eux; \
    test -d /src/depends/offline-sources; \
    test -d /src/depends/offline-sources/download-stamps; \
    if [ "${JOBS}" = "0" ]; then JOBS="$(nproc)"; fi; \
    HOST_TRIPLET="$(/src/depends/config.guess)"; \
    make -C depends -j"${JOBS}" \
      HOST="${HOST_TRIPLET}" \
      SOURCES_PATH="/src/depends/offline-sources" \
      build_linux_DOWNLOAD=false \
      NO_QT=1 \
      NO_WALLET=1 \
      NO_ZMQ=1 \
      NO_USDT=1 \
      NO_IPC=1; \
    cmake -B build -G Ninja \
      --toolchain="depends/${HOST_TRIPLET}/toolchain.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-static" \
      -DBUILD_DAEMON=ON \
      -DBUILD_CLI=ON \
      -DBUILD_TX=OFF \
      -DBUILD_UTIL=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_BENCH=OFF \
      -DBUILD_FUZZ_BINARY=OFF \
      -DENABLE_WALLET=OFF \
      -DWITH_ZMQ=OFF \
      -DENABLE_IPC=OFF; \
    cmake --build build --target bitcoind bitcoin-cli -j"${JOBS}"; \
    strip build/bin/bitcoind; \
    ! readelf -d build/bin/bitcoind | grep -q NEEDED

CMD ["/bin/sh"]
