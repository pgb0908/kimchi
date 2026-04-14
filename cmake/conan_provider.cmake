include(cmake/conan_version.cmake)
include(cmake/conan_profile_detect.cmake)
include(cmake/conan_host_profile.cmake)
include(cmake/conan_install.cmake)

set(CONAN_MINIMUM_VERSION 2.0.5)
cmake_policy(PUSH)
cmake_minimum_required(VERSION 3.24)


## cmake 상에서 find_package를 찾는 구문
macro(conan_provide_dependency_check)
    set(_CONAN_PROVIDE_DEPENDENCY_INVOKED FALSE)
    get_property(_CONAN_PROVIDE_DEPENDENCY_INVOKED GLOBAL PROPERTY CONAN_PROVIDE_DEPENDENCY_INVOKED)
    if(NOT _CONAN_PROVIDE_DEPENDENCY_INVOKED)
        message(WARNING "Conan is correctly configured as dependency provider, "
                "but Conan has not been invoked. Please add at least one "
                "call to `find_package()`.")
        if(DEFINED CONAN_COMMAND)
            # supress warning in case `CONAN_COMMAND` was specified but unused.
            set(_CONAN_COMMAND ${CONAN_COMMAND})
            unset(_CONAN_COMMAND)
        endif()
    endif()
    unset(_CONAN_PROVIDE_DEPENDENCY_INVOKED)
endmacro()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL conan_provide_dependency_check)


macro(construct_profile_argument argument_variable profile_list)
    set(${argument_variable} "")
    if("${profile_list}" STREQUAL "CONAN_HOST_PROFILE")
        set(_arg_flag "--profile:host=")
    elseif("${profile_list}" STREQUAL "CONAN_BUILD_PROFILE")
        set(_arg_flag "--profile:build=")
    endif()

    set(_profile_list "${${profile_list}}")
    list(TRANSFORM _profile_list REPLACE "auto-cmake" "${CMAKE_BINARY_DIR}/conan_host_profile")
    list(TRANSFORM _profile_list PREPEND ${_arg_flag})
    set(${argument_variable} ${_profile_list})

    unset(_arg_flag)
    unset(_profile_list)
endmacro()


macro(conan_provide_dependency method package_name)
    set_property(GLOBAL PROPERTY CONAN_PROVIDE_DEPENDENCY_INVOKED TRUE)
    get_property(_conan_install_success GLOBAL PROPERTY CONAN_INSTALL_SUCCESS)
    if(NOT _conan_install_success)
        ## 코난 버전을 가져와서 버전에 대한 validation 체크
        find_program(CONAN_COMMAND "conan" REQUIRED)
        conan_get_version(${CONAN_COMMAND} CONAN_CURRENT_VERSION)
        conan_version_check(MINIMUM ${CONAN_MINIMUM_VERSION} CURRENT ${CONAN_CURRENT_VERSION})

        message(STATUS "CMake-Conan: first find_package() found. Installing dependencies with Conan")
        if("default" IN_LIST CONAN_HOST_PROFILE OR "default" IN_LIST CONAN_BUILD_PROFILE)
            conan_profile_detect_default()
        endif()
        if("auto-cmake" IN_LIST CONAN_HOST_PROFILE)
            detect_host_profile(${CMAKE_BINARY_DIR}/conan_host_profile)
        endif()
        construct_profile_argument(_host_profile_flags CONAN_HOST_PROFILE)
        construct_profile_argument(_build_profile_flags CONAN_BUILD_PROFILE)
        if(EXISTS "${CMAKE_SOURCE_DIR}/conanfile.py")
            file(READ "${CMAKE_SOURCE_DIR}/conanfile.py" outfile)
            if(NOT "${outfile}" MATCHES ".*CMakeDeps.*")
                message(WARNING "Cmake-conan: CMakeDeps generator was not defined in the conanfile")
            endif()
            set(generator "")
        elseif (EXISTS "${CMAKE_SOURCE_DIR}/conanfile.txt")
            file(READ "${CMAKE_SOURCE_DIR}/conanfile.txt" outfile)
            if(NOT "${outfile}" MATCHES ".*CMakeDeps.*")
                message(WARNING "Cmake-conan: CMakeDeps generator was not defined in the conanfile. "
                        "Please define the generator as it will be mandatory in the future")
            endif()
            set(generator "-g;CMakeDeps")
        endif()
        get_property(_multiconfig_generator GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
        if(NOT _multiconfig_generator)
            message(STATUS "CMake-Conan: Installing single configuration ${CMAKE_BUILD_TYPE}")
            conan_install(${_host_profile_flags} ${_build_profile_flags} ${CONAN_INSTALL_ARGS} ${generator})
        else()
            message(STATUS "CMake-Conan: Installing both Debug and Release")
            conan_install(${_host_profile_flags} ${_build_profile_flags} -s build_type=Release ${CONAN_INSTALL_ARGS} ${generator})
            conan_install(${_host_profile_flags} ${_build_profile_flags} -s build_type=Debug ${CONAN_INSTALL_ARGS} ${generator})
        endif()
        unset(_host_profile_flags)
        unset(_build_profile_flags)
        unset(_multiconfig_generator)
        unset(_conan_install_success)
    else()
        message(STATUS "CMake-Conan: find_package(${ARGV1}) found, 'conan install' already ran")
        unset(_conan_install_success)
    endif()

    get_property(_conan_generators_folder GLOBAL PROPERTY CONAN_GENERATORS_FOLDER)

    # Ensure that we consider Conan-provided packages ahead of any other,
    # irrespective of other settings that modify the search order or search paths
    # This follows the guidelines from the find_package documentation
    #  (https://cmake.org/cmake/help/latest/command/find_package.html):
    #       find_package (<PackageName> PATHS paths... NO_DEFAULT_PATH)
    #       find_package (<PackageName>)

    # Filter out `REQUIRED` from the argument list, as the first call may fail
    set(_find_args_${package_name} "${ARGN}")
    list(REMOVE_ITEM _find_args_${package_name} "REQUIRED")
    if(NOT "MODULE" IN_LIST _find_args_${package_name})
        find_package(${package_name} ${_find_args_${package_name}} BYPASS_PROVIDER PATHS "${_conan_generators_folder}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        unset(_find_args_${package_name})
    endif()

    # Invoke find_package a second time - if the first call succeeded,
    # this will simply reuse the result. If not, fall back to CMake default search
    # behaviour, also allowing modules to be searched.
    if(NOT ${package_name}_FOUND)
        list(FIND CMAKE_MODULE_PATH "${_conan_generators_folder}" _index)
        if(_index EQUAL -1)
            list(PREPEND CMAKE_MODULE_PATH "${_conan_generators_folder}")
        endif()
        unset(_index)
        find_package(${package_name} ${ARGN} BYPASS_PROVIDER)
        list(REMOVE_ITEM CMAKE_MODULE_PATH "${_conan_generators_folder}")
    endif()
endmacro()
cmake_language(
        SET_DEPENDENCY_PROVIDER conan_provide_dependency
        SUPPORTED_METHODS FIND_PACKAGE
)


message(STATUS "TEST")
set(CONAN_HOST_PROFILE "default;auto-cmake" CACHE STRING "Conan host profile")
set(CONAN_BUILD_PROFILE "default" CACHE STRING "Conan build profile")
set(CONAN_INSTALL_ARGS "--build=missing" CACHE STRING "Command line arguments for conan install")

cmake_policy(POP)