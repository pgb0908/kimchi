
function(conan_install)
    cmake_parse_arguments(ARGS CONAN_ARGS ${ARGN})
    set(CONAN_OUTPUT_FOLDER ${CMAKE_BINARY_DIR}/conan)
    # Invoke "conan install" with the provided arguments
    set(CONAN_ARGS ${CONAN_ARGS} -of=${CONAN_OUTPUT_FOLDER})
    message(STATUS "CMake-Conan: conan install ${CMAKE_SOURCE_DIR} ${CONAN_ARGS} ${ARGN}")


    # In case there was not a valid cmake executable in the PATH, we inject the
    # same we used to invoke the provider to the PATH
    if(DEFINED PATH_TO_CMAKE_BIN)
        set(_OLD_PATH $ENV{PATH})
        set(ENV{PATH} "$ENV{PATH}:${PATH_TO_CMAKE_BIN}")
    endif()

    execute_process(COMMAND ${CONAN_COMMAND} install ${CMAKE_SOURCE_DIR} ${CONAN_ARGS} ${ARGN} --format=json
            RESULT_VARIABLE return_code
            OUTPUT_VARIABLE conan_stdout
            ERROR_VARIABLE conan_stderr
            ECHO_ERROR_VARIABLE    # show the text output regardless
            #ECHO_OUTPUT_VARIABLE
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    if(DEFINED PATH_TO_CMAKE_BIN)
        set(ENV{PATH} "${_OLD_PATH}")
    endif()

    if(NOT "${return_code}" STREQUAL "0")
        message(FATAL_ERROR "Conan install failed='${return_code}'")
    endif()

    # the files are generated in a folder that depends on the layout used, if
    # one is specified, but we don't know a priori where this is.
    # TODO: this can be made more robust if Conan can provide this in the json output
    string(JSON CONAN_GENERATORS_FOLDER GET "${conan_stdout}" graph nodes 0 generators_folder)
    cmake_path(CONVERT ${CONAN_GENERATORS_FOLDER} TO_CMAKE_PATH_LIST CONAN_GENERATORS_FOLDER)
    # message("conan stdout: ${conan_stdout}")
    message(STATUS "CMake-Conan: CONAN_GENERATORS_FOLDER=${CONAN_GENERATORS_FOLDER}")
    set_property(GLOBAL PROPERTY CONAN_GENERATORS_FOLDER "${CONAN_GENERATORS_FOLDER}")
    # reconfigure on conanfile changes
    string(JSON CONANFILE GET "${conan_stdout}" graph nodes 0 label)
    message(STATUS "CMake-Conan: CONANFILE=${CMAKE_SOURCE_DIR}/${CONANFILE}")
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${CONANFILE}")
    # success
    set_property(GLOBAL PROPERTY CONAN_INSTALL_SUCCESS TRUE)

endfunction()