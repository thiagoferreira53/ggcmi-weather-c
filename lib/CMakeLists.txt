set(SOURCE_LIST calendar.c config.c hyperslab.c io.c location.c unit_util.c)
set(HEADER_LIST calendar.h config.h hyperslab.h io.h location.h unit_util.h)

add_library(ggcmiw ${SOURCE_LIST} ${HEADER_LIST})
set_property(TARGET ggcmiw PROPERTY C_STANDARD 99)
target_link_libraries(ggcmiw PRIVATE PkgConfig::JANSSON PkgConfig::NETCDF PkgConfig::UDUNITS MPI::MPI_C)
target_include_directories(ggcmiw PRIVATE ${MPI_C_INCLUDE_DIRS})
