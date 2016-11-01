#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pmilter_log.h"

static const char *err_levels[] = {"emerg", "alert", "crit", "error", "warn", "notice", "info", "debug"};

int pmilter_get_log_level(char *level_str)
{ 
  int i;
  int level = PMILTER_LOG_WARN;

  for (i = 0; i < sizeof(err_levels) / sizeof(const char *); i++) {
    if (strcmp(level_str, err_levels[i]) == 0) {
      level = i;
    }
  } 
  return level;
}

void pmilter_log_core_error(int level, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  /* TODO: add time stamp */
  fprintf(stderr, "[%s] ", err_levels[level]);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

