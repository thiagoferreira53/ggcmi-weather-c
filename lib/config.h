#ifndef WTH_CONFIG_H
#define WTH_CONFIG_H
#include <stddef.h>

#include "location.h"

typedef struct FileConfig_ {
  char *file_name;
  char *netcdf_var;
  char *dssat_var;
  char *source_unit;
  char *target_unit;
  int netcdf_id;
  int is_temp;
  int is_rh;
} FileConfig;

typedef struct Config_ {
  int start_year;
  char *output_dir;
  size_t num_mappings;
  size_t num_points;
  int mode; // 0=global, 1=extent, 2=points
  LonLat *points;
  FileConfig *mappings;
} Config;

Config *LoadConfig(const char *source);
void FreeConfig(Config *config);
#endif
