# DockerRun.cmake — reusable CTest script for spinning up a Docker container,
# running a test binary against it, then tearing the container down.
#
# Required variables (pass via -D on the cmake -P command line):
#   DB              — short name, e.g. mysql, postgresql, mongodb, redis, cassandra, neo4j
#   IMAGE           — Docker image tag, e.g. mysql:8
#   HOST_PORT       — port on localhost to bind
#   CONTAINER_PORT  — port inside the container
#   TEST_BINARY     — path to the compiled GTest binary
#
# Optional variables:
#   EXTRA_DOCKER_ARGS — additional arguments inserted before the image name
#   EXTRA_ENV_ARGS    — extra -e KEY=VAL pairs for the docker run command

if(NOT DEFINED DB)
    message(FATAL_ERROR "DockerRun.cmake: DB variable is not set")
endif()
if(NOT DEFINED IMAGE)
    message(FATAL_ERROR "DockerRun.cmake: IMAGE variable is not set")
endif()
if(NOT DEFINED HOST_PORT)
    message(FATAL_ERROR "DockerRun.cmake: HOST_PORT variable is not set")
endif()
if(NOT DEFINED CONTAINER_PORT)
    message(FATAL_ERROR "DockerRun.cmake: CONTAINER_PORT variable is not set")
endif()
if(NOT DEFINED TEST_BINARY)
    message(FATAL_ERROR "DockerRun.cmake: TEST_BINARY variable is not set")
endif()

set(CONTAINER_NAME "orm_test_${DB}")

# ── 1. Remove any stale container with the same name ─────────────────────────
execute_process(
    COMMAND docker rm -f ${CONTAINER_NAME}
    OUTPUT_QUIET ERROR_QUIET
)

# ── 2. Start the container in detached mode ───────────────────────────────────
set(DOCKER_RUN_CMD
    docker run -d
    --name ${CONTAINER_NAME}
    -p ${HOST_PORT}:${CONTAINER_PORT}
)

if(DEFINED EXTRA_DOCKER_ARGS)
    list(APPEND DOCKER_RUN_CMD ${EXTRA_DOCKER_ARGS})
endif()

list(APPEND DOCKER_RUN_CMD ${IMAGE})

execute_process(
    COMMAND ${DOCKER_RUN_CMD}
    RESULT_VARIABLE docker_rc
)
if(NOT docker_rc EQUAL 0)
    message(FATAL_ERROR "DockerRun: 'docker run' failed for ${IMAGE}")
endif()

# ── 3. Wait for the port to be reachable (max 120 s) ─────────────────────────
set(MAX_WAIT_S 120)
set(SLEEP_S    2)
set(WAITED     0)
set(PORT_OPEN  FALSE)

while(NOT PORT_OPEN AND WAITED LESS MAX_WAIT_S)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E sleep ${SLEEP_S}
    )
    math(EXPR WAITED "${WAITED} + ${SLEEP_S}")

    # Use docker inspect to check container health / running state
    execute_process(
        COMMAND docker inspect --format={{.State.Running}} ${CONTAINER_NAME}
        OUTPUT_VARIABLE RUNNING_STATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(RUNNING_STATE STREQUAL "true")
        # Also probe the port via a short nc / curl / PowerShell attempt
        if(CMAKE_HOST_WIN32)
            execute_process(
                COMMAND powershell -NonInteractive -Command
                    "try { $t = New-Object Net.Sockets.TcpClient('127.0.0.1', ${HOST_PORT}); $t.Close(); exit 0 } catch { exit 1 }"
                RESULT_VARIABLE PORT_RC
                OUTPUT_QUIET ERROR_QUIET
            )
        else()
            execute_process(
                COMMAND bash -c "echo > /dev/tcp/127.0.0.1/${HOST_PORT}"
                RESULT_VARIABLE PORT_RC
                OUTPUT_QUIET ERROR_QUIET
            )
        endif()
        if(PORT_RC EQUAL 0)
            set(PORT_OPEN TRUE)
        endif()
    endif()

    if(NOT PORT_OPEN)
        message(STATUS "DockerRun: waiting for ${DB} on port ${HOST_PORT} (${WAITED}s elapsed)…")
    endif()
endwhile()

if(NOT PORT_OPEN)
    execute_process(COMMAND docker rm -f ${CONTAINER_NAME} OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "DockerRun: timed out waiting for ${DB} to become ready on port ${HOST_PORT}")
endif()

# Extra grace period for DB initialisation scripts to finish
execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 3)

# ── 4. Run the test binary ────────────────────────────────────────────────────
# Build the environment variable list for the test process (uppercased DB name).
string(TOUPPER "${DB}" DB_UPPER)
set(TEST_ENV
    "ORM_${DB_UPPER}_HOST=127.0.0.1"
    "ORM_${DB_UPPER}_PORT=${HOST_PORT}"
)

if(DEFINED EXTRA_ENV_ARGS)
    list(APPEND TEST_ENV ${EXTRA_ENV_ARGS})
endif()

# On Windows, prepend vcpkg_installed bin dirs to PATH so the test binary can
# find runtime DLLs (libpq.dll, libssl-3-x64.dll, libmysql.dll, etc.).
if(CMAKE_HOST_WIN32)
    # Locate the test binary's directory to find the project root heuristically.
    get_filename_component(TEST_BINARY_DIR "${TEST_BINARY}" DIRECTORY)
    # Walk up to find vcpkg_installed relative to the binary (build is inside project root).
    get_filename_component(BUILD_ROOT "${TEST_BINARY_DIR}" DIRECTORY)
    get_filename_component(BUILD_ROOT "${BUILD_ROOT}" DIRECTORY)
    get_filename_component(PROJECT_ROOT "${BUILD_ROOT}" DIRECTORY)
    set(_VCPKG_BIN_CANDIDATES
        "${PROJECT_ROOT}/vcpkg_installed/x64-windows/bin"
        "${BUILD_ROOT}/vcpkg_installed/x64-windows/bin"
    )
    set(_VCPKG_BIN "")
    foreach(_candidate IN LISTS _VCPKG_BIN_CANDIDATES)
        if(EXISTS "${_candidate}")
            set(_VCPKG_BIN "${_candidate}")
            break()
        endif()
    endforeach()
    if(_VCPKG_BIN)
        # Prepend to PATH in the child environment.
        list(APPEND TEST_ENV "PATH=${_VCPKG_BIN};$ENV{PATH}")
        message(STATUS "DockerRun: prepending ${_VCPKG_BIN} to PATH for DLL resolution")
    endif()
endif()

execute_process(
    COMMAND ${TEST_BINARY}
    ENVIRONMENT ${TEST_ENV}
    OUTPUT_VARIABLE TEST_STDOUT
    ERROR_VARIABLE  TEST_STDERR
    RESULT_VARIABLE TEST_RC
)

# Always print captured output so failures are diagnosable.
if(TEST_STDOUT)
    message(STATUS "DockerRun [${DB}] stdout:\n${TEST_STDOUT}")
endif()
if(TEST_STDERR)
    message(STATUS "DockerRun [${DB}] stderr:\n${TEST_STDERR}")
endif()

# ── 5. Tear down the container ────────────────────────────────────────────────
execute_process(COMMAND docker rm -f ${CONTAINER_NAME} OUTPUT_QUIET ERROR_QUIET)

# ── 6. Propagate the test exit code ──────────────────────────────────────────
if(NOT TEST_RC EQUAL 0)
    message(FATAL_ERROR "DockerRun: test binary exited with code ${TEST_RC}")
endif()
