cmake_minimum_required(VERSION 3.24)
set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/conan_provider.cmake") # 가장 프로젝트 상위에 존재해야함
set(CONAN_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/conan/bin/conan")
if(NOT EXISTS "${CONAN_COMMAND}")
    message(STATUS "Conan not found. Running scripts/install_conan.sh...")
    execute_process(
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_conan.sh"
        RESULT_VARIABLE _conan_install_result
    )
    if(NOT _conan_install_result EQUAL 0)
        message(FATAL_ERROR "Conan install failed. Run scripts/install_conan.sh manually.")
    endif()
endif()

# 레시피 export (idempotent, 수초 소요)
execute_process(
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/export_recipes.sh"
    RESULT_VARIABLE _recipes_result
)
if(NOT _recipes_result EQUAL 0)
    message(FATAL_ERROR "Recipe export failed. Run scripts/export_recipes.sh manually.")
endif()


project(kimchi C CXX)

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
        -std=c++17
)


string (REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -D_DEBUG -DGOOGLE_STRIP_LOG=0")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -ggdb -DNDEBUG -DGOOGLE_STRIP_LOG=1")
SET(DEPENDENT_LIBRARIES event glog pthread)

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
option(BUILD_TESTING "Build tests" ON)
message (STATUS "Build tests " ${BUILD_TESTING})


add_subdirectory(src)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include (CTest)
    add_subdirectory(test)
endif()