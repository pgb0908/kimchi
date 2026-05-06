find_package(folly REQUIRED)
find_package(fizz REQUIRED)
find_package(wangle REQUIRED)
find_package(mvfst REQUIRED)
find_package(proxygen REQUIRED)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(fmt REQUIRED)

set(KIMCHI_SOURCES
    main.cpp
    config/loader.cpp
    server/gateway_server.cpp
    server/admin_server.cpp
    handler/gateway_handler.cpp
    handler/gateway_handler_factory.cpp
    handler/admin_handler.cpp
    handler/admin_handler_factory.cpp
)

add_executable(kimchi ${KIMCHI_SOURCES})

target_include_directories(kimchi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(kimchi PRIVATE
    proxygen::proxygen
    proxygen::proxygenhttpserver
    Folly::folly
    glog::glog
    gflags::gflags
    fmt::fmt
)
