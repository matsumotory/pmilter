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

#define pmilter_log_error(level, config, fmt, ...)                                                                     \
  if ((config)->log_level >= level)                                                                                    \
  pmilter_log_core_error(level, fmt, ##__VA_ARGS__)

int pmilter_get_log_level(char *level_str);
void pmilter_log_core_error(int level, const char *fmt, ...);

#endif /* _PMILTER_LOG_H_ */
