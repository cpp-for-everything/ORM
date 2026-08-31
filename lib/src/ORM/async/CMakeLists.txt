# ── ORM async infrastructure ──────────────────────────────────────────────────
# Provides Task<T>, CancellationToken, IoContext with platform-specific backends.
#
# The reactor backend is selected per platform:
#   macOS   → kqueue_context.cpp (BSD kqueue, EV_ONESHOT readiness)
#   Windows → iocp_context.cpp   (WSAPoll-based readiness reactor)
#   Linux   → either uring_context.cpp (io_uring) or epoll_context.cpp,
#            controlled by ORM_LINUX_REACTOR:
#              auto  – prefer io_uring if liburing is installed, else epoll
#              uring – io_uring (errors if liburing not found)
#              epoll – plain epoll(7) with eventfd wakeup

set(ORM_LINUX_REACTOR "auto" CACHE STRING
    "Linux reactor backend for orm::async: auto|uring|epoll")
set_property(CACHE ORM_LINUX_REACTOR PROPERTY STRINGS auto uring epoll)

# Always compile the final-suspend destroy helper out of line (see task.hpp).
set(ORM_ASYNC_SOURCES task_final_suspend.cpp)
set(ORM_ASYNC_LINUX_REACTOR "")

if(APPLE)
    list(APPEND ORM_ASYNC_SOURCES kqueue_context.cpp)
elseif(UNIX AND NOT APPLE)
    set(_orm_have_liburing FALSE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(URING QUIET liburing)
        if(URING_FOUND)
            set(_orm_have_liburing TRUE)
        endif()
    endif()

    if(ORM_LINUX_REACTOR STREQUAL "uring")
        if(NOT _orm_have_liburing)
            message(FATAL_ERROR
                "ORM_LINUX_REACTOR=uring requested but liburing not found "
                "(install liburing-dev / liburing-devel).")
        endif()
        set(ORM_ASYNC_LINUX_REACTOR "uring")
    elseif(ORM_LINUX_REACTOR STREQUAL "epoll")
        set(ORM_ASYNC_LINUX_REACTOR "epoll")
    else() # auto
        if(_orm_have_liburing)
            set(ORM_ASYNC_LINUX_REACTOR "uring")
        else()
            set(ORM_ASYNC_LINUX_REACTOR "epoll")
        endif()
    endif()

    if(ORM_ASYNC_LINUX_REACTOR STREQUAL "uring")
        list(APPEND ORM_ASYNC_SOURCES uring_context.cpp)
        message(STATUS "ORM: Linux reactor = io_uring (liburing ${URING_VERSION})")
    else()
        list(APPEND ORM_ASYNC_SOURCES epoll_context.cpp)
        message(STATUS "ORM: Linux reactor = epoll")
    endif()
elseif(WIN32)
    list(APPEND ORM_ASYNC_SOURCES iocp_context.cpp)
endif()

if(ORM_ASYNC_SOURCES)
    add_library(orm_async STATIC ${ORM_ASYNC_SOURCES})
else()
    # Fallback: header-only for unsupported platforms
    add_library(orm_async INTERFACE)
endif()

add_library(orm::async ALIAS orm_async)

if(ORM_ASYNC_SOURCES)
    target_include_directories(orm_async PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(orm_async PUBLIC orm::orm)
    target_compile_features(orm_async PUBLIC cxx_std_23)

    # Platform-specific link deps
    if(UNIX AND NOT APPLE AND ORM_ASYNC_LINUX_REACTOR STREQUAL "uring")
        target_compile_definitions(orm_async PUBLIC ORM_HAS_LIBURING=1)
        target_link_libraries(orm_async PUBLIC ${URING_LIBRARIES})
        target_include_directories(orm_async PUBLIC ${URING_INCLUDE_DIRS})
        target_link_directories(orm_async PUBLIC ${URING_LIBRARY_DIRS})
    elseif(WIN32)
        target_link_libraries(orm_async PUBLIC ws2_32 mswsock)
    endif()
else()
    target_include_directories(orm_async INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/include>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(orm_async INTERFACE orm::orm)
    target_compile_features(orm_async INTERFACE cxx_std_23)
endif()
