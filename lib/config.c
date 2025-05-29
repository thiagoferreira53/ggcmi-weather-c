#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <jansson.h>

#include "config.h"

static int DirectoryExists(const char *directory) {
  if (access(directory, F_OK | R_OK | W_OK | X_OK)) {
    return 0;
  } else {
    return 1;
  }
}

static char *GetDirectoryString(const char *directory) {
  size_t string_size;
  char *final_directory;
  string_size = strlen(directory);
  if (directory[string_size - 1] != '/') {
    string_size += 1;
    final_directory = (char *)malloc(string_size);
    size_t size = snprintf(final_directory, string_size + 1, "%s/", directory);
  } else {
    final_directory = (char *)malloc(string_size);
    strncpy(final_directory, directory, string_size);
  }
  return final_directory;
}

static char *InsertConfigString(json_t *obj, const char *key) {
  if (obj == NULL || key == NULL)
    return NULL;
  json_t *json_v = json_object_get(obj, key);
  if (json_v == NULL) {
    return NULL;
  }
  const char *string_v = json_string_value(json_v);
  if (string_v == NULL) {
    return NULL;
  }
  size_t string_size = json_string_length(json_v) + 1;
  char *v = (char *)malloc(string_size);
  if (v == NULL) {
    return NULL;
  }
  strncpy(v, string_v, string_size);
  return v;
}

static int ValidLonLatShape(const json_t *arr) {
  if (!json_is_array(arr)) {
    fprintf(stderr, "error: Longitude/Latitude point is not an array.\n");
    return 0;
  }
  if (json_array_size(arr) != 2) {
    fprintf(stderr,
            "error: Invalid array items in Longitude/Latitude point.\n");
    return 0;
  }
  json_t *value;
  double lvalue;
  for (size_t i = 0; i < 2; ++i) {
    value = json_array_get(arr, i);
    if (!json_is_number(value)) {
      fprintf(stderr,
              "error: Longitude/Latitude point is not a number pair.\n");
      return 0;
    }
    lvalue = json_number_value(value);
    if (i == 0) {
      if (lvalue < -180.0 || lvalue > 180.0) {
        fprintf(stderr, "error: Longitude %f is out of bounds.\n", lvalue);
        return 0;
      }
    } else if (i == 1) {
      if (lvalue < -90 || lvalue > 90) {
        fprintf(stderr, "error: Latitude %f is out of bounds.\n", lvalue);
        return 0;
      }
    }
  }
  return 1;
}

static LonLat *ArrayToLonLat(json_t *arr) {
  LonLat *ll = (LonLat *)malloc(sizeof(LonLat));
  double longitude, latitude;
  longitude = json_number_value(json_array_get(arr, 0));
  latitude = json_number_value(json_array_get(arr, 1));
  ll->longitude = longitude;
  ll->latitude = latitude;
  return ll;
}

static int ValidModeShape(const json_t *obj, const int mode) {
  if (mode == 0) {
    return 1;
  } else if (mode == 1) {
    const char *check_fields[2] = {"top_left", "bottom_right"};
    LonLat *check_data[2];
    json_t *field_data;
    if (json_is_object(obj)) {
      for (size_t i = 0; i < 2; ++i) {
        if ((field_data = json_object_get(obj, check_fields[i])) != NULL) {
          if (ValidLonLatShape(field_data)) {
            check_data[i] = ArrayToLonLat(field_data);
          } else {
            return 0;
          }
        } else {
          fprintf(stderr, "error: %s is not defined in extent.\n", check_fields[i]);
          return 0;
        }
      }
      int check_pf = 0;
      if (check_data[0]->longitude <= check_data[1]->longitude) {
        if (check_data[0]->latitude >= check_data[1]->latitude) {
          check_pf = 1;
        }
      }
      if (!check_pf) {
        fprintf(stderr,
                "error: bottom_right is not below and right of top_left.\n");
      }
      free(check_data[1]);
      free(check_data[0]);
      return check_pf;
    } else {
      fprintf(stderr, "error: extent is not an object.\n");
      return 0;
    }
  } else if (mode == 2) {
    fprintf(stderr, "error: points mode is not yet supported.\n");
    return 0;
  }
}

Config *LoadConfig(const char *source) {
  json_t *root;
  json_error_t error;
  Config *config = NULL;

  root = json_load_file(source, JSON_REJECT_DUPLICATES, &error);

  if (!root) {
    fprintf(stderr, "error: [line %d] %s\n", error.line, error.text);
    return NULL;
  }
  if (!json_is_object(root)) {
    fprintf(stderr, "error: root is not a JSON object\n");
    json_decref(root);
    return NULL;
  }
  json_t *start_year, *output_dir, *mode_finder, *mappings;
  int mode = 0;
  start_year = json_object_get(root, "start_year");
  if (!json_is_integer(start_year)) {
    fprintf(stderr, "error: root->start_year is not an integer\n");
    json_decref(root);
    return NULL;
  }
  if (json_integer_value(start_year) > 3000) {
    fprintf(
        stderr,
        "error: we haven't come up with a better way to handle this by now?\n");
    json_decref(root);
    return NULL;
  }
  if (json_integer_value(start_year) < 1700) {
    fprintf(stderr, "error: this probably isn't realistic to run\n");
    json_decref(root);
    return NULL;
  }

  output_dir = json_object_get(root, "output_dir");
  if (!json_is_string(output_dir)) {
    fprintf(stderr, "error: output_dir is not specified\n");
    json_decref(root);
    return NULL;
  }

  if (!DirectoryExists(json_string_value(output_dir))) {
    fprintf(stderr, "error: output_dir does not exist\n");
    json_decref(root);
    return NULL;
  }

  mode_finder = json_object_get(root, "points");
  // This is where I check the shape of the points/extent
  // 0 is entire globe
  // 1 is extent (rectangle)
  // 2 is points (specified in array)
  if (mode_finder != NULL) {
    mode = 2;
  } else {
    mode_finder = json_object_get(root, "extent");
    if (mode_finder != NULL) {
      mode = 1;
    } else {
      mode = 0;
    }
  }

  if (!ValidModeShape(mode_finder, mode)) {
    json_decref(root);
    return NULL;
  }

  size_t mode_size;
  if (mode < 2) {
    mode_size = 2;
  } else {
    mode_size = json_array_size(mode_finder);
  }

  mappings = json_object_get(root, "mapping");
  if (!json_is_array(mappings)) {
    fprintf(stderr, "error: root->mappings is not an array\n");
    json_decref(root);
    return NULL;
  }
  size_t mappings_size = json_array_size(mappings);

  /* Start actually loading in the config once everything is checked */
  config = (Config *)malloc(sizeof(Config));

  if (config == NULL) {
    json_decref(root);
    return NULL;
  }
  config->num_mappings = mappings_size;
  config->num_points = mode_size;
  config->start_year = json_integer_value(start_year);
  config->output_dir = GetDirectoryString(json_string_value(output_dir));
  config->mode = mode;
  config->points = (LonLat *)malloc(sizeof(LonLat) * mode_size);
  config->mappings = (FileConfig *)malloc(sizeof(FileConfig) * mappings_size);

  if (mode == 0) {
    config->points[0].longitude = -LONGITUDE_OFFSET;
    config->points[0].latitude = LATITUDE_OFFSET;
    config->points[1].longitude = LONGITUDE_OFFSET;
    config->points[1].latitude = -LATITUDE_OFFSET;
  } else if (mode == 1) {
    LonLat *point = ArrayToLonLat(json_object_get(mode_finder, "top_left"));
    config->points[0].longitude = point->longitude;
    config->points[0].latitude = point->latitude;
    free(point);
    point = ArrayToLonLat(json_object_get(mode_finder, "bottom_right"));
    config->points[1].longitude = point->longitude;
    config->points[1].latitude = point->latitude;
    free(point);
    point = NULL;
  } else {
    fprintf(stderr, "error: Point system is not available yet.\n");
    free(config->points);
    config->points = NULL;
    free(config);
    config = NULL;
    json_decref(root);
    return NULL;
  }

  size_t index;
  json_t *value;
  json_array_foreach(mappings, index, value) {
    config->mappings[index].file_name = InsertConfigString(value, "file");
    config->mappings[index].dssat_var = InsertConfigString(value, "dssatVar");
    config->mappings[index].netcdf_var = InsertConfigString(value, "netcdfVar");
    config->mappings[index].source_unit =
        InsertConfigString(value, "sourceUnit");
    config->mappings[index].target_unit =
        InsertConfigString(value, "targetUnit");
    config->mappings[index].netcdf_id = -1;
    config->mappings[index].is_temp = 0;
    config->mappings[index].is_rh = 0;
    if (config->mappings[index].dssat_var != NULL) {
      if (strlen(config->mappings[index].dssat_var) == 4) {
        if (strncmp("TMIN", config->mappings[index].dssat_var, 4) == 0) {
          config->mappings[index].is_temp = 1;
        } else if (strncmp("TMAX", config->mappings[index].dssat_var, 4) == 0) {
          config->mappings[index].is_temp = 2;
        } else if (strncmp("HURS", config->mappings[index].dssat_var, 4) == 0) {
          //printf("HURS found in config\n");
          config->mappings[index].is_rh = 1;
        }
      }
    }
  }
  json_decref(root);
  return config;

cleanup:
  FreeConfig(config);
  json_decref(root);
  return NULL;
}

void FreeConfig(Config *config) {
  /* Prototype release of nested configuration array */
  if (config != NULL) {
    if (config->mappings != NULL) {
      free(config->mappings);
      config->mappings = NULL;
    }
    if (config->points != NULL) {
      free(config->points);
      config->points = NULL;
    }
    free(config);
    config = NULL;
  }
}
