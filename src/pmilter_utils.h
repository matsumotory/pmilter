#ifndef _PMILTER_UTILS_H_
#define _PMILTER_UTILS_H_

#include "libmilter/mfdef.h"
#include "pmilter.h"

char *pmilter_ipaddrdup(pmilter_state *pmilter, const char *hostname, const _SOCK_ADDR *hostaddr);

void pmilter_http_date_str(time_t *time, char *date);

#endif /* _PMILTER_UTILS_H_ */
