cmake_minimum_required(VERSION 3.12..3.18)
project(GGCMI2DSSATW
    VERSION 1.1.0
    DESCRIPTION "GGCMI Weather NetCDF files to DSSAT Weather files")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    include(CTest)
endif()

include(FetchContent)
find_package(MPI REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)
pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)
pkg_check_modules(UDUNITS REQUIRED IMPORTED_TARGET udunits2)

include_directories(lib)
add_subdirectory(lib)
add_subdirectory(app)

