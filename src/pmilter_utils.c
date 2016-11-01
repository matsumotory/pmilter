#include <arpa/inet.h>
#include <string.h>
#include "libmilter/mfdef.h"
#include "pmilter.h"
#include "pmilter_log.h"

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
