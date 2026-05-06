find_package(proxygen REQUIRED)

set(KIMCHI_SOURCES
    main.cpp
    config/loader.cpp
    server/gateway_server.cpp
    server/admin_server.cpp
    handler/gateway_handler.cpp
    handler/gateway_handler_factory.cpp
    handler/admin_handler.cpp
    handler/admin_handler_factory.cpp
    filter/auth_filter.cpp
    filter/header_filter.cpp
    filter/rate_limit_filter.cpp
)

add_executable(kimchi ${KIMCHI_SOURCES})

target_include_directories(kimchi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kimchi PRIVATE
    proxygen::proxygen
    proxygen::proxygenhttpserver
)
