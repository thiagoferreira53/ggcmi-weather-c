#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calendar.h"

// Use only in the context of valid and invalid
enum { date_invalid = 0, date_valid = 1 };
// Use everywhere else
enum { date_ok, date_error };
static int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/*
 * This need to be able to parse,
 * 2000-02-02
 * 2000-1-2
 * 2000/02/01
 * 2000/1/2
 */
int IsLeapYear(int year) {
  if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) {
    return 1;
  } else {
    return 0;
  }
}

int ValidDateParts(const int year, const int month, const int day_of_month) {
  int day_error = 0;

  if (year < MIN_SUPPORTED_YEAR || year > MAX_SUPPORTED_YEAR) {
    fprintf(stderr, "error: year is out-of-bounds: %d-%0d-%0d\n", year, month,
            day_of_month);
    return date_invalid;
  }
  int is_leap = IsLeapYear(year);
  if (month < 1 || month > 12) {
    fprintf(stderr, "error: month is out-of-bounds: %d-%0d-%0d\n", year, month,
            day_of_month);
    return date_invalid;
  }
  if (day_of_month < 1) {
    day_error = 1;
  } else if (is_leap && month == 2) {
    if (day_of_month > 29) {
      day_error = 1;
    }
  } else if (day_of_month > month_days[month - 1]) {
    day_error = 1;
  }
  if (day_error) {
    fprintf(stderr, "error: day is out-of-bounds: %4d-%02d-%02d\n", year, month,
            day_of_month);
    return date_invalid;
  }
  return date_valid;
}

int ValidDate(const date_t *date) {
  if (ValidDateParts(date->year, date->month, date->day_of_month)) {
    if (date->is_leap == IsLeapYear(date->year)) {
      return date_valid;
    } else {
      return date_invalid;
    }
  } else {
    return date_invalid;
  }
}

int CreateDate(const int year, const int month, const int day_of_month,
               date_t *date) {
  if (ValidDateParts(year, month, day_of_month)) {
    date->year = year;
    date->is_leap = IsLeapYear(year);
    date->month = month;
    date->day_of_month = day_of_month;
    return date_ok;
  } else {
    return date_error;
  }
}

int ParseDate(const char *date_string, date_t *date) {
  int year, month, day;
  int status = sscanf(date_string, "%d-%d-%d", &year, &month, &day);
  if (status != 3) {
    fprintf(stderr, "error: could not parse date as [yyyy-mm-dd]: %s\n",
            date_string);
    return date_error;
  }
  status = CreateDate(year, month, day, date);
  if (status) {
    return date_error;
  } else {
    return date_ok;
  }
}

int AddOneDay(date_t *date) {
  date->day_of_month = date->day_of_month + 1;
  if (date->month - 1 < 0 || date->month - 1 > 12) {
    fprintf(stderr,
            "error: potential out-of-bounds with month during AddOneDay().\n");
    return date_error;
  }
  int ceil_dom = month_days[date->month - 1];
  if (date->day_of_month > ceil_dom) {
    if (date->is_leap && date->month == 2) {
      if (date->day_of_month <= 29) {
        return date_ok;
      }
    }

    date->day_of_month = 1;
    if (date->month == 12) {
      date->month = 1;
      date->year = date->year + 1;
      date->is_leap = IsLeapYear(date->year);
    } else {
      date->month = date->month + 1;
    }
  }
  if (ValidDate(date)) {
    return date_ok;
  } else {
    return date_error;
  }
}

size_t DateAsString(const date_t *date, char *dest_str) {
  size_t size = snprintf(dest_str, ISODATE_STRING_LEN, "%4d-%02d-%02d",
                         date->year, date->month, date->day_of_month);
  return size >= ISODATE_STRING_LEN;
}

int GetDOY(const date_t *date) {
  int doy = 0;
  if (date->month == 1) {
    doy = date->day_of_month;
  } else {
    for (size_t i = 0; i < date->month - 1; ++i) {
      doy += month_days[i];
    }
    doy += date->day_of_month;
    if (date->is_leap && date->month > 2) {
      doy++;
    }
  }
  return doy;
}

size_t DateAsDSSAT2String(const date_t *date, char *dest_str) {
  int year2d = date->year % 100;
  int doy = GetDOY(date);
  size_t size = snprintf(dest_str, D2DDATE_STRING_LEN, "%02d%03d", year2d, doy);
  return size >= D2DDATE_STRING_LEN;
}

size_t DateAsDSSAT4String(const date_t *date, char *dest_str) {
  int doy = GetDOY(date);
  size_t size =
      snprintf(dest_str, D4DDATE_STRING_LEN, "%d%03d", date->year, doy);
  return size >= D4DDATE_STRING_LEN;
}

size_t MonthsInDays(size_t days) {
  size_t years = days / 365;
  size_t months = years / 12;
  return months;
}
