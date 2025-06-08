add_executable(ggcmi2dssatw main.c)
set_property(TARGET ggcmi2dssatw PROPERTY C_STANDARD 99)
target_include_directories(ggcmi2dssatw PRIVATE ${MPI_C_INCLUDE_PATH})
target_link_libraries(ggcmi2dssatw PRIVATE MPI::MPI_C PkgConfig::NETCDF ggcmiw m)
