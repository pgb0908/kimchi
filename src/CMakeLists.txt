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

string(TOUPPER "${CMAKE_BUILD_TYPE}" KIMCHI_BUILD_TYPE_UPPER)

target_include_directories(kimchi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

foreach(_pkg folly proxygen wangle fizz mvfst glog gflags fmt)
    set(_pkg_folder_var "${_pkg}_PACKAGE_FOLDER_${KIMCHI_BUILD_TYPE_UPPER}")
    if(DEFINED ${_pkg_folder_var} AND EXISTS "${${_pkg_folder_var}}/include")
        target_include_directories(kimchi PRIVATE "${${_pkg_folder_var}}/include")
    endif()
endforeach()

set(KIMCHI_FOLLY_DEMANGLE_LIB "")
if(DEFINED folly_PACKAGE_FOLDER_${KIMCHI_BUILD_TYPE_UPPER})
    set(KIMCHI_FOLLY_DEMANGLE_LIB
        "${folly_PACKAGE_FOLDER_${KIMCHI_BUILD_TYPE_UPPER}}/lib/libfolly_demangle.a")
endif()

find_library(KIMCHI_LIBIBERTY NAMES iberty)

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

if(EXISTS "${KIMCHI_FOLLY_DEMANGLE_LIB}" AND KIMCHI_LIBIBERTY)
    target_link_libraries(kimchi PRIVATE
        "$<LINK_GROUP:RESCAN,Folly::folly,${KIMCHI_FOLLY_DEMANGLE_LIB},${KIMCHI_LIBIBERTY}>"
    )
endif()
