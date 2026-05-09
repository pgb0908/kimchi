cmake_minimum_required(VERSION 3.24)
project(kimchi C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set (CXX_FLAGS
        -g
        -fPIC
        -Wall
        -D__STDC_FORMAT_MACROS
        -Wno-unused-parameter
        -Wno-unused-function
        -Wunused-variable
        -Wunused-value
        -Wshadow
        -Wcast-qual
        -Wcast-align
        -Wwrite-strings
        -Wsign-compare
        -Winvalid-pch
        -fms-extensions
        -Wfloat-equal
        -Wextra
        -std=gnu++20
)


string (REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

find_package(Threads REQUIRED)

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -D_DEBUG -DGOOGLE_STRIP_LOG=0")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -ggdb -DNDEBUG -DGOOGLE_STRIP_LOG=1")

set(SANITIZE "" CACHE STRING "Sanitizer to enable in Debug. One of: '', address, thread")
set_property(CACHE SANITIZE PROPERTY STRINGS "" address thread)
if (SANITIZE STREQUAL "address")
    add_compile_options(
            $<$<CONFIG:Debug>:-fsanitize=address>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    add_link_options(
            $<$<CONFIG:Debug>:-fsanitize=address>
    )
elseif (SANITIZE STREQUAL "thread")
    add_compile_options(
            $<$<CONFIG:Debug>:-fsanitize=thread>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    add_link_options(
            $<$<CONFIG:Debug>:-fsanitize=thread>
    )
endif ()


message (STATUS "This is BINARY dir " ${PROJECT_BINARY_DIR})
message (STATUS "This is SOURCE dir " ${PROJECT_SOURCE_DIR})
message (STATUS "CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message(STATUS "Build type ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "CMAKE_CXX_FLAGS_DEBUG " ${CMAKE_CXX_FLAGS_DEBUG})
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "CMAKE_CXX_FLAGS_RELEASE " ${CMAKE_CXX_FLAGS_RELEASE})
endif()
message (STATUS "EXECUTABLE_OUTPUT_PATH " ${EXECUTABLE_OUTPUT_PATH})
message (STATUS "LIBRARY_OUTPUT_PATH " ${LIBRARY_OUTPUT_PATH})
option(BUILD_TESTING "Build tests" OFF)
message (STATUS "Build tests " ${BUILD_TESTING})


add_subdirectory(src)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include (CTest)
    add_subdirectory(test)
endif()
