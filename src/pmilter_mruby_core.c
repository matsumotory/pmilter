#include "libmilter/mfdef.h"

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "pmilter.h"

static mrb_value pmilter_mrb_get_name(mrb_state *mrb, mrb_value self)
{
  return mrb_str_new_lit(mrb, PMILTER_NAME);
}

#define PMILTER_DEFINE_CONST(val)  mrb_define_const(mrb, class, #val, mrb_fixnum_value(val));

void pmilter_mrb_core_class_init(mrb_state *mrb, struct RClass *class)
{

  PMILTER_DEFINE_CONST(SMFIR_ADDRCPT);
  PMILTER_DEFINE_CONST(SMFIR_DELRCPT);
  PMILTER_DEFINE_CONST(SMFIR_ADDRCPT_PAR);
  PMILTER_DEFINE_CONST(SMFIR_SHUTDOWN);
  PMILTER_DEFINE_CONST(SMFIR_ACCEPT);
  PMILTER_DEFINE_CONST(SMFIR_REPLBODY);
  PMILTER_DEFINE_CONST(SMFIR_CONTINUE);
  PMILTER_DEFINE_CONST(SMFIR_DISCARD);
  PMILTER_DEFINE_CONST(SMFIR_CHGFROM);
  PMILTER_DEFINE_CONST(SMFIR_CONN_FAIL);
  PMILTER_DEFINE_CONST(SMFIR_ADDHEADER);
  PMILTER_DEFINE_CONST(SMFIR_INSHEADER);
  PMILTER_DEFINE_CONST(SMFIR_SETSYMLIST);
  PMILTER_DEFINE_CONST(SMFIR_CHGHEADER);
  PMILTER_DEFINE_CONST(SMFIR_PROGRESS);
  PMILTER_DEFINE_CONST(SMFIR_QUARANTINE);
  PMILTER_DEFINE_CONST(SMFIR_REJECT);
  PMILTER_DEFINE_CONST(SMFIR_SKIP);
  PMILTER_DEFINE_CONST(SMFIR_TEMPFAIL);
  PMILTER_DEFINE_CONST(SMFIR_REPLYCODE);


  mrb_define_class_method(mrb, class, "name", pmilter_mrb_get_name, MRB_ARGS_NONE());
}
