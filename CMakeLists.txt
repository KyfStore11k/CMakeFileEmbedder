cmake_minimum_required(VERSION 4.1)
project(CMakeFileEmbedder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_INSTALL_INCLUDEDIR "include")
set(CMAKE_INSTALL_LIBDIR "lib")

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
    if(MSVC)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MINHOOK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.x64.lib")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(MINHOOK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.x86.lib")
        else()
            message(FATAL_ERROR "Unsupported pointer size; expected 4 or 8.")
        endif()
    elseif(MINGW)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(MINHOOK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.x64.a")
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(MINHOOK_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/libMinHook.x86.a")
        else()
            message(FATAL_ERROR "Unsupported pointer size; expected 4 or 8.")
        endif()
    else()
        message(FATAL_ERROR "Unsupported compiler; expected MSVC or MinGW.")
    endif()

    target_link_libraries(CMakeFileEmbedder PRIVATE "${MINHOOK_LIB}")
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