/*
** pmilter - A Programmable Mail Filter
**
** See Copyright Notice in LICENSE
*/

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

static mrb_value pmilter_mrb_set_status(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = mrb->ud;
  mrb_int status;

  mrb_get_args(mrb, "i", &status);

  pmilter->status = status;

  return mrb_fixnum_value(status);
}

#define PMILTER_DEFINE_CONST(val) mrb_define_const(mrb, class, #val, mrb_fixnum_value(val));

void pmilter_mrb_core_class_init(mrb_state *mrb, struct RClass *class)
{

  /*
**  Continue processing message/connection.
*/

  PMILTER_DEFINE_CONST(SMFIS_CONTINUE);

  /*
  **  Reject the message/connection.
  **  No further routines will be called for this message
  **  (or connection, if returned from a connection-oriented routine).
  */

  PMILTER_DEFINE_CONST(SMFIS_REJECT);

  /*
  **  Accept the message,
  **  but silently discard the message.
  **  No further routines will be called for this message.
  **  This is only meaningful from message-oriented routines.
  */

  PMILTER_DEFINE_CONST(SMFIS_DISCARD);

  /*
  **  Accept the message/connection.
  **  No further routines will be called for this message
  **  (or connection, if returned from a connection-oriented routine;
  **  in this case, it causes all messages on this connection
  **  to be accepted without filtering).
  */

  PMILTER_DEFINE_CONST(SMFIS_ACCEPT);

  /*
  **  Return a temporary failure, i.e.,
  **  the corresponding SMTP command will return a 4xx status code.
  **  In some cases this may prevent further routines from
  **  being called on this message or connection,
  **  although in other cases (e.g., when processing an envelope
  **  recipient) processing of the message will continue.
  */

  PMILTER_DEFINE_CONST(SMFIS_TEMPFAIL);

  /*
  **  Do not send a reply to the MTA
  */

  PMILTER_DEFINE_CONST(SMFIS_NOREPLY);

  /*
  **  Skip over rest of same callbacks, e.g., body.
  */

  PMILTER_DEFINE_CONST(SMFIS_SKIP);

  /* xxfi_negotiate: use all existing protocol options/actions */

  PMILTER_DEFINE_CONST(SMFIS_ALL_OPTS);

  mrb_define_class_method(mrb, class, "name", pmilter_mrb_get_name, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, class, "status=", pmilter_mrb_set_status, MRB_ARGS_REQ(1));
}
