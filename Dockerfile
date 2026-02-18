FROM fedora:40

ARG JOBS=0

WORKDIR /src

# Local libevent source archive (for static build) is expected here.
COPY local-src/ /src/local-src/
COPY . .

RUN set -eux; \
    dnf -y install \
      gcc-c++ \
      make \
      cmake \
      ninja-build \
      pkgconf-pkg-config \
      binutils \
      boost-devel \
      boost-static \
      openssl-devel \
      zlib-static \
      glibc-static \
      libstdc++-static \
      libevent-devel; \
    if ! find /usr/lib64 -name 'libevent_core.a' -print -quit | grep -q .; then \
      tar -xf /src/local-src/libevent-2.1.11-stable.tar.gz -C /tmp; \
      LIBEVENT_DIR="$(find /tmp -maxdepth 1 -type d -name 'libevent-*' | head -n 1)"; \
      test -n "${LIBEVENT_DIR}"; \
      cd "${LIBEVENT_DIR}"; \
      ./configure --disable-shared --enable-static --prefix=/usr/local; \
      make -j"$(nproc)"; \
      make install; \
      cd /src; \
    fi;

RUN set -eux; \
    if [ "${JOBS}" = "0" ]; then JOBS="$(nproc)"; fi; \
    cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-static" \
      -DBUILD_DAEMON=ON \
      -DBUILD_CLI=OFF \
      -DBUILD_TX=OFF \
      -DBUILD_UTIL=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_BENCH=OFF \
      -DBUILD_FUZZ_BINARY=OFF \
      -DENABLE_WALLET=OFF \
      -DWITH_ZMQ=OFF \
      -DENABLE_IPC=OFF; \
    cmake --build build --target bitcoind -j"${JOBS}"; \
    strip build/bin/bitcoind; \
    ! readelf -d build/bin/bitcoind | grep -q NEEDED

CMD ["/bin/sh"]
