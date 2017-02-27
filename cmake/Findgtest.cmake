# will search googletest library under CMAKE_INSTALL_PREFIX, as all projects share the same value
# but cmake starts with some system variable which usually contains the file already
find_path(GTEST_INCLUDE_DIR
    NAMES gtest/gtest.h
    PATHS "${CMAKE_INSTALL_PREFIX}/include")

find_library(GTEST_LIBRARY_DIR
    NAMES gtest_main
    PATHS "${CMAKE_INSTALL_PREFIX}/lib")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(gtest DEFAULT_MSG GTEST_INCLUDE_DIR GTEST_LIBRARY_DIR)

set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
get_filename_component(GTEST_LIBRARY_DIRS ${GTEST_LIBRARY_DIR} PATH)
message($GTEST_LIBRARY_DIRS)
set(GTEST_LIBRARIES "gtest")
