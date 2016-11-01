#include "pmilter.h"
#include "libmilter/mfdef.h"
#include "pmilter_log.h"
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

static const char *MONTH[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static const char *DAY_OF_WEEK[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static char *dig_memcpy(char *buf, int n, size_t len)
{
  char *p;

  p = buf + len - 1;
  do {
    // dig to str
    *p-- = (n % 10) + '0';
    n /= 10;
  } while (p >= buf);

  return buf + len;
}

// Sat, 27 Dec 2014 08:30:29 GMT
void pmilter_http_date_str(time_t *time, char *date)
{
  struct tm t;
  char *p = date;

  if (gmtime_r(time, &t) == NULL) {
    return;
  }

  memcpy(p, DAY_OF_WEEK[t.tm_wday], 3);
  p += 3;
  *p++ = ',';
  *p++ = ' ';
  p = dig_memcpy(p, t.tm_mday, 2);
  *p++ = ' ';
  memcpy(p, MONTH[t.tm_mon], 3);
  p += 3;
  *p++ = ' ';
  p = dig_memcpy(p, t.tm_year + 1900, 4);
  *p++ = ' ';
  p = dig_memcpy(p, t.tm_hour, 2);
  *p++ = ':';
  p = dig_memcpy(p, t.tm_min, 2);
  *p++ = ':';
  p = dig_memcpy(p, t.tm_sec, 2);
  memcpy(p, " GMT", 4);
  p += 4;
  *p = '\0';
}

char *pmilter_ipaddrdup(pmilter_state *pmilter, const char *hostname, const _SOCK_ADDR *hostaddr)
{
  char addr_buf[INET6_ADDRSTRLEN];

  switch (hostaddr->sa_family) {
  case AF_INET: {
    struct sockaddr_in *sin = (struct sockaddr_in *)hostaddr;
    if (NULL == inet_ntop(AF_INET, &(sin->sin_addr), addr_buf, INET_ADDRSTRLEN)) {
      pmilter_log_error(PMILTER_LOG_ERR, pmilter->config, "inet_ntop AF_INET4 failed\n");
      return NULL;
    }
    break;
  }
  case AF_INET6: {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)hostaddr;
    if (NULL == inet_ntop(AF_INET6, &(sin6->sin6_addr), addr_buf, INET6_ADDRSTRLEN)) {
      pmilter_log_error(PMILTER_LOG_ERR, pmilter->config, "inet_ntop AF_INET6 failed\n");
      return NULL;
    }
    break;
  }
  default:
    pmilter_log_error(PMILTER_LOG_ERR, pmilter->config, "Unknown protocol: sa_familyr=%d\n", hostaddr->sa_family);
    return NULL;
  }

  return strdup(addr_buf);
}
