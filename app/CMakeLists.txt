add_executable(ggcmi2dssatw main.c)
set_property(TARGET ggcmi2dssatw PROPERTY C_STANDARD 99)
target_include_directories(ggcmi2dssatw PRIVATE ${MPI_C_INCLUDE_PATH})
target_link_libraries(ggcmi2dssatw PRIVATE MPI::MPI_C PkgConfig::NETCDF ggcmiw m)

# Set RPATH so the executable can find libraries at runtime
set_target_properties(ggcmi2dssatw PROPERTIES
    BUILD_RPATH "/usr/local/lib:/usr/local/openmpi-5.0.6/lib:/usr/local/gcc-14.2.0/lib64"
    INSTALL_RPATH "/usr/local/lib:/usr/local/openmpi-5.0.6/lib:/usr/local/gcc-14.2.0/lib64"
    BUILD_RPATH_USE_ORIGIN TRUE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)
