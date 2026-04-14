if(MSVC)
    set(PP_FLAGS /EP)
else()
    set(PP_FLAGS -E -P)
endif()

execute_process(
        COMMAND ${COMPILER} ${PP_FLAGS} "${SRC_PATH}"
        OUTPUT_VARIABLE SOURCE_CONTENT
        ERROR_VARIABLE PP_ERROR
        RESULT_VARIABLE PP_RESULT
)

if(NOT PP_RESULT EQUAL 0)
    message(FATAL_ERROR "Preprocessing failed:\n${PP_ERROR}")
endif()

set(ENTRY_POINT "main")
set(ARG_MODE "none")

if(SOURCE_CONTENT MATCHES "_tWinMain\\s*\\(")
    set(ENTRY_POINT "_tWinMain")
elseif(SOURCE_CONTENT MATCHES "wWinMain\\s*\\(")
    set(ENTRY_POINT "wWinMain")
elseif(SOURCE_CONTENT MATCHES "WinMain\\s*\\(")
    set(ENTRY_POINT "WinMain")
else()
    if(SOURCE_CONTENT MATCHES "(^|[^a-zA-Z0-9_])main\\s*\\(\\s*void\\s*\\)")
        set(ARG_MODE "none")

    elseif(SOURCE_CONTENT MATCHES "(^|[^a-zA-Z0-9_])main\\s*\\(\\s*int[^,)]*,[^)]*\\*[^)]*\\)")
        set(ARG_MODE "argc_argv_envp")

    elseif(SOURCE_CONTENT MATCHES "(^|[^a-zA-Z0-9_])main\\s*\\(\\s*int[^,)]*,[^)]*\\*\\s*\\*[^)]*\\)")
        set(ARG_MODE "argc_argv")

    elseif(SOURCE_CONTENT MATCHES "(^|[^a-zA-Z0-9_])main\\s*\\(")
        set(ARG_MODE "none")

    else()
        message(FATAL_ERROR "No valid main() found")
    endif()
endif()

if(ENTRY_POINT STREQUAL "main")
    set(RENAMED_MAIN "user_main")
    if(ARG_MODE STREQUAL "none")
        set(SIG "int main()")
        set(CALL "user_main()")
    elseif(ARG_MODE STREQUAL "argc_argv")
        set(SIG "int main(int argc, char *argv[])")
        set(CALL "user_main(argc, argv)")
    elseif(ARG_MODE STREQUAL "argc_argv_envp")
        set(SIG "int main(int argc, char *argv[], char *envp[])")
        set(CALL "user_main(argc, argv, envp)")
    endif()
    set(BODY "InstallFileInterceptionHooks();\nreturn ${CALL};")
elseif(ENTRY_POINT STREQUAL "WinMain")
    set(RENAMED_MAIN "user_WinMain")
    set(SIG "int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)")
    set(BODY "InstallFileInterceptionHooks();\nreturn user_WinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);")
elseif(ENTRY_POINT STREQUAL "wWinMain")
    set(RENAMED_MAIN "user_wWinMain")
    set(SIG "int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)")
    set(BODY "InstallFileInterceptionHooks();\nreturn user_wWinMain(hInstance,hPrevInstance,pCmdLine,nCmdShow);")
elseif(ENTRY_POINT STREQUAL "_tWinMain")
    set(RENAMED_MAIN "user_tWinMain")
    set(SIG "int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)")
    set(BODY "InstallFileInterceptionHooks();\nreturn user_tWinMain(hInstance,hPrevInstance,lpCmdLine,nCmdShow);")
endif()

file(WRITE "${OUT_FILE}" "// Auto-generated wrapper main\n")
file(APPEND "${OUT_FILE}" "#define ${ENTRY_POINT} ${RENAMED_MAIN}\n")
file(APPEND "${OUT_FILE}" "#include \"${SRC_PATH}\"\n")
file(APPEND "${OUT_FILE}" "#undef ${ENTRY_POINT}\n")
file(APPEND "${OUT_FILE}" "extern \"C\" void InstallFileInterceptionHooks();\n")
file(APPEND "${OUT_FILE}" "${SIG}\n{\n${BODY}\n}\n")