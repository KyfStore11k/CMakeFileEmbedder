get_filename_component(CMAKEFILEEMBEDDER_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

function(add_main_executable_file TARGET_NAME SOURCE_FILE)
    if(ARGC GREATER 2)
        message(FATAL_ERROR "add_main_executable_file expects exactly one SOURCE_FILE, not ${ARGN}")
    endif()

    get_filename_component(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}" REALPATH)
    if(NOT EXISTS "${SRC_PATH}")
        message(FATAL_ERROR "add_main_executable_file: SOURCE_FILE='${SOURCE_FILE}' does not exist at '${SRC_PATH}'")
    endif()

    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(SOURCE_PATH "${GEN_DIR}/embedded_files.cpp")
    file(MAKE_DIRECTORY "${GEN_DIR}")

    file(WRITE "${SOURCE_PATH}"
            "#include <unordered_map>\n"
            "#include <string>\n\n"
            "namespace embedded {\n"
            "struct EmbeddedFile {\n"
            "    const unsigned char* data;\n"
            "    size_t size;\n"
            "};\n"
            "std::unordered_map<std::string, EmbeddedFile> registry;\n"
            "}\n"
    )
    target_sources(${TARGET_NAME} PRIVATE "${SOURCE_PATH}")

    set(WRAPPER_MAIN "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_wrapper_main.cpp")

    add_custom_command(
            OUTPUT "${WRAPPER_MAIN}"
            COMMAND ${CMAKE_COMMAND}
            -DSRC_PATH=${SRC_PATH}
            -DOUT_FILE=${WRAPPER_MAIN}
            -DCOMPILER=${CMAKE_CXX_COMPILER}
            -DMSVC=$<BOOL:${MSVC}>
            -P "${CMAKEFILEEMBEDDER_DIR}/GenerateMainWrapper.cmake"
            DEPENDS "${SRC_PATH}"
            VERBATIM
    )

    add_custom_target(${TARGET_NAME}_generate_wrapper DEPENDS "${WRAPPER_MAIN}")
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_generate_wrapper)

    get_target_property(TGT_SOURCES ${TARGET_NAME} SOURCES)
    set(NEW_SOURCES "")
    foreach(src ${TGT_SOURCES})
        get_filename_component(ABS_SRC "${src}" ABSOLUTE)
        if(NOT ABS_SRC STREQUAL "${SRC_PATH}")
            list(APPEND NEW_SOURCES "${src}")
        endif()
    endforeach()

    set_target_properties(${TARGET_NAME} PROPERTIES SOURCES "${NEW_SOURCES}")
    target_sources(${TARGET_NAME} PRIVATE "${WRAPPER_MAIN}")

    if(TARGET CMakeFileEmbedder::CMakeFileEmbedder)
        target_link_libraries(${TARGET_NAME} PRIVATE CMakeFileEmbedder::CMakeFileEmbedder)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE CMakeFileEmbedder)
    endif()
endfunction()