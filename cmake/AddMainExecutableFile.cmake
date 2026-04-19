get_filename_component(CMAKEFILEEMBEDDER_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

function(add_main_executable_file TARGET_NAME SOURCE_FILE)
    if(ARGC GREATER 2)
        message(FATAL_ERROR "add_main_executable_file expects exactly one SOURCE_FILE")
    endif()

    get_filename_component(SRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}" REALPATH)
    if(NOT EXISTS "${SRC_PATH}")
        message(FATAL_ERROR "SOURCE_FILE not found: ${SRC_PATH}")
    endif()

    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${GEN_DIR}")

    set(SOURCE_PATH "${GEN_DIR}/embedded_files.cpp")

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

    if(MSVC)
        set(PP_FLAGS /EP /nologo)
    else()
        set(PP_FLAGS -E -P -x c++)
    endif()

    set(PP_FLAGS_FILE ${CMAKE_BINARY_DIR}/cfe_cc_pp_flags.txt)
    file(WRITE "${PP_FLAGS_FILE}" "${PP_FLAGS}")

    add_custom_command(
            OUTPUT "${WRAPPER_MAIN}"
            COMMAND ${CMAKE_COMMAND}
            -DSRC_PATH=${SRC_PATH}
            -DOUT_FILE=${WRAPPER_MAIN}
            -DCOMPILER=${CMAKE_CXX_COMPILER}
            -DPP_FLAGS_FILE=${PP_FLAGS_FILE}
            -DMSVC=$<BOOL:${MSVC}>
            -DINCLUDE_DIRS=$<TARGET_PROPERTY:${TARGET_NAME},INCLUDE_DIRECTORIES>
            -DINT_INCLUDE_DIRS=$<TARGET_PROPERTY:${TARGET_NAME},INTERFACE_INCLUDE_DIRECTORIES>
            -DCOMPILE_DEFINITIONS=$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_DEFINITIONS>
            -DINT_COMPILE_DEFINITIONS=$<TARGET_PROPERTY:${TARGET_NAME},INTERFACE_COMPILE_DEFINITIONS>
            -DCOMPILE_OPTIONS=$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_OPTIONS>
            -DINT_COMPILE_OPTIONS=$<TARGET_PROPERTY:${TARGET_NAME},INTERFACE_COMPILE_OPTIONS>
            -P "${CMAKEFILEEMBEDDER_DIR}/GenerateMainWrapper.cmake"
            DEPENDS
            "${SRC_PATH}"
            "${PP_FLAGS_FILE}"
            "${CMAKEFILEEMBEDDER_DIR}/GenerateMainWrapper.cmake"
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
    elseif(TARGET CMakeFileEmbedder)
        target_link_libraries(${TARGET_NAME} PRIVATE CMakeFileEmbedder)
    else()
        message(FATAL_ERROR "CMakeFileEmbedder not found")
    endif()
endfunction()