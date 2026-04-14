cmake_minimum_required(VERSION 4.1)
project(CMakeFileEmbedder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_INSTALL_INCLUDEDIR "include")
set(CMAKE_INSTALL_LIBDIR "lib")

set(CMAKEFILEEMBEDDER_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(WIN32)
        add_library(CMakeFileEmbedder
                src/pWindowsHook.cpp
        )
elseif(UNIX AND NOT APPLE)
        add_library(CMakeFileEmbedder SHARED
                src/pLinuxHook.cpp
        )
        target_link_libraries(CMakeFileEmbedder
                PRIVATE
                dl
        )
        set_target_properties(CMakeFileEmbedder PROPERTIES
                POSITION_INDEPENDENT_CODE ON)
else()
        message(FATAL_ERROR "CMakeFileEmbedder: not supported on this platform (expected Windows or Linux)")
endif()

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MH_ARCH "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(MH_ARCH "x86")
    else()
        message(FATAL_ERROR "Unsupported pointer size; expected 4 or 8.")
    endif()

    if(MSVC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(MH_EXT "lib")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(MH_EXT "a")
    else()
        message(FATAL_ERROR "Unsupported compiler; expected MSVC or GNU (MinGW).")
    endif()

    set(MINHOOK_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.${MH_ARCH}.${MH_EXT}")
    set(MINHOOK_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.${MH_ARCH}d.${MH_EXT}")

    target_link_libraries(CMakeFileEmbedder PRIVATE
            $<$<CONFIG:Debug>:${MINHOOK_DEBUG}>
            $<$<NOT:$<CONFIG:Debug>>:${MINHOOK_RELEASE}>
    )
endif()

target_include_directories(CMakeFileEmbedder
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(CMakeFileEmbedder
        PUBLIC
        cxx_std_20
)

install(
        TARGETS CMakeFileEmbedder
        EXPORT CMakeFileEmbedderTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        CONFIGURATIONS Debug Release
)

install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFileEmbedderConfigVersion.cmake"
        VERSION ${CMakeFileEmbedder_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeFileEmbedderConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFileEmbedderConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)

install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFileEmbedderConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFileEmbedderConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)

install(
        FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddMainExecutableFile.cmake"
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)
install(
        FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AddEmbeddedFiles.cmake"
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)
install(
        FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateMainWrapper.cmake"
        DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)

if (UNIX AND NOT APPLE)
    install(
            FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeFileEmbedderTargets-debug.cmake"
            DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
    )
    install(
            FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CMakeFileEmbedderTargets-release.cmake"
            DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
    )
endif()

install(
        EXPORT CMakeFileEmbedderTargets
        NAMESPACE CMakeFileEmbedder::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CMakeFileEmbedder
)