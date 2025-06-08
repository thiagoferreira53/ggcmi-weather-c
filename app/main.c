#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mpi.h>
#include <netcdf.h>

#include "calendar.h"
#include "config.h"
#include "hyperslab.h"
#include "io.h"
#include "location.h"
#include "unit_util.h"

static void resetDailyAvg(float *daily_avg) {
  for (size_t i = 0; i < 31; ++i) {
    daily_avg[i] = -99.9f;
  }
}

static float calculateMonthlyAvg(const float *daily_avg) {
  float sum = 0.0f;
  size_t i = 0;
  while (daily_avg[i] != -99.9f && i < 31) {
    sum += daily_avg[i];
    ++i;
  }
  if (i == 0) {
    return 0.0f;
  }
  return sum / i;
}

int main(int argc, char **argv) {
  printf("== GGCMI to DSSAT Weather(4-Digit Year) Extractor ==\n");
  size_t start_time = time(NULL);
  if (argc != 2) {
    fprintf(stderr, "error: not enough arguments\n");
    return EXIT_FAILURE;
  }
  MPI_Init(NULL, NULL);
  int world_size;
  int world_rank;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  char *config_file = argv[1];
  Config *config;

  printf("Loading config file: %s\n", config_file);
  config = LoadConfig(config_file);
  if (!config) {
    return EXIT_FAILURE;
  }

  NetCdfInfo info[config->num_mappings];
  for (size_t i = 0; i < config->num_mappings; ++i) {
    info[i].unit = NULL;
  }
  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);

  if (OpenAllDataFiles(config, MPI_COMM_WORLD, MPI_INFO_ENV) !=
      config->num_mappings) {
    CloseAllDataFiles(config, info);
    MPI_Finalize();
    FreeConfig(config);
    return EXIT_FAILURE;
  }

  int status;
  if (InjectNetCdfInfo(config, info)) {
    CloseAllDataFiles(config, info);
    MPI_Finalize();
    FreeConfig(config);
    return EXIT_FAILURE;
  }

  // This is the base allocation from config.c (extent)
  // TODO: Refactor to enable point based extraction
  XY offset;
  
  size_t x_length, y_length;
  if (config->mode < 2) {
    offset = LonLatToXY(config->points[0]);
    XY bottom_right = LonLatToXY(config->points[1]);
    x_length = bottom_right.x - offset.x + 1;
    y_length = bottom_right.y - offset.y + 1;
    if (world_rank == 0) {
      printf("Box ul: %zu, %zu\n", offset.x, offset.y);
      printf("Box br: %zu, %zu\n", bottom_right.x, bottom_right.y);
      printf("Box size: %d, %d\n", x_length, y_length);
    }
  }

  printf("Before hyperslab allocation: sizeof days => %zu\n", info[0].time_len);
  // TODO: Enable world_sizes to split into hyperslabs and run from there.
  Hyperslab *slabs = AllocateHyperslabs(
      Position(0, offset.x, offset.y),
      Edges(info[0].time_len, x_length, y_length), world_size, world_rank);

  Hyperslab h = slabs[world_rank];

  int app_status = EXIT_SUCCESS;
  float *values =
      (float *)malloc(sizeof(float) * config->num_mappings * h.flat_size);
  float *converted_values =
      (float *)malloc(sizeof(float) * config->num_mappings * h.flat_size);

  InitUnitSystem();
  ConverterContainer converters[config->num_mappings];
  for (size_t i = 0; i < config->num_mappings; ++i) {
    if (BuildConverter(config->mappings[i].source_unit,
                       config->mappings[i].target_unit, &converters[i])) {
      fprintf(stderr, "error: unable to build the converter for %s -> %s\n",
              config->mappings[i].source_unit, config->mappings[i].target_unit);
      app_status = EXIT_FAILURE;
      goto release_resources;
    }
  }
  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);
  printf("Starting allocation and fetching data\n");
  for (size_t m = 0; m < config->num_mappings; ++m) {
    if ((status = nc_get_vara_float(config->mappings[m].netcdf_id,
                                    info[m].var_varid, h.corner.shape,
                                    h.edges.shape, &values[m * h.flat_size]))) {
      fprintf(stderr,
              "error: unable to extract values from %s for variable "
              "%s.\n\t%s\n\tCorner: %d, %d, %d\n\tEdges: %d, %d, %d\n",
              config->mappings[m].file_name, config->mappings->netcdf_var,
              nc_strerror(status), h.corner.day, h.corner.x, h.corner.y,
              h.edges.days, h.edges.x_length, h.edges.y_length);
      app_status = EXIT_FAILURE;
      goto release_resources;
    }
  }
  printf("Ending allocation and fetching data\n");
  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);

  size_t counter = 0;
  size_t skipped = 0;
  char date_str[ISODATE_STRING_LEN];
  char start_date_str[ISODATE_STRING_LEN];
  status = snprintf(start_date_str, ISODATE_STRING_LEN, "%d-01-01",
                    config->start_year);
  if (status >= ISODATE_STRING_LEN) {
    fprintf(stderr, "error: year string is weirdly too long.\n");
    app_status = EXIT_FAILURE;
    goto release_resources;
  }

  date_t date;
  status = ParseDate(start_date_str, &date);
  if (status) {
    app_status = EXIT_FAILURE;
    goto release_resources;
  }

  int current_month = date.month;
  size_t months = 1;
  float daily_avg[31];
  double monthly_sum = 0.0;
  float tminavg = -99.9f;
  float tmaxavg = -99.9f;
  float tmin = -99.9f;
  float tmax = -99.9f;
  float mavg;
  float davg;
  float raw_value;
  float value;
  size_t index;

  resetDailyAvg(daily_avg);

  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);
  printf("Starting I/O\n");

  char debug_file[15];
  snprintf(debug_file, 15, "debug_%d.csv", world_rank);
  FILE *debug = fopen(debug_file, "w");
  fprintf(debug, "longitude,latitude,ID\n");
  for (size_t x = 0; x < h.edges.x_length; ++x) {
    for (size_t y = 0; y < h.edges.y_length; ++y) {
      ParseDate(start_date_str, &date);
      for (size_t d = 0; d < h.edges.days; ++d) {
        for (size_t m = 0; m < config->num_mappings; ++m) {
          index = (m * h.flat_size) + HyperslabValueIndex(h, Position(d, x, y));
          raw_value = values[index];
          if (raw_value == info[m].fill_value) {
            value = raw_value;
            if (d == 0) {
              ++skipped;
              goto skip_entry;
            }
          } else {
            value = ConvertValue(converters[m].cv, raw_value);
          }
          converted_values[index] = value;
          if (config->mappings[m].is_temp == 1) {
            tmin = value;
          } else if (config->mappings[m].is_temp == 2) {
            tmax = value;
          }
        }
        if (tmax != 99.9f && tmin != 99.9f) {
          davg = (tmax + tmin) / 2.0f;
          daily_avg[date.day_of_month - 1] = davg;
        }
        counter++;
        AddOneDay(&date);
        if (current_month != date.month) {
          mavg = calculateMonthlyAvg(daily_avg);
          if (tminavg == -99.9f) {
            tminavg = mavg;
          }
          if (tmaxavg == -99.9f) {
            tmaxavg = mavg;
          }
          if (tminavg > mavg) {
            tminavg = mavg;
          }
          if (tmaxavg < mavg) {
            tmaxavg = mavg;
          }
          monthly_sum += mavg;
          ++months;
          current_month = date.month;
          resetDailyAvg(daily_avg);
        }
      }
      XY global_pos = XYPosition(h.corner.x + x, h.corner.y + y);
      LonLat global_ll = XYToLonLat(global_pos);
      // Now we write out the file
      fprintf(debug, "%.2f,%.2f,%zu\n", global_ll.longitude, global_ll.latitude,
              XYToGlobalId(global_pos));
      char filename[2048];
      GenerateFileName(global_pos, config->output_dir, filename);
      FILE *fh = fopen(filename, "w");
      if (fh != NULL) {
        fprintf(fh, "*WEATHER DATA: GGCMI\n\n");
        fprintf(fh, "@ INSI      LAT     LONG  ELEV   TAV   AMP REFHT WNDHT\n");
        fprintf(fh, " GGCMI %8.2f %8.2f %5d %5.1f %5.1f\n", global_ll.latitude,
                global_ll.longitude, -99, (monthly_sum / months),
                tmaxavg - tminavg);
        fprintf(fh, "@  DATE");
        for (size_t i = 0; i < config->num_mappings; ++i) {
          if (config->mappings[i].dssat_var == "OZON7") {
            fprintf(fh, " %4s", config->mappings[i].dssat_var);
          }else {
          fprintf(fh, "  %4s", config->mappings[i].dssat_var);
          }
        }
        fprintf(fh, "\n");
        ParseDate(start_date_str, &date);
        for (size_t d = 0; d < h.edges.days; ++d) {
          DateAsDSSAT4String(&date, date_str);
          fprintf(fh, "%s", date_str);
          for (size_t m = 0; m < config->num_mappings; ++m) {
            index =
                (m * h.flat_size) + HyperslabValueIndex(h, Position(d, x, y));
            fprintf(fh, " %5.1f", converted_values[index]);
          }
          AddOneDay(&date);
          fprintf(fh, "\n");
        }
        fclose(fh);
      } else {
        fprintf(stderr, "error: could not open file for writing: %s\n",
                filename);
      }
    skip_entry:
      monthly_sum = 0.0;
      tminavg = -99.9f;
      tmaxavg = -99.9f;
      tmin = -99.9f;
      tmax = -99.9f;
      mavg = -99.9f;
      months = 1;
      resetDailyAvg(daily_avg);
    }
  }
  fclose(debug);
  printf("Records written: %zu\n", counter);
  printf("Records expected: %zu\n", h.flat_size);
  printf("Records skipped: %zu\n", skipped * h.edges.days);
  printf("Ending I/O\n");
  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);
release_resources:
  for (size_t i = 0; i < config->num_mappings; ++i) {
    printf("Releasing resources for %s\n", config->mappings[i].file_name);
    FreeConverterContainer(&converters[i]);
  }
  FreeUnitSystem();
  free(slabs);
  slabs = NULL;
  free(converted_values);
  converted_values = NULL;
  free(values);
  values = NULL;
  CloseAllDataFiles(config, info);
  FreeConfig(config);
  config = NULL;
  printf("[%d] Checkpoint in seconds: %zu\n", world_rank,
         time(NULL) - start_time);
  MPI_Finalize();
  return app_status;
}
