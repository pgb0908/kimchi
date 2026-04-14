
function(detect_os OS OS_API_LEVEL OS_SDK OS_SUBSYSTEM OS_VERSION)
    # it could be cross compilation
    message(STATUS "CMake-Conan: cmake_system_name=${CMAKE_SYSTEM_NAME}")
    if(CMAKE_SYSTEM_NAME AND NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(${OS} Macos PARENT_SCOPE)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
            set(${OS} Neutrino PARENT_SCOPE)
        elseif(CMAKE_SYSTEM_NAME STREQUAL "CYGWIN")
            set(${OS} Windows PARENT_SCOPE)
            set(${OS_SUBSYSTEM} cygwin PARENT_SCOPE)
        elseif(CMAKE_SYSTEM_NAME MATCHES "^MSYS")
            set(${OS} Windows PARENT_SCOPE)
            set(${OS_SUBSYSTEM} msys2 PARENT_SCOPE)
        else()
            set(${OS} ${CMAKE_SYSTEM_NAME} PARENT_SCOPE)
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Android")
            if(DEFINED ANDROID_PLATFORM)
                string(REGEX MATCH "[0-9]+" _OS_API_LEVEL ${ANDROID_PLATFORM})
            elseif(DEFINED CMAKE_SYSTEM_VERSION)
                set(_OS_API_LEVEL ${CMAKE_SYSTEM_VERSION})
            endif()
            message(STATUS "CMake-Conan: android api level=${_OS_API_LEVEL}")
            set(${OS_API_LEVEL} ${_OS_API_LEVEL} PARENT_SCOPE)
        endif()
        if(CMAKE_SYSTEM_NAME MATCHES "Darwin|iOS|tvOS|watchOS")
            # CMAKE_OSX_SYSROOT contains the full path to the SDK for MakeFile/Ninja
            # generators, but just has the original input string for Xcode.
            if(NOT IS_DIRECTORY ${CMAKE_OSX_SYSROOT})
                set(_OS_SDK ${CMAKE_OSX_SYSROOT})
            else()
                if(CMAKE_OSX_SYSROOT MATCHES Simulator)
                    set(apple_platform_suffix simulator)
                else()
                    set(apple_platform_suffix os)
                endif()
                if(CMAKE_OSX_SYSROOT MATCHES AppleTV)
                    set(_OS_SDK "appletv${apple_platform_suffix}")
                elseif(CMAKE_OSX_SYSROOT MATCHES iPhone)
                    set(_OS_SDK "iphone${apple_platform_suffix}")
                elseif(CMAKE_OSX_SYSROOT MATCHES Watch)
                    set(_OS_SDK "watch${apple_platform_suffix}")
                endif()
            endif()
            if(DEFINED _OS_SDK)
                message(STATUS "CMake-Conan: cmake_osx_sysroot=${CMAKE_OSX_SYSROOT}")
                set(${OS_SDK} ${_OS_SDK} PARENT_SCOPE)
            endif()
            if(DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
                message(STATUS "CMake-Conan: cmake_osx_deployment_target=${CMAKE_OSX_DEPLOYMENT_TARGET}")
                set(${OS_VERSION} ${CMAKE_OSX_DEPLOYMENT_TARGET} PARENT_SCOPE)
            endif()
        endif()
    endif()
endfunction()


function(detect_arch ARCH)
    # CMAKE_OSX_ARCHITECTURES can contain multiple architectures, but Conan only supports one.
    # Therefore this code only finds one. If the recipes support multiple architectures, the
    # build will work. Otherwise, there will be a linker error for the missing architecture(s).
    if(DEFINED CMAKE_OSX_ARCHITECTURES)
        string(REPLACE " " ";" apple_arch_list "${CMAKE_OSX_ARCHITECTURES}")
        list(LENGTH apple_arch_list apple_arch_count)
        if(apple_arch_count GREATER 1)
            message(WARNING "CMake-Conan: Multiple architectures detected, this will only work if Conan recipe(s) produce fat binaries.")
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin|iOS|tvOS|watchOS" AND NOT CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(host_arch ${CMAKE_OSX_ARCHITECTURES})
    elseif(MSVC)
        set(host_arch ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID})
    else()
        set(host_arch ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    if(host_arch MATCHES "aarch64|arm64|ARM64")
        set(_ARCH armv8)
    elseif(host_arch MATCHES "armv7|armv7-a|armv7l|ARMV7")
        set(_ARCH armv7)
    elseif(host_arch MATCHES armv7s)
        set(_ARCH armv7s)
    elseif(host_arch MATCHES "i686|i386|X86")
        set(_ARCH x86)
    elseif(host_arch MATCHES "AMD64|amd64|x86_64|x64")
        set(_ARCH x86_64)
    endif()
    message(STATUS "CMake-Conan: cmake_system_processor=${_ARCH}")
    set(${ARCH} ${_ARCH} PARENT_SCOPE)
endfunction()


function(detect_cxx_standard CXX_STANDARD)
    set(${CXX_STANDARD} ${CMAKE_CXX_STANDARD} PARENT_SCOPE)
    if(CMAKE_CXX_EXTENSIONS)
        set(${CXX_STANDARD} "gnu${CMAKE_CXX_STANDARD}" PARENT_SCOPE)
    endif()
endfunction()


macro(detect_gnu_libstdcxx)
    # _CONAN_IS_GNU_LIBSTDCXX true if GNU libstdc++
    check_cxx_source_compiles("
    #include <cstddef>
    #if !defined(__GLIBCXX__) && !defined(__GLIBCPP__)
    static_assert(false);
    #endif
    int main(){}" _CONAN_IS_GNU_LIBSTDCXX)

    # _CONAN_GNU_LIBSTDCXX_IS_CXX11_ABI true if C++11 ABI
    check_cxx_source_compiles("
    #include <string>
    static_assert(sizeof(std::string) != sizeof(void*), \"using libstdc++\");
    int main () {}" _CONAN_GNU_LIBSTDCXX_IS_CXX11_ABI)

    set(_CONAN_GNU_LIBSTDCXX_SUFFIX "")
    if(_CONAN_GNU_LIBSTDCXX_IS_CXX11_ABI)
        set(_CONAN_GNU_LIBSTDCXX_SUFFIX "11")
    endif()
    unset (_CONAN_GNU_LIBSTDCXX_IS_CXX11_ABI)
endmacro()


macro(detect_libcxx)
    # _CONAN_IS_LIBCXX true if LLVM libc++
    check_cxx_source_compiles("
    #include <cstddef>
    #if !defined(_LIBCPP_VERSION)
       static_assert(false);
    #endif
    int main(){}" _CONAN_IS_LIBCXX)
endmacro()


function(detect_lib_cxx LIB_CXX)
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        message(STATUS "CMake-Conan: android_stl=${CMAKE_ANDROID_STL_TYPE}")
        set(${LIB_CXX} ${CMAKE_ANDROID_STL_TYPE} PARENT_SCOPE)
        return()
    endif()

    include(CheckCXXSourceCompiles)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        detect_gnu_libstdcxx()
        set(${LIB_CXX} "libstdc++${_CONAN_GNU_LIBSTDCXX_SUFFIX}" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        set(${LIB_CXX} "libc++" PARENT_SCOPE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
        # Check for libc++
        detect_libcxx()
        if(_CONAN_IS_LIBCXX)
            set(${LIB_CXX} "libc++" PARENT_SCOPE)
            return()
        endif()

        # Check for libstdc++
        detect_gnu_libstdcxx()
        if(_CONAN_IS_GNU_LIBSTDCXX)
            set(${LIB_CXX} "libstdc++${_CONAN_GNU_LIBSTDCXX_SUFFIX}" PARENT_SCOPE)
            return()
        endif()

        # TODO: it would be an error if we reach this point
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # Do nothing - compiler.runtime and compiler.runtime_type
        # should be handled separately: https://github.com/conan-io/cmake-conan/pull/516
        return()
    else()
        # TODO: unable to determine, ask user to provide a full profile file instead
    endif()
endfunction()


function(detect_compiler COMPILER COMPILER_VERSION COMPILER_RUNTIME COMPILER_RUNTIME_TYPE)
    if(DEFINED CMAKE_CXX_COMPILER_ID)
        set(_COMPILER ${CMAKE_CXX_COMPILER_ID})
        set(_COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION})
    else()
        if(NOT DEFINED CMAKE_C_COMPILER_ID)
            message(FATAL_ERROR "C or C++ compiler not defined")
        endif()
        set(_COMPILER ${CMAKE_C_COMPILER_ID})
        set(_COMPILER_VERSION ${CMAKE_C_COMPILER_VERSION})
    endif()

    message(STATUS "CMake-Conan: CMake compiler=${_COMPILER}")
    message(STATUS "CMake-Conan: CMake compiler version=${_COMPILER_VERSION}")

    if(_COMPILER MATCHES MSVC)
        set(_COMPILER "msvc")
        string(SUBSTRING ${MSVC_VERSION} 0 3 _COMPILER_VERSION)
        # Configure compiler.runtime and compiler.runtime_type settings for MSVC
        if(CMAKE_MSVC_RUNTIME_LIBRARY)
            set(_msvc_runtime_library ${CMAKE_MSVC_RUNTIME_LIBRARY})
        else()
            set(_msvc_runtime_library MultiThreaded$<$<CONFIG:Debug>:Debug>DLL) # default value documented by CMake
        endif()

        set(_KNOWN_MSVC_RUNTIME_VALUES "")
        list(APPEND _KNOWN_MSVC_RUNTIME_VALUES MultiThreaded MultiThreadedDLL)
        list(APPEND _KNOWN_MSVC_RUNTIME_VALUES MultiThreadedDebug MultiThreadedDebugDLL)
        list(APPEND _KNOWN_MSVC_RUNTIME_VALUES MultiThreaded$<$<CONFIG:Debug>:Debug> MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)

        # only accept the 6 possible values, otherwise we don't don't know to map this
        if(NOT _msvc_runtime_library IN_LIST _KNOWN_MSVC_RUNTIME_VALUES)
            message(FATAL_ERROR "CMake-Conan: unable to map MSVC runtime: ${_msvc_runtime_library} to Conan settings")
        endif()

        # Runtime is "dynamic" in all cases if it ends in DLL
        if(_msvc_runtime_library MATCHES ".*DLL$")
            set(_COMPILER_RUNTIME "dynamic")
        else()
            set(_COMPILER_RUNTIME "static")
        endif()
        message(STATUS "CMake-Conan: CMake compiler.runtime=${_COMPILER_RUNTIME}")

        # Only define compiler.runtime_type when explicitly requested
        # If a generator expression is used, let Conan handle it conditional on build_type
        if(NOT _msvc_runtime_library MATCHES "<CONFIG:Debug>:Debug>")
            if(_msvc_runtime_library MATCHES "Debug")
                set(_COMPILER_RUNTIME_TYPE "Debug")
            else()
                set(_COMPILER_RUNTIME_TYPE "Release")
            endif()
            message(STATUS "CMake-Conan: CMake compiler.runtime_type=${_COMPILER_RUNTIME_TYPE}")
        endif()

        unset(_KNOWN_MSVC_RUNTIME_VALUES)

    elseif(_COMPILER MATCHES AppleClang)
        set(_COMPILER "apple-clang")
        string(REPLACE "." ";" VERSION_LIST ${CMAKE_CXX_COMPILER_VERSION})
        list(GET VERSION_LIST 0 _COMPILER_VERSION)
    elseif(_COMPILER MATCHES Clang)
        set(_COMPILER "clang")
        string(REPLACE "." ";" VERSION_LIST ${CMAKE_CXX_COMPILER_VERSION})
        list(GET VERSION_LIST 0 _COMPILER_VERSION)
    elseif(_COMPILER MATCHES GNU)
        set(_COMPILER "gcc")
        string(REPLACE "." ";" VERSION_LIST ${CMAKE_CXX_COMPILER_VERSION})
        list(GET VERSION_LIST 0 _COMPILER_VERSION)
    endif()

    message(STATUS "CMake-Conan: [settings] compiler=${_COMPILER}")
    message(STATUS "CMake-Conan: [settings] compiler.version=${_COMPILER_VERSION}")
    if (_COMPILER_RUNTIME)
        message(STATUS "CMake-Conan: [settings] compiler.runtime=${_COMPILER_RUNTIME}")
    endif()
    if (_COMPILER_RUNTIME_TYPE)
        message(STATUS "CMake-Conan: [settings] compiler.runtime_type=${_COMPILER_RUNTIME_TYPE}")
    endif()

    set(${COMPILER} ${_COMPILER} PARENT_SCOPE)
    set(${COMPILER_VERSION} ${_COMPILER_VERSION} PARENT_SCOPE)
    set(${COMPILER_RUNTIME} ${_COMPILER_RUNTIME} PARENT_SCOPE)
    set(${COMPILER_RUNTIME_TYPE} ${_COMPILER_RUNTIME_TYPE} PARENT_SCOPE)
endfunction()


function(detect_build_type BUILD_TYPE)
    get_property(_MULTICONFIG_GENERATOR GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(NOT _MULTICONFIG_GENERATOR)
        # Only set when we know we are in a single-configuration generator
        # Note: we may want to fail early if `CMAKE_BUILD_TYPE` is not defined
        set(${BUILD_TYPE} ${CMAKE_BUILD_TYPE} PARENT_SCOPE)
    endif()
endfunction()


macro(set_conan_compiler_if_appleclang lang command output_variable)
    if(CMAKE_${lang}_COMPILER_ID STREQUAL "AppleClang")
        execute_process(COMMAND xcrun --find ${command}
                OUTPUT_VARIABLE _xcrun_out OUTPUT_STRIP_TRAILING_WHITESPACE)
        cmake_path(GET _xcrun_out PARENT_PATH _xcrun_toolchain_path)
        cmake_path(GET CMAKE_${lang}_COMPILER PARENT_PATH _compiler_parent_path)
        if ("${_xcrun_toolchain_path}" STREQUAL "${_compiler_parent_path}")
            set(${output_variable} "")
        endif()
        unset(_xcrun_out)
        unset(_xcrun_toolchain_path)
        unset(_compiler_parent_path)
    endif()
endmacro()



macro(append_compiler_executables_configuration)
    set(_conan_c_compiler "")
    set(_conan_cpp_compiler "")
    if(CMAKE_C_COMPILER)
        set(_conan_c_compiler "\"c\":\"${CMAKE_C_COMPILER}\",")
        set_conan_compiler_if_appleclang(C cc _conan_c_compiler)
    else()
        message(WARNING "CMake-Conan: The C compiler is not defined. "
                "Please define CMAKE_C_COMPILER or enable the C language.")
    endif()
    if(CMAKE_CXX_COMPILER)
        set(_conan_cpp_compiler "\"cpp\":\"${CMAKE_CXX_COMPILER}\"")
        set_conan_compiler_if_appleclang(CXX c++ _conan_cpp_compiler)
    else()
        message(WARNING "CMake-Conan: The C++ compiler is not defined. "
                "Please define CMAKE_CXX_COMPILER or enable the C++ language.")
    endif()

    if(NOT "x${_conan_c_compiler}${_conan_cpp_compiler}" STREQUAL "x")
        string(APPEND PROFILE "tools.build:compiler_executables={${_conan_c_compiler}${_conan_cpp_compiler}}\n")
    endif()
    unset(_conan_c_compiler)
    unset(_conan_cpp_compiler)
endmacro()


function(detect_host_profile output_file)
    detect_os(MYOS MYOS_API_LEVEL MYOS_SDK MYOS_SUBSYSTEM MYOS_VERSION)
    detect_arch(MYARCH)
    detect_compiler(MYCOMPILER MYCOMPILER_VERSION MYCOMPILER_RUNTIME MYCOMPILER_RUNTIME_TYPE)
    detect_cxx_standard(MYCXX_STANDARD)
    detect_lib_cxx(MYLIB_CXX)
    detect_build_type(MYBUILD_TYPE)

    set(PROFILE "")
    string(APPEND PROFILE "[settings]\n")
    if(MYARCH)
        string(APPEND PROFILE arch=${MYARCH} "\n")
    endif()
    if(MYOS)
        string(APPEND PROFILE os=${MYOS} "\n")
    endif()
    if(MYOS_API_LEVEL)
        string(APPEND PROFILE os.api_level=${MYOS_API_LEVEL} "\n")
    endif()
    if(MYOS_VERSION)
        string(APPEND PROFILE os.version=${MYOS_VERSION} "\n")
    endif()
    if(MYOS_SDK)
        string(APPEND PROFILE os.sdk=${MYOS_SDK} "\n")
    endif()
    if(MYOS_SUBSYSTEM)
        string(APPEND PROFILE os.subsystem=${MYOS_SUBSYSTEM} "\n")
    endif()
    if(MYCOMPILER)
        string(APPEND PROFILE compiler=${MYCOMPILER} "\n")
    endif()
    if(MYCOMPILER_VERSION)
        string(APPEND PROFILE compiler.version=${MYCOMPILER_VERSION} "\n")
    endif()
    if(MYCOMPILER_RUNTIME)
        string(APPEND PROFILE compiler.runtime=${MYCOMPILER_RUNTIME} "\n")
    endif()
    if(MYCOMPILER_RUNTIME_TYPE)
        string(APPEND PROFILE compiler.runtime_type=${MYCOMPILER_RUNTIME_TYPE} "\n")
    endif()
    if(MYCXX_STANDARD)
        string(APPEND PROFILE compiler.cppstd=${MYCXX_STANDARD} "\n")
    endif()
    if(MYLIB_CXX)
        string(APPEND PROFILE compiler.libcxx=${MYLIB_CXX} "\n")
    endif()
    if(MYBUILD_TYPE)
        string(APPEND PROFILE "build_type=${MYBUILD_TYPE}\n")
    endif()

    if(NOT DEFINED output_file)
        set(_FN "${CMAKE_BINARY_DIR}/profile")
    else()
        set(_FN ${output_file})
    endif()

    string(APPEND PROFILE "[conf]\n")
    string(APPEND PROFILE "tools.cmake.cmaketoolchain:generator=${CMAKE_GENERATOR}\n")

    # propagate compilers via profile
    append_compiler_executables_configuration()

    if(MYOS STREQUAL "Android")
        string(APPEND PROFILE "tools.android:ndk_path=${CMAKE_ANDROID_NDK}\n")
    endif()

    message(STATUS "CMake-Conan: Creating profile ${_FN}")
    file(WRITE ${_FN} ${PROFILE})
    message(STATUS "CMake-Conan: Profile: \n${PROFILE}")
endfunction()