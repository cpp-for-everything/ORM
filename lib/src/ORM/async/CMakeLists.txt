# ── ORM async infrastructure ──────────────────────────────────────────────────
# Provides Task<T>, CancellationToken, IoContext with platform-specific backends.

set(ORM_ASYNC_SOURCES "")

if(APPLE)
    list(APPEND ORM_ASYNC_SOURCES kqueue_context.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND ORM_ASYNC_SOURCES uring_context.cpp)
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
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(URING REQUIRED liburing)
        target_link_libraries(orm_async PUBLIC ${URING_LIBRARIES})
        target_include_directories(orm_async PUBLIC ${URING_INCLUDE_DIRS})
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
