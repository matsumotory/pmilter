#include "libmilter/mfdef.h"

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "pmilter.h"

static mrb_value pmilter_mrb_session_client_ipaddr(mrb_state *mrb, mrb_value self)
{
  pmilter_mrb_shared_state *pmilter = (pmilter_mrb_shared_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->conn->ipaddr);
}

void pmilter_mrb_session_class_init(mrb_state *mrb, struct RClass *class)
{
  struct RClass *class_session = mrb_define_class_under(mrb, class, "Session", mrb->object_class);

  mrb_define_method(mrb, class_session, "client_ipaddr", pmilter_mrb_session_client_ipaddr, MRB_ARGS_NONE());

}
