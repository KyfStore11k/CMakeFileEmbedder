function(add_embedded_files TARGET)
    if(ARGC LESS 2)
        message(FATAL_ERROR "add_embedded_files(TARGET [file1 file2 ...])")
    endif()

    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${GEN_DIR}")

    set(HEADER_PATH "${GEN_DIR}/embedded_files.h")
    set(SOURCE_PATH "${GEN_DIR}/embedded_files.cpp")

    file(WRITE "${HEADER_PATH}" "#pragma once\n")
    file(APPEND "${HEADER_PATH}" "#include <cstddef>\n#include <unordered_map>\n#include <string>\n\n")

    file(WRITE "${SOURCE_PATH}" "#include \"embedded_files.h\"\n\nnamespace embedded {\n")
    file(APPEND "${HEADER_PATH}" "namespace embedded {\n")

    file(APPEND "${HEADER_PATH}" "    struct EmbeddedFile {\n")
    file(APPEND "${HEADER_PATH}" "        const unsigned char* data;\n")
    file(APPEND "${HEADER_PATH}" "        size_t size;\n")
    file(APPEND "${HEADER_PATH}" "    };\n\n")
    file(APPEND "${HEADER_PATH}" "    extern std::unordered_map<std::string, EmbeddedFile> registry;\n")
    file(APPEND "${HEADER_PATH}" "}\n")

    file(APPEND "${SOURCE_PATH}" "std::unordered_map<std::string, EmbeddedFile> registry;\n")

    foreach(FILE_PATH ${ARGN})
        get_filename_component(ABS_PATH "${FILE_PATH}" ABSOLUTE)

        get_filename_component(FILE_NAME "${ABS_PATH}" NAME)
        string(REPLACE "/" "_" SYMBOL_NAME "${FILE_NAME}")
        string(REPLACE "." "_" SYMBOL_NAME "${SYMBOL_NAME}")

        file(SIZE "${ABS_PATH}" FILE_SIZE)
        file(READ "${ABS_PATH}" HEX_DATA HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," HEX_LIST "${HEX_DATA}")
        string(REGEX REPLACE "0x,$" "0x00" HEX_LIST "${HEX_LIST}")

        file(APPEND "${SOURCE_PATH}"
                "const unsigned char ${SYMBOL_NAME}_data[${FILE_SIZE}] = { ${HEX_LIST} };\n"
                "size_t ${SYMBOL_NAME}_size = ${FILE_SIZE};\n"
                "\n")

        file(APPEND "${SOURCE_PATH}"
                "struct __init__${SYMBOL_NAME} {\n"
                "    __init__${SYMBOL_NAME}() {\n"
                "        registry[\"${FILE_PATH}\"] = {\n"
                "            ${SYMBOL_NAME}_data,\n"
                "            ${SYMBOL_NAME}_size\n"
                "        };\n"
                "    }\n"
                "} __init__${SYMBOL_NAME}_instance;\n\n")
    endforeach()

    file(APPEND "${SOURCE_PATH}" "}\n")

    target_include_directories(${TARGET}
            PRIVATE
            "${GEN_DIR}"
    )
    target_sources(${TARGET}
            PRIVATE
            "${HEADER_PATH}"
            "${SOURCE_PATH}"
    )
endfunction()