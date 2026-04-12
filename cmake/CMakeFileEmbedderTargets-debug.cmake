set(CMAKE_IMPORT_FILE_VERSION 1)

set_property(TARGET CMakeFileEmbedder::CMakeFileEmbedder APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(CMakeFileEmbedder::CMakeFileEmbedder PROPERTIES
        IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
        IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libCMakeFileEmbedder.so"
)

list(APPEND _cmake_import_check_targets CMakeFileEmbedder::CMakeFileEmbedder )
list(APPEND _cmake_import_check_files_for_CMakeFileEmbedder::CMakeFileEmbedder "${_IMPORT_PREFIX}/lib/libCMakeFileEmbedder.so" )

set(CMAKE_IMPORT_FILE_VERSION)
