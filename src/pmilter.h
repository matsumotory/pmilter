/*
** pmilter - A Programmable Mail Filter
**
** See Copyright Notice in LICENSE
*/

#ifndef _PMILTER_H_
#define _PMILTER_H_

#include "mruby.h"
#include "mruby/compile.h"

#include "libmilter/mfapi.h"
#include "libmilter/mfdef.h"

#define PMILTER_NAME "pmilter"
#define PMILTER_VERSION "0.0.1"
#define PMILTER_DESCRIPTION PMILTER_NAME "/" PMILTER_VERSION
#define PMILTER_VERSION_NUM 0000001

#ifndef bool
#define bool int
#define TRUE 1
#define FALSE 0
#endif /* ! bool */

#define PMILTER_CONF_UNSET NULL
#define PMILTER_UNDEFINED -2
#define PMILTER_ERROR -1
#define PMILTER_OK 0

#define DEBUG_SMFI_SYMVAL(macro_name)                                                                                  \
  fprintf(stderr, "    " #macro_name ": %s\n", smfi_getsymval(ctx, "{" #macro_name "}"))
#define DEBUG_SMFI_CHAR(val) fprintf(stderr, "    " #val ": %s\n", val)
#define DEBUG_SMFI_INT(val) fprintf(stderr, "    " #val ": %d\n", val)
#define DEBUG_SMFI_HOOK(val) fprintf(stderr, #val "\n")

#define PMILTER_GET_SYMVAL(ctx, macro_name) smfi_getsymval(ctx, "{" #macro_name "}")

typedef enum code_type_t { PMILTER_MRB_CODE_TYPE_FILE, PMILTER_MRB_CODE_TYPE_STRING } code_type;

typedef struct pmilter_mrb_code_t {

  union code {
    const char *file;
    const char *string;
  } code;
  code_type code_type;
  struct RProc *proc;
  mrbc_context *ctx;

} pmilter_mrb_code;

typedef struct connection_rec_t {

  char *hostname;
  char *ipaddr;
  const _SOCK_ADDR *hostaddr;

} connection_rec;

typedef struct smtp_header_t {

  char *key;
  unsigned char *value;

} smtp_header;

typedef struct command_rec_t {

  connection_rec *conn;
  char *connect_daemon;
  char *envelope_from;
  char *envelope_to;
  char *helohost;
  int receive_time;
  smtp_header *header;
  unsigned char *body_chunk;
  size_t body_chunk_len;

} command_rec;

typedef struct pmilter_config_t {

  int log_level;

  /* enable mruby handler functions */
  int enable_mruby_handler : 1;

  /* Sets the number of seconds libmilter will wait for an MTA connection before timing out a socket. If smfi_settimeout
   * is not called, a default timeout of 7210 seconds is used. */
  int timeout;

  /* Sets the socket through which the filter communicates with sendmail.
  ** The address of the desired communication socket. The address should be a NULL-terminated string in "proto:address"
  ** format:
  ** {unix|local}:/path/to/file -- A named pipe.
  ** inet:port@{hostname|ip-address} -- An IPV4 socket.
  ** inet6:port@{hostname|ip-address} -- An IPV6 socket.
  */
  char *listen;

  /* Sets the incoming socket backlog used by listen(2). If smfi_setbacklog is not called, the operating system default
   * is used. */
  int listen_backlog;

  /* smfi_setdbg sets the milter library's internal debugging level to a new level so that code details may be traced. A
   * level of zero turns off debugging. The greater (more positive) the level the more detailed the debugging. Six is
   * the current, highest, useful value. */
  int debug;

  /* mruby_state for init phase like postconfig */
  mrb_state *mrb;

  /* post config mruby handler */
  const char *mruby_postconfig_handler_path;
  pmilter_mrb_code *mruby_postconfig_handler;

  /* session mrub handlers */
  const char *mruby_connect_handler_path;
  const char *mruby_helo_handler_path;
  const char *mruby_envfrom_handler_path;
  const char *mruby_envrcpt_handler_path;
  const char *mruby_header_handler_path;
  const char *mruby_eoh_handler_path;
  const char *mruby_body_handler_path;
  const char *mruby_eom_handler_path;
  const char *mruby_abort_handler_path;
  const char *mruby_close_handler_path;
  const char *mruby_unknown_handler_path;
  const char *mruby_data_handler_path;

} pmilter_config;

typedef struct pmilter_state_t {

  mrb_state *mrb;

  /* commands of milter */
  command_rec *cmd;

  /* pmilter configuration */
  pmilter_config *config;

  /* return status of libmilter */
  int status;

  /* libmiler context */
  SMFICTX *ctx;

  /* hook phase tupe */

  const char *phase;

  /* mruby handler code */
  pmilter_mrb_code *mruby_connect_handler;
  pmilter_mrb_code *mruby_helo_handler;
  pmilter_mrb_code *mruby_envfrom_handler;
  pmilter_mrb_code *mruby_envrcpt_handler;
  pmilter_mrb_code *mruby_header_handler;
  pmilter_mrb_code *mruby_eoh_handler;
  pmilter_mrb_code *mruby_body_handler;
  pmilter_mrb_code *mruby_eom_handler;
  pmilter_mrb_code *mruby_abort_handler;
  pmilter_mrb_code *mruby_close_handler;
  pmilter_mrb_code *mruby_unknown_handler;
  pmilter_mrb_code *mruby_data_handler;

} pmilter_state;

void pmilter_mrb_class_init(mrb_state *mrb);

#endif // PMILTER_H
