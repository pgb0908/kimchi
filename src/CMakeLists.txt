find_package(proxygen REQUIRED)
find_package(jwt-cpp REQUIRED)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(Libevent REQUIRED)
find_package(c-ares REQUIRED)

set(KIMCHI_SOURCES
    main.cpp
    config/loader.cpp
    server/gateway_server.cpp
    server/admin_server.cpp
    handler/gateway_handler.cpp
    handler/gateway_handler_factory.cpp
    handler/upstream_client.cpp
    handler/admin_handler.cpp
    handler/admin_handler_factory.cpp
    filter/header_filter.cpp
    filter/rate_limit_filter.cpp
    policy/jwks_cache.cpp
    policy/jwt_policy.cpp
    policy/policy_engine.cpp
)

add_executable(kimchi ${KIMCHI_SOURCES})

set(VCPKG_LIB_DIR ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/$<IF:$<CONFIG:Debug>,debug/lib,lib>)

target_include_directories(kimchi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_directories(kimchi PRIVATE ${VCPKG_LIB_DIR})

target_link_libraries(kimchi PRIVATE
    proxygen::proxygen
    proxygen::proxygenhttpserver
    jwt-cpp::jwt-cpp
    glog::glog
    gflags::gflags
    libevent::core
    c-ares::cares
    Threads::Threads
    # wangle port의 cmake config가 folly fdsock 의존성을 누락해서 직접 명시
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,${VCPKG_LIB_DIR}/libfolly_io_async_fdsock_async_fd_socket.a>"
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,${VCPKG_LIB_DIR}/libfolly_io_async_fdsock_socket_fds.a>"
)
