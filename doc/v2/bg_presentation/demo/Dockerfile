# syntax=docker/dockerfile:1.6
# ============================================================================
#  Dockerfile за демото на дипломната работа
# ============================================================================
#  Образът съдържа всичко необходимо за изграждане и изпълнение на двете
#  демонстрации, плюс всички клиентски библиотеки за свързване с поддържаните
#  бази данни:
#    - GCC 14 (поддръжка на C++23 корутини)
#    - CMake + Ninja + pkg-config
#    - libsqlite3-dev               (SQLiteDB)
#    - libmariadb-dev               (MySQLDB / MariaDB)
#    - libpq-dev                    (PostgreSQLDB)
#    - libmongoc-dev + libbson-dev  (MongoDB)
#    - libhiredis-dev               (RedisDB)
#    - cassandra-cpp-driver         (CassandraDB --- сглобен от код)
#    - libneo4j-client              (Neo4jDB --- сглобен от код)
#    - liburing-dev                 (io_uring реактор)
#    - предварително изтеглен Boost.PFR в /opt/boost_pfr
#
#  Цел: пълно "out of the box" поведение --- никакви WSL / host
#  предварителни инсталации, никакви ръчни стъпки извън `docker compose up`.
# ============================================================================

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# ── 1. Системни инструменти + всички apt-достъпни клиентски библиотеки ─────
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++-14 \
        gcc-14 \
        cmake \
        ninja-build \
        pkg-config \
        git \
        ca-certificates \
        wget \
        bash \
        netcat-openbsd \
        # Build deps за драйверите, които компилираме от код
        autoconf \
        automake \
        libtool \
        m4 \
        # io_uring реактор
        liburing-dev \
        # SQLite
        libsqlite3-dev \
        # MariaDB / MySQL клиент (предоставя libmariadb.pc и mariadb.pc)
        libmariadb-dev \
        # PostgreSQL клиент (libpq)
        libpq-dev \
        # MongoDB C драйвер
        libmongoc-dev \
        libbson-dev \
        # Redis (hiredis)
        libhiredis-dev \
        # Транзитивни зависимости за Cassandra C++ драйвера
        libuv1-dev \
        libssl-dev \
        zlib1g-dev \
        libkrb5-dev \
        # Транзитивни зависимости за libneo4j-client
        libpcre2-dev \
        libedit-dev \
    && rm -rf /var/lib/apt/lists/*

# GCC 14 = подразбиращ се компилатор
ENV CC=gcc-14 \
    CXX=g++-14

# ── 2. Boost.PFR (header-only) ──────────────────────────────────────────────
ARG BOOST_PFR_VERSION=2.2.0
RUN cd /tmp && \
    wget -q "https://github.com/boostorg/pfr/archive/refs/tags/${BOOST_PFR_VERSION}.tar.gz" && \
    tar xzf "${BOOST_PFR_VERSION}.tar.gz" && \
    mv "pfr-${BOOST_PFR_VERSION}" /opt/boost_pfr && \
    rm -f "/tmp/${BOOST_PFR_VERSION}.tar.gz"

# CMake FetchContent ще намери предварително изтегления Boost.PFR вместо
# да тегли наново. (Името boost_pfr трябва да съвпада с FetchContent_Declare
# в lib/CMakeLists.txt.)
ENV FETCHCONTENT_SOURCE_DIR_BOOST_PFR=/opt/boost_pfr

# ── 3. Cassandra C++ драйвер ────────────────────────────────────────────────
#  Няма deb пакет в Ubuntu 24.04 нито за amd64, нито за arm64 — сглобяваме
#  от upstream. Активни опции: SSL ON (вече имаме libssl-dev),
#  KERBEROS ON (libkrb5-dev), MULTICORE_COMPILATION ON за по-бърз build.
ARG CASSANDRA_CPP_VERSION=2.17.1
RUN cd /tmp && \
    wget -q "https://github.com/apache/cassandra-cpp-driver/archive/refs/tags/${CASSANDRA_CPP_VERSION}.tar.gz" -O cassandra.tgz && \
    tar xzf cassandra.tgz && \
    cmake -G Ninja -S "cassandra-cpp-driver-${CASSANDRA_CPP_VERSION}" -B cassandra-build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCASS_BUILD_STATIC=OFF \
        -DCASS_BUILD_SHARED=ON \
        -DCASS_USE_OPENSSL=ON \
        -DCASS_USE_KERBEROS=ON \
        -DCASS_USE_ZLIB=ON \
        -DCASS_USE_LIBSSH2=OFF \
        -DCASS_BUILD_EXAMPLES=OFF \
        -DCASS_BUILD_TESTS=OFF && \
    ninja -C cassandra-build install && \
    ldconfig && \
    rm -rf /tmp/cassandra.tgz /tmp/cassandra-cpp-driver-${CASSANDRA_CPP_VERSION} /tmp/cassandra-build

# ── 4. libneo4j-client ─────────────────────────────────────────────────────
#  Няма в Ubuntu noble — сглобяваме от upstream (cleishm/libneo4j-client).
#  Изисква GNU make; ninja-build не го дърпа автоматично.
RUN apt-get update && apt-get install -y --no-install-recommends make \
    && rm -rf /var/lib/apt/lists/*

ARG LIBNEO4J_CLIENT_VERSION=2.2.0
#  GCC 14 включва нови warnings (stringop-truncation, dangling-pointer) които
#  v2.2.0 не очаква; configure-ът заковава -Werror. Изваждаме го с sed върху
#  генерираните Makefile-и преди да викнем make.
RUN cd /tmp && \
    wget -q "https://github.com/cleishm/libneo4j-client/releases/download/v${LIBNEO4J_CLIENT_VERSION}/libneo4j-client-${LIBNEO4J_CLIENT_VERSION}.tar.gz" && \
    tar xzf "libneo4j-client-${LIBNEO4J_CLIENT_VERSION}.tar.gz" && \
    cd "libneo4j-client-${LIBNEO4J_CLIENT_VERSION}" && \
    ./configure --disable-tools --prefix=/usr/local && \
    find . -name Makefile -exec sed -i 's/-Werror//g' {} + && \
    make -j"$(nproc)" && \
    make install && \
    ldconfig && \
    cd /tmp && \
    rm -rf "libneo4j-client-${LIBNEO4J_CLIENT_VERSION}" "libneo4j-client-${LIBNEO4J_CLIENT_VERSION}.tar.gz"

# ── Compat symlink за <mysql/mysql.h> ──────────────────────────────────────
#  Кодът на конектора (lib/include/ORM/db/connectors/MySQLDB/mysql_live.hpp)
#  включва `<mysql/mysql.h>`, което съответства на Oracle libmysqlclient-dev
#  layout-а. libmariadb-dev обаче слага хедърите в /usr/include/mariadb/ и
#  оставя /usr/include/mariadb/mysql/ почти празна (само client_plugin.h).
#  Един символен link от /usr/include/mysql към mariadb решава
#  несъответствието без да пипаме сорса.
RUN ln -sf /usr/include/mariadb /usr/include/mysql

WORKDIR /work
