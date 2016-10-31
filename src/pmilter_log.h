#ifndef _PMILTER_LOG_H_
#define _PMILTER_LOG_H_

#define PMILTER_LOG_EMERG 0
#define PMILTER_LOG_ALERT 1
#define PMILTER_LOG_CRIT 2
#define PMILTER_LOG_ERR 3
#define PMILTER_LOG_WARN 4
#define PMILTER_LOG_NOTICE 5
#define PMILTER_LOG_INFO 6
#define PMILTER_LOG_DEBUG 7

const char *err_levels[] = {"emerg", "alert", "crit", "error", "warn", "notice", "info", "debug"};

#define pmilter_log_error(level, config, fmt, ...)                                                                     \
  if ((config)->log_level >= level)                                                                                    \
  fprintf(stderr, "[%s] " fmt "\n", err_levels[level], ##__VA_ARGS__)

#endif /* _PMILTER_LOG_H_ */
