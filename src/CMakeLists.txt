find_package(proxygen REQUIRED)
find_package(jwt-cpp REQUIRED)

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

target_include_directories(kimchi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kimchi PRIVATE
    proxygen::proxygen
    proxygen::proxygenhttpserver
    jwt-cpp::jwt-cpp
)
