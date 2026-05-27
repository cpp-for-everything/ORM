# syntax=docker/dockerfile:1.6
# ============================================================================
#  Dockerfile за демото на дипломната работа
# ============================================================================
#  Образът съдържа всичко необходимо за изграждане и изпълнение на двете
#  демонстрации (capability gating + асинхронен бенчмарк/MariaDB проба):
#    - GCC 14 (поддръжка на C++23 корутини)
#    - CMake + Ninja
#    - libsqlite3-dev (за Демо A)
#    - libmariadb-dev (за MariaDB пробата на Демо B)
#    - liburing-dev (за io_uring реактора)
#    - предварително изтеглен Boost.PFR в /opt/boost_pfr
# ============================================================================

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++-14 \
        gcc-14 \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        wget \
        libsqlite3-dev \
        libmariadb-dev \
        liburing-dev \
        netcat-openbsd \
        bash \
    && rm -rf /var/lib/apt/lists/*

# GCC 14 = подразбиращ се компилатор
ENV CC=gcc-14 \
    CXX=g++-14

# Boost.PFR (header-only) се изтегля еднократно при изграждане на образа,
# така че примерите и пробата не теглят нищо при изпълнение.
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

WORKDIR /work
