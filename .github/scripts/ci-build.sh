#!/usr/bin/env bash
# Configure, build, and run ctest for the host CI matrix.
# Usage: ci-build.sh <Debug|Release> [build-dir]
set -euo pipefail

BUILD_TYPE="${1:?build type (Debug|Release)}"
BUILD_DIR="${2:-build}"

if [[ -z "${CXX:-}" ]]; then
  if command -v c++ >/dev/null 2>&1; then
    CXX=c++
  else
    echo "CXX is not set and c++ was not found on PATH" >&2
    exit 1
  fi
fi

mkdir -p "$BUILD_DIR"
{
  echo "build_type=${BUILD_TYPE}"
  echo "CC=${CC:-}"
  echo "CXX=${CXX}"
  echo "----- compiler version -----"
  case "${CXX##*/}" in
    cl|cl.exe|CL|CL.exe)
      # MSVC cl.exe prints its banner to stderr and exits non-zero with no inputs.
      "${CXX}" 2>&1 | head -n 5 || true
      ;;
    *)
      "${CXX}" --version
      ;;
  esac
} | tee compiler-version.txt

# CMAKE_EXTRA_ARGS is a word-split list of extra -D flags (e.g. vcpkg toolchain).
# shellcheck disable=SC2086
cmake -S . -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON \
  -DCMAKE_CXX_EXTENSIONS=OFF \
  -DORM_ENABLE_SQLITE=ON \
  ${CMAKE_EXTRA_ARGS:-}

cmake --build "$BUILD_DIR" --parallel

echo "----- ctest -N (registered tests) -----"
ctest --test-dir "$BUILD_DIR" -N | tee ctest-list.txt

set +e
ctest --test-dir "$BUILD_DIR" --output-on-failure --output-junit ctest-results.xml | tee ctest.log
exit_code=${PIPESTATUS[0]}
set -e
echo "${exit_code}" > ctest-exit.txt

{
  echo "## CI result"
  echo
  echo "- build_type: ${BUILD_TYPE}"
  echo "- ctest exit code: ${exit_code}"
  echo "- sqlite: ORM_ENABLE_SQLITE=ON"
  echo "- live DB connectors: not enabled in this job (see docker-tests.yml)"
  echo
  echo "### compiler"
  echo '```'
  cat compiler-version.txt
  echo '```'
  echo
  echo "### ctest -N"
  echo '```'
  cat ctest-list.txt
  echo '```'
  echo
  echo "### ctest --output-on-failure"
  echo '```'
  cat ctest.log
  echo '```'
} | tee ci-report.md

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  cat ci-report.md >> "${GITHUB_STEP_SUMMARY}"
fi

exit "${exit_code}"
