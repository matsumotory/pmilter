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
#include "mruby/hash.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "pmilter.h"

/* use libmilter object via mruby user data */
static mrb_value pmilter_mrb_session_client_ipaddr(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->conn->ipaddr);
}

static mrb_value pmilter_mrb_session_client_hostname(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->conn->hostname);
}

/* use smfi_getsymval and ctx */
static mrb_value pmilter_mrb_session_client_daemon(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{daemon_name}"));
}

/* mruby hooked pahse name */
static mrb_value pmilter_mrb_session_phase_name(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->phase);
}

/* get value from command_rec */
static mrb_value pmilter_mrb_session_helo_hostname(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->helohost);
}

static mrb_value pmilter_mrb_session_envelope_from(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->envelope_from);
}

static mrb_value pmilter_mrb_session_envelope_to(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, pmilter->cmd->envelope_to);
}

static mrb_value pmilter_mrb_session_receive_time(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_fixnum_value(pmilter->cmd->receive_time);
}

static mrb_value pmilter_mrb_session_body_chunk(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new(mrb, pmilter->cmd->body_chunk, pmilter->cmd->body_chunk_len);
}

static mrb_value pmilter_mrb_session_header(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;
  mrb_value hash = mrb_hash_new(mrb);

  mrb_hash_set(mrb, hash, mrb_str_new_cstr(mrb, pmilter->cmd->header->key),
               mrb_str_new_cstr(mrb, pmilter->cmd->header->value));

  return hash;
}

static mrb_value pmilter_mrb_session_add_header(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;
  mrb_value key, val;
  int ret;

  mrb_get_args(mrb, "oo", &key, &val);

  ret = smfi_addheader(pmilter->ctx, mrb_str_to_cstr(mrb, key), mrb_str_to_cstr(mrb, val));

  return mrb_fixnum_value(ret);
}

/* SASL authentication */
static mrb_value pmilter_mrb_session_auth_type(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{auth_type}"));
}

static mrb_value pmilter_mrb_session_auth_authen(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{auth_authen}"));
}

static mrb_value pmilter_mrb_session_auth_author(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{auth_author}"));
}

static mrb_value pmilter_mrb_session_cert_issuer(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{cert_issuer}"));
}

static mrb_value pmilter_mrb_session_cert_subject(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{cert_subject}"));
}

static mrb_value pmilter_mrb_session_cipher_bits(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{cipher_bits}"));
}

static mrb_value pmilter_mrb_session_cipher(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{cipher}"));
}

static mrb_value pmilter_mrb_session_tls_version(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{tls_version}"));
}

static mrb_value pmilter_mrb_session_myhostname(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{j}"));
}

static mrb_value pmilter_mrb_session_message_id(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{i}"));
}

static mrb_value pmilter_mrb_session_mail_addr(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{mail_addr}"));
}

static mrb_value pmilter_mrb_session_rcpt_addr(mrb_state *mrb, mrb_value self)
{
  pmilter_state *pmilter = (pmilter_state *)mrb->ud;

  return mrb_str_new_cstr(mrb, smfi_getsymval(pmilter->ctx, "{rcpt_addr}"));
}

void pmilter_mrb_session_class_init(mrb_state *mrb, struct RClass *class)
{
  struct RClass *class_session = mrb_define_class_under(mrb, class, "Session", mrb->object_class);
  struct RClass *class_headers = mrb_define_class_under(mrb, class_session, "Headers", mrb->object_class);

  /* connect phase */
  mrb_define_method(mrb, class_session, "client_ipaddr", pmilter_mrb_session_client_ipaddr, MRB_ARGS_NONE());
  mrb_define_method(mrb, class_session, "client_hostname", pmilter_mrb_session_client_hostname, MRB_ARGS_NONE());
  mrb_define_method(mrb, class_session, "client_daemon", pmilter_mrb_session_client_daemon, MRB_ARGS_NONE());
  mrb_define_method(mrb, class_session, "handler_phase_name", pmilter_mrb_session_phase_name, MRB_ARGS_NONE());

  /* helo phase */
  mrb_define_method(mrb, class_session, "helo_hostname", pmilter_mrb_session_helo_hostname, MRB_ARGS_NONE());

  /* sender address from callback args */
  mrb_define_method(mrb, class_session, "envelope_from", pmilter_mrb_session_envelope_from, MRB_ARGS_NONE());
  /* reviever address from callback args */
  mrb_define_method(mrb, class_session, "envelope_to", pmilter_mrb_session_envelope_to, MRB_ARGS_NONE());

  mrb_define_method(mrb, class_session, "receive_time", pmilter_mrb_session_receive_time, MRB_ARGS_NONE());
  
  /* sender address from symval */
  mrb_define_method(mrb, class_session, "mail_addr", pmilter_mrb_session_mail_addr, MRB_ARGS_NONE());
  /* reciver address from symbal */
  mrb_define_method(mrb, class_session, "rcpt_addr", pmilter_mrb_session_rcpt_addr, MRB_ARGS_NONE());

  /* data phase */
  mrb_define_method(mrb, class_session, "body_chunk", pmilter_mrb_session_body_chunk, MRB_ARGS_NONE());

  /* all phase */
  mrb_define_method(mrb, class_session, "myhostname", pmilter_mrb_session_myhostname, MRB_ARGS_NONE());

  /* data and eom message */
  mrb_define_method(mrb, class_session, "message_id", pmilter_mrb_session_message_id, MRB_ARGS_NONE());

  /* mail, data, eom phase useing SASL auth */
  /* SASL login method */
  mrb_define_method(mrb, class_session, "auth_type", pmilter_mrb_session_auth_type, MRB_ARGS_NONE());
  /* SASL login name */
  mrb_define_method(mrb, class_session, "auth_authen", pmilter_mrb_session_auth_authen, MRB_ARGS_NONE());
  /* SASL login sender */
  mrb_define_method(mrb, class_session, "auth_author", pmilter_mrb_session_auth_author, MRB_ARGS_NONE());

  /* helo, mail, data, eom phase using TLS */
  /* tls client cert issuserr */
  mrb_define_method(mrb, class_session, "cert_issuer", pmilter_mrb_session_cert_issuer, MRB_ARGS_NONE());
  /* tls client cert subject */
  mrb_define_method(mrb, class_session, "cert_subject", pmilter_mrb_session_cert_subject, MRB_ARGS_NONE());
  /* tls session key size */
  mrb_define_method(mrb, class_session, "cipher_bits", pmilter_mrb_session_cipher_bits, MRB_ARGS_NONE());
  /* tls encryp method*/
  mrb_define_method(mrb, class_session, "cipher", pmilter_mrb_session_cipher, MRB_ARGS_NONE());
  /* tls version */
  mrb_define_method(mrb, class_session, "tls_version", pmilter_mrb_session_tls_version, MRB_ARGS_NONE());

  /* Pmilter::Session::Heaers */
  mrb_define_method(mrb, class_headers, "header", pmilter_mrb_session_header, MRB_ARGS_NONE());

  /*
  **  The xxfi_eom routine is called at the end of a message (essentially,
  **  after the final DATA dot). This routine can call some special routines
  **  to modify the envelope, header, or body of the message before the
  **  message is enqueued. These routines must not be called from any vendor
  **  routine other than xxfi_eom.
  */

  mrb_define_method(mrb, class_headers, "[]=", pmilter_mrb_session_add_header, MRB_ARGS_NONE());
}
