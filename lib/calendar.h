#ifndef GGCMI_WTH_GEN__CALENDAR_H_
#define GGCMI_WTH_GEN__CALENDAR_H_
#include <stdio.h>

#define MIN_SUPPORTED_YEAR 1000
#define MAX_SUPPORTED_YEAR 3000
#define ISODATE_STRING_LEN 11
#define D2DDATE_STRING_LEN 6
#define D4DDATE_STRING_LEN 8

typedef struct date {
  int year;
  int month;
  int day_of_month;
  int is_leap;

} date_t;

int ValidDateParts(const int year, const int month, const int day_of_month);
int ValidDate(const date_t *date);
int CreateDate(int year, int month, int day_of_month, date_t *date);
int ParseDate(const char *date_string, date_t *date);
int AddOneDay(date_t *date);
int GetDOY(const date_t *date);
size_t DateAsString(const date_t *date, char *dest_str);
size_t DateAsDSSAT2String(const date_t *date, char *dest_str);
size_t DateAsDSSAT4String(const date_t *date, char *dest_str);
size_t MonthsInDays(size_t days);
#endif // GGCMI_WTH_GEN__CALENDAR_H_
