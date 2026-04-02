# Optimized Multi-Stage Dockerfile with Per-Database Build and Test Stages
# Each database driver is built in isolation and tested independently
# This enables parallel builds and selective testing

# ══════════════════════════════════════════════════════════════════════════════
# Base Images
# ══════════════════════════════════════════════════════════════════════════════

# Common base with build tools
FROM ubuntu:24.04 AS base-builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    wget \
    curl \
    ca-certificates \
    libgtest-dev \
    libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# Common runtime base
FROM ubuntu:24.04 AS base-runtime
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# ══════════════════════════════════════════════════════════════════════════════
# PostgreSQL Stack
# ══════════════════════════════════════════════════════════════════════════════

FROM base-builder AS postgresql-builder
RUN apt-get update && apt-get install -y \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# Build libpqxx
RUN wget -q https://github.com/jtv/libpqxx/archive/refs/tags/7.7.5.tar.gz -O libpqxx.tar.gz \
    && tar -xzf libpqxx.tar.gz \
    && cd libpqxx-7.7.5 \
    && mkdir build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_SHARED_LIBS=ON \
    && make -j$(nproc) install \
    && ldconfig \
    && cd ../.. && rm -rf libpqxx-7.7.5 libpqxx.tar.gz

FROM base-runtime AS postgresql-runtime
RUN apt-get update && apt-get install -y \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=postgresql-builder /usr/local/lib/libpqxx* /usr/local/lib/
COPY --from=postgresql-builder /usr/local/include/pqxx /usr/local/include/pqxx
RUN ldconfig

FROM postgresql-runtime AS postgresql-test
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config libpq-dev libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=ON \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=OFF \
        -DORM_ENABLE_REDIS=OFF \
        -DORM_ENABLE_CASSANDRA=OFF \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=OFF \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest -R postgresql --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# MongoDB Stack
# ══════════════════════════════════════════════════════════════════════════════

FROM base-builder AS mongodb-builder
RUN apt-get update && apt-get install -y \
    libuv1-dev \
    libsasl2-dev \
    libsnappy-dev \
    zlib1g-dev \
    libzstd-dev \
    libmongoc-1.0-0 \
    libmongoc-dev \
    libbson-1.0-0 \
    libbson-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Build MongoDB C++ Driver
RUN git clone --depth 1 --branch r3.10.1 https://github.com/mongodb/mongo-cxx-driver.git \
    && cd mongo-cxx-driver \
    && rm -rf build && mkdir build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_CXX_STANDARD=17 \
    && make -j$(nproc) install \
    && ldconfig \
    && cd ../.. && rm -rf mongo-cxx-driver

FROM base-runtime AS mongodb-runtime
RUN apt-get update && apt-get install -y \
    libuv1 \
    libsasl2-2 \
    libsnappy1v5 \
    zlib1g \
    libzstd1 \
    libmongoc-1.0-0 \
    libbson-1.0-0 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=mongodb-builder /usr/local/lib/libbsoncxx* /usr/local/lib/
COPY --from=mongodb-builder /usr/local/lib/libmongocxx* /usr/local/lib/
COPY --from=mongodb-builder /usr/local/include/bsoncxx /usr/local/include/bsoncxx
COPY --from=mongodb-builder /usr/local/include/mongocxx /usr/local/include/mongocxx
RUN ldconfig

FROM mongodb-runtime AS mongodb-test
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config \
    libmongoc-dev libbson-dev libuv1-dev libssl-dev \
    libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=OFF \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=ON \
        -DORM_ENABLE_REDIS=OFF \
        -DORM_ENABLE_CASSANDRA=OFF \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=OFF \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest -R mongodb --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# Redis Stack
# ══════════════════════════════════════════════════════════════════════════════

FROM base-runtime AS redis-runtime
RUN apt-get update && apt-get install -y \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

FROM redis-runtime AS redis-test
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=OFF \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=OFF \
        -DORM_ENABLE_REDIS=ON \
        -DORM_ENABLE_CASSANDRA=OFF \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=OFF \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest -R redis --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# Cassandra Stack
# ══════════════════════════════════════════════════════════════════════════════

FROM base-builder AS cassandra-builder
RUN apt-get update && apt-get install -y \
    libuv1-dev \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Build Cassandra C++ Driver
RUN wget -q https://github.com/datastax/cpp-driver/archive/refs/tags/2.17.1.tar.gz -O cassandra.tar.gz \
    && tar -xzf cassandra.tar.gz \
    && cd cassandra-cpp-driver-2.17.1 \
    && mkdir build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCASS_BUILD_STATIC=ON \
        -DCASS_BUILD_SHARED=ON \
    && make -j$(nproc) install \
    && ldconfig \
    && cd ../.. && rm -rf cassandra-cpp-driver-2.17.1 cassandra.tar.gz

FROM base-runtime AS cassandra-runtime
RUN apt-get update && apt-get install -y \
    libuv1 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*
COPY --from=cassandra-builder /usr/local/lib/ /usr/local/lib/
COPY --from=cassandra-builder /usr/local/include/cassandra.h /usr/local/include/
RUN ldconfig

FROM cassandra-runtime AS cassandra-test
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config libuv1-dev libssl-dev \
    libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=OFF \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=OFF \
        -DORM_ENABLE_REDIS=OFF \
        -DORM_ENABLE_CASSANDRA=ON \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=OFF \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest -R cassandra --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# SQLite Stack
# ══════════════════════════════════════════════════════════════════════════════

FROM base-runtime AS sqlite-runtime
RUN apt-get update && apt-get install -y \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

FROM sqlite-runtime AS sqlite-test
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=OFF \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=OFF \
        -DORM_ENABLE_REDIS=OFF \
        -DORM_ENABLE_CASSANDRA=OFF \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=ON \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest -R sqlite --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# Unit + MockDB Test Stage (no external DB services required)
# ══════════════════════════════════════════════════════════════════════════════

FROM base-builder AS unit-test
WORKDIR /workspace
COPY . /workspace/
RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_ENABLE_POSTGRESQL=OFF \
        -DORM_ENABLE_MYSQL=OFF \
        -DORM_ENABLE_MONGODB=OFF \
        -DORM_ENABLE_REDIS=OFF \
        -DORM_ENABLE_CASSANDRA=OFF \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=OFF \
    && cmake --build . -j$(nproc)
CMD ["bash", "-c", "cd build && ctest --output-on-failure"]

# ══════════════════════════════════════════════════════════════════════════════
# Combined Runtime (All Drivers)
# ══════════════════════════════════════════════════════════════════════════════

FROM base-runtime AS all-drivers-runtime
RUN apt-get update && apt-get install -y \
    libpq5 \
    libuv1 \
    libsasl2-2 \
    libsnappy1v5 \
    zlib1g \
    libzstd1 \
    libmongoc-1.0-0 \
    libbson-1.0-0 \
    libhiredis-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy all built drivers
COPY --from=postgresql-builder /usr/local/lib/libpqxx* /usr/local/lib/
COPY --from=postgresql-builder /usr/local/include/pqxx /usr/local/include/pqxx
COPY --from=mongodb-builder /usr/local/lib/libbsoncxx* /usr/local/lib/
COPY --from=mongodb-builder /usr/local/lib/libmongocxx* /usr/local/lib/
COPY --from=mongodb-builder /usr/local/include/bsoncxx /usr/local/include/bsoncxx
COPY --from=mongodb-builder /usr/local/include/mongocxx /usr/local/include/mongocxx
COPY --from=cassandra-builder /usr/local/lib/ /usr/local/lib/
COPY --from=cassandra-builder /usr/local/include/cassandra.h /usr/local/include/
RUN ldconfig

# ══════════════════════════════════════════════════════════════════════════════
# Full Test Suite (All Drivers)
# ══════════════════════════════════════════════════════════════════════════════

FROM all-drivers-runtime AS all-test
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libpq-dev \
    libmysqlclient-dev \
    libmongoc-dev \
    libbson-dev \
    libuv1-dev \
    libssl-dev \
    libgtest-dev \
    libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace/

RUN mkdir -p build && cd build \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DORM_USE_SYSTEM_LIBS=ON \
        -DORM_ENABLE_POSTGRESQL=ON \
        -DORM_ENABLE_MYSQL=ON \
        -DORM_ENABLE_MONGODB=ON \
        -DORM_ENABLE_REDIS=ON \
        -DORM_ENABLE_CASSANDRA=ON \
        -DORM_ENABLE_NEO4J=OFF \
        -DORM_ENABLE_SQLITE=ON \
    && cmake --build . -j$(nproc)

CMD ["bash", "-c", "cd build && ctest --output-on-failure"]
