# folly/wangle/fizz/mvfst use the "builddirs + native cmake config" pattern.
# Their Conan-generated wrapper targets omit include dirs and miss component libs
# (e.g. libfolly_demangle). We bypass the wrappers and load native configs directly
# from CMAKE_MODULE_PATH (which Conan toolchain populates), skipping the generators dir.
set(_conan_generators_dir "${CMAKE_BINARY_DIR}/conan/build/${CMAKE_BUILD_TYPE}/generators")
message(STATUS "[DBG] generators_dir=${_conan_generators_dir}")
message(STATUS "[DBG] MODULE_PATH=${CMAKE_MODULE_PATH}")

foreach(_meta_pkg folly wangle fizz mvfst)
    set(_native_dir "")
    foreach(_path ${CMAKE_MODULE_PATH})
        if("${_path}" STREQUAL "${_conan_generators_dir}")
            continue()
        endif()
        if(EXISTS "${_path}/${_meta_pkg}-config.cmake")
            set(_native_dir "${_path}")
            break()
        endif()
    endforeach()

    if(_native_dir)
        message(STATUS "Using native cmake config for ${_meta_pkg}: ${_native_dir}")
        # BYPASS_PROVIDER skips conan_provide_dependency which would otherwise
        # force the search into the generators folder and load the broken wrapper.
        find_package(${_meta_pkg} CONFIG
            PATHS "${_native_dir}" NO_DEFAULT_PATH BYPASS_PROVIDER REQUIRED)
    else()
        find_package(${_meta_pkg} REQUIRED)
    endif()
endforeach()

find_package(proxygen REQUIRED)
find_package(c-ares REQUIRED)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)

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
    wangle::wangle
    fizz::fizz
    mvfst::mvfst
    c-ares::cares
    Folly::folly
    glog::glog
    gflags::gflags
)
