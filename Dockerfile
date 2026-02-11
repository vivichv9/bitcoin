FROM alpine:3.21

ARG ALPINE_REPO_BASE=https://mirrors.edge.kernel.org/alpine
ARG ALPINE_VERSION=v3.21

RUN printf '%s\n' \
    "${ALPINE_REPO_BASE}/${ALPINE_VERSION}/main" \
    "${ALPINE_REPO_BASE}/${ALPINE_VERSION}/community" \
    > /etc/apk/repositories

RUN apk add --no-cache \
    bash \
    bison \
    build-base \
    boost-dev \
    ca-certificates \
    cmake \
    curl \
    git \
    libevent-dev \
    linux-headers \
    make \
    patch \
    pkgconf \
    python3 \
    py3-pip \
    sqlite-dev \
    samurai \
    xz \
    zeromq-dev

WORKDIR /src
COPY . .

RUN set -eux; \
    cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBUILD_DAEMON=ON \
      -DBUILD_CLI=ON \
      -DBUILD_TX=OFF \
      -DBUILD_UTIL=OFF \
      -DBUILD_TESTS=ON \
      -DBUILD_BENCH=OFF \
      -DBUILD_FUZZ_BINARY=OFF \
      -DENABLE_WALLET=OFF \
      -DWITH_ZMQ=OFF \
      -DENABLE_IPC=OFF; \
    cmake --build build --target bitcoind bitcoin-cli -j 13; \
    ln -sf /src/build/test/config.ini /src/test/config.ini

CMD ["/bin/sh"]
