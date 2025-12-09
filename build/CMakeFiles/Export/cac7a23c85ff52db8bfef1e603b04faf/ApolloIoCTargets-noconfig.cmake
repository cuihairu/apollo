#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ApolloIoC::common" for configuration ""
set_property(TARGET ApolloIoC::common APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(ApolloIoC::common PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libcommon.a"
  )

list(APPEND _cmake_import_check_targets ApolloIoC::common )
list(APPEND _cmake_import_check_files_for_ApolloIoC::common "${_IMPORT_PREFIX}/lib/libcommon.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
