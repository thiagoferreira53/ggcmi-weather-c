#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>
#include <netcdf.h>
#include <netcdf_par.h>

#include "config.h"
#include "io.h"

static const char *kLongitudeString = "lon";
static const char *kLongitudeAltString = "longitude";
static const char *kLatitudeString = "lat";
static const char *kLatitudeAltString = "latitude";
static const char *kTimeString = "time";
static const char *kTimeAltString = "tstep";
static const char *kFillValueString = "missing_value";
static const char *kUnitString = "units";

// Helper function to try both dimension name conventions
static int try_inq_dimid(int ncid, const char *name1, const char *name2, int *dimid) {
  int status = nc_inq_dimid(ncid, name1, dimid);
  if (status != NC_NOERR && name2 != NULL) {
    status = nc_inq_dimid(ncid, name2, dimid);
  }
  return status;
}

// Helper function to try both variable name conventions
static int try_inq_varid(int ncid, const char *name1, const char *name2, int *varid) {
  int status = nc_inq_varid(ncid, name1, varid);
  if (status != NC_NOERR && name2 != NULL) {
    status = nc_inq_varid(ncid, name2, varid);
  }
  return status;
}

int OpenAllDataFiles(Config *config, MPI_Comm mpi_comm, MPI_Info mpi_info) {
  for (size_t i = 0; i < config->num_mappings; ++i) {
    int status;
    if ((status =
             nc_open_par(config->mappings[i].file_name, NC_NOWRITE, mpi_comm,
                         mpi_info, &config->mappings[i].netcdf_id))) {
      fprintf(stderr, "error: cannot open file %s: %s\n",
              config->mappings[i].file_name, nc_strerror(status));
      return -1;
    } else {
      printf("Opened %s\n", config->mappings[i].file_name);
    }
  }
  return config->num_mappings;
}

int InjectNetCdfInfo(Config *config, NetCdfInfo *info) {
  int status;
  char varname[NC_MAX_NAME + 1];
  InqVars inq;

  for (size_t i = 0; i < config->num_mappings; ++i) {
    int dimid;
    if ((status = nc_inq(config->mappings[i].netcdf_id, &inq.num_dims,
                         &inq.num_vars, &inq.num_gattrs, &inq.num_unlimited))) {
      fprintf(stderr, "error: cannot inquire NetCDF file #%zu: %s\n", i + 1,
              nc_strerror(status));
      return 1;
    }
    // Fill the NetCdfInfo
    if ((status = try_inq_varid(config->mappings[i].netcdf_id, kLongitudeString,
                                kLongitudeAltString, &info[i].longitude_varid))) {
      fprintf(stderr, "error: cannot find longitude/lon varid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = try_inq_varid(config->mappings[i].netcdf_id, kLatitudeString,
                                kLatitudeAltString, &info[i].latitude_varid))) {
      fprintf(stderr, "error: cannot find latitude/lat varid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = nc_inq_varid(config->mappings[i].netcdf_id, kTimeString,
                               &info[i].time_varid))) {
      fprintf(stderr, "error: cannot find time varid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = nc_inq_varid(config->mappings[i].netcdf_id,
                               config->mappings[i].netcdf_var,
                               &info[i].var_varid))) {
      fprintf(stderr, "error: cannot find %s varid in file #%zu: %s\n",
              config->mappings[i].netcdf_var, i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = try_inq_dimid(config->mappings[i].netcdf_id, kLongitudeString,
                                kLongitudeAltString, &dimid))) {
      fprintf(stderr, "error: cannot find longitude/lon dimid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = nc_inq_dimlen(config->mappings[i].netcdf_id, dimid,
                                &info[i].longitude_len))) {
      fprintf(
          stderr,
          "error: cannot find the dimension length for longitude/lon in file #%zu: %s\n",
          i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = try_inq_dimid(config->mappings[i].netcdf_id, kLatitudeString,
                                kLatitudeAltString, &dimid))) {
      fprintf(stderr, "error: cannot find latitude/lat dimid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = nc_inq_dimlen(config->mappings[i].netcdf_id, dimid,
                                &info[i].latitude_len))) {
      fprintf(
          stderr,
          "error: cannot find the dimension length for latitude/lat in file #%zu: %s\n",
          i + 1, nc_strerror(status));
      return 1;
    }
    if ((status = try_inq_dimid(config->mappings[i].netcdf_id, kTimeString,
                                kTimeAltString, &dimid))) {
      fprintf(stderr, "error: cannot find time/tstep dimid for file #%zu: %s\n",
              i + 1, nc_strerror(status));
      fprintf(stderr, "File: %s\n", config->mappings[i].file_name);
      fprintf(stderr, "Attempting to list all dimensions in this file:\n");
      for (int d = 0; d < inq.num_dims; d++) {
        char dim_name[NC_MAX_NAME + 1];
        size_t dim_len;
        if (nc_inq_dim(config->mappings[i].netcdf_id, d, dim_name, &dim_len) == NC_NOERR) {
          fprintf(stderr, "  Dimension %d: %s (length=%zu)\n", d, dim_name, dim_len);
        }
      }
      return 1;
    }
    if ((status = nc_inq_dimlen(config->mappings[i].netcdf_id, dimid,
                                &info[i].time_len))) {
      fprintf(
          stderr,
          "error: cannot find the dimension length for time/tstep in file #%zu: %s\n",
          i + 1, nc_strerror(status));
      return 1;
    }
    
    // Get file variable names
    //int ncid = config->mappings[i].netcdf_id;
    //int num_vars = 0;
    //int retval;
    //
    //if ((retval = nc_inq(ncid, NULL, &num_vars, NULL, NULL))) {
    //    fprintf(stderr, "Error inquiring file info: %s\n", nc_strerror(retval));
    //    return 1;
    //}
    //
    //printf("Variables in file #%zu:\n", i + 1);
    //for (int varid = 0; varid < num_vars; ++varid) {
    //    char var_name[NC_MAX_NAME + 1];
    //    if ((retval = nc_inq_varname(ncid, varid, var_name))) {
    //        fprintf(stderr, "  Error getting variable name (varid %d): %s\n", varid, nc_strerror(retval));
    //    } else {
    //        printf("  %s\n", var_name);
    //    }
    //}    
    
    status = nc_get_att_float(config->mappings[i].netcdf_id, info[i].var_varid,
      "missing_value", &info[i].fill_value);
    if (status == NC_NOERR) {
    } else if (status == NC_ENOTATT) {
    // Try "_FillValue" if "missing_value" not found
    status = nc_get_att_float(config->mappings[i].netcdf_id, info[i].var_varid,
              "_FillValue", &info[i].fill_value);
    if (status == NC_NOERR) {
    fprintf(stderr, "Info: using _FillValue instead of missing_value for variable '%s' in file #%zu\n",
    config->mappings[i].netcdf_var, i + 1);
    } else if (status == NC_ENOTATT) {
    fprintf(stderr, "Warning: no fill value attribute found for variable '%s' in file #%zu. Using default.\n",
    config->mappings[i].netcdf_var, i + 1);
    info[i].fill_value = -9999.0;
    } else {
    fprintf(stderr, "Error reading _FillValue for variable '%s': %s\n",
    config->mappings[i].netcdf_var, nc_strerror(status));
    return 1;
    }
    } else {
    fprintf(stderr, "Error reading missing_value for variable '%s': %s\n",
    config->mappings[i].netcdf_var, nc_strerror(status));
    return 1;
    }
    size_t unit_len = 0;
    if ((status = nc_inq_attlen(config->mappings[i].netcdf_id,
                                info[i].var_varid, kUnitString, &unit_len))) {
      fprintf(
          stderr,
          "error: could not handle unit extraction (len) in file #%zu: %s\n",
          i + 1, nc_strerror(status));
      return 1;
    }
    unit_len++;
    info[i].unit = (char *)malloc(unit_len * sizeof(char *));
    memset(info[i].unit, 0, unit_len);

    if ((status =
             nc_get_att_text(config->mappings[i].netcdf_id, info[i].var_varid,
                             kUnitString, info[i].unit))) {
      fprintf(
          stderr,
          "error: could not handle unit extraction (str) in file #%zu: %s\n",
          i + 1, nc_strerror(status));
      return 1;
    }
    info[i].unit[unit_len] = '\0';
  }
  return 0;
}

int CloseAllDataFiles(Config *config, NetCdfInfo *info) {
  int retval = 0;
  int status;
  for (size_t i = 0; i < config->num_mappings; ++i) {
    if (info[i].unit != NULL) {
      free(info[i].unit);
      info[i].unit = NULL;
    }
    if (config->mappings[i].netcdf_id != -1) {
      if ((status = nc_close(config->mappings[i].netcdf_id))) {
        fprintf(stderr, "error: %s [%s]", nc_strerror(status),
                config->mappings[i].file_name);
        retval = 1;
      }
      config->mappings[i].netcdf_id = -1;
      printf("Closed %s\n", config->mappings[i].file_name);
    }
  }
  return retval;
}
