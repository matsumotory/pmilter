#ifndef PMILTER_H
#define PMILTER_H

#include "mruby.h"

#define PMILTER_NAME "pmilter"
#define PMILTER_VERSION_STR "0.0.1"
#define PMILTER_VERSION 0000001

#ifndef bool
#define bool int
#define TRUE 1
#define FALSE 0
#endif /* ! bool */

#define PMILTER_CONF_UNSET NULL
#define PMILTER_UNDEFINED -2
#define PMILTER_ERROR -1
#define PMILTER_OK 0

void pmilter_mrb_class_init(mrb_state *mrb);

#endif // NGX_STREAM_MRUBY_INIT_H
