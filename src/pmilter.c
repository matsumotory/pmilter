/*
** pmilter - A Programmable Mail Filter
**
** See Copyright Notice in LICENSE
*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "pthread.h"

#include "libmilter/mfapi.h"
#include "libmilter/mfdef.h"

#include "toml.h"
#include "toml_private.h"

#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/string.h"

#include "pmilter.h"
#include "pmilter_config.h"
#include "pmilter_log.h"
#include "pmilter_utils.h"

#define PMILTER_CODE_MRBC_CONTEXT_FREE(mrb, code)                                                                      \
  if (code != PMILTER_CONF_UNSET && mrb && (code)->ctx) {                                                              \
    mrbc_context_free(mrb, (code)->ctx);                                                                               \
    (code)->ctx = NULL;                                                                                                \
  }

extern sfsistat mrb_xxfi_cleanup(SMFICTX *, bool);
static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

/* mruby functions */
static void pmilter_mrb_raise_error(mrb_state *mrb, mrb_value obj)
{
  struct RString *str;
  char *err_out;

  obj = mrb_funcall(mrb, obj, "inspect", 0);
  if (mrb_type(obj) == MRB_TT_STRING) {
    str = mrb_str_ptr(obj);
    err_out = str->as.heap.ptr;
    fprintf(stderr, "mrb_run failed: error: %s", err_out);
  }
}

static void pmilter_mrb_state_clean(mrb_state *mrb)
{
  mrb->exc = 0;
}

static pmilter_mrb_code *pmilter_mrb_code_from_file(const char *file_path)
{
  pmilter_mrb_code *code;

  if (file_path == NULL) {
    return NULL;
  }

  /* need free */
  code = malloc(sizeof(pmilter_mrb_code));
  if (code == NULL) {
    return NULL;
  }

  code->code.file = file_path;
  code->code_type = PMILTER_MRB_CODE_TYPE_FILE;

  return code;
}

static pmilter_mrb_code *pmilter_mrb_code_from_string(const char *code_string)
{
  pmilter_mrb_code *code;

  /* need free */
  code = malloc(sizeof(pmilter_mrb_code));
  if (code == NULL) {
    return NULL;
  }

  code->code.string = code_string;
  code->code_type = PMILTER_MRB_CODE_TYPE_STRING;

  return code;
}

static int pmilter_state_compile_internal(mrb_state *mrb, pmilter_config *config, pmilter_mrb_code *code)
{
  FILE *mrb_file;
  struct mrb_parser_state *p;

  if (code == NULL) {
    return PMILTER_ERROR;
  }

  if (code->code_type == PMILTER_MRB_CODE_TYPE_FILE) {
    if ((mrb_file = fopen(code->code.file, "r")) == NULL) {
      fprintf(stderr, "open failed handler mruby path: %s\n", code->code.file);
      return PMILTER_ERROR;
    }

    code->ctx = mrbc_context_new(mrb);
    mrbc_filename(mrb, code->ctx, (char *)code->code.file);
    p = mrb_parse_file(mrb, mrb_file, code->ctx);
    fclose(mrb_file);
  } else {
    code->ctx = mrbc_context_new(mrb);
    mrbc_filename(mrb, code->ctx, "INLINE CODE");
    p = mrb_parse_string(mrb, (char *)code->code.string, code->ctx);
  }

  if (p == NULL || (0 < p->nerr)) {
    return PMILTER_ERROR;
  }

  code->proc = mrb_generate_code(mrb, p);
  mrb_pool_close(p->pool);
  if (code->proc == NULL) {
    return PMILTER_ERROR;
  }

  if (code->code_type == PMILTER_MRB_CODE_TYPE_FILE) {
    pmilter_log_error(PMILTER_LOG_DEBUG, config, "%s:%d: compile info: code->code.file=(%s)", __func__, __LINE__,
                      code->code.file);
  } else {
    pmilter_log_error(PMILTER_LOG_DEBUG, config, "%s:%d: compile info: "
                                                 "code->code.string=(%s)",
                      __func__, __LINE__, code->code.string);
  }

  return PMILTER_OK;
}

static int pmilter_state_compile(pmilter_state *pmilter, pmilter_mrb_code *code)
{
  return pmilter_state_compile_internal(pmilter->mrb, pmilter->config, code);
}

/* pmilter mruby handlers */
#define PMILTER_ADD_MRUBY_HADNLER(hook_phase)                                                                          \
  static int pmilter_##hook_phase##_handler(pmilter_state *pmilter)                                                    \
  {                                                                                                                    \
    mrb_state *mrb = pmilter->mrb;                                                                                     \
    mrb_int ai = mrb_gc_arena_save(mrb);                                                                               \
                                                                                                                       \
    pmilter->status = SMFIS_CONTINUE;                                                                                  \
    pmilter->phase = "mruby_" #hook_phase "_handler";                                                                  \
                                                                                                                       \
    mrb->ud = pmilter;                                                                                                 \
    mrb_run(mrb, pmilter->mruby_##hook_phase##_handler->proc, mrb_top_self(mrb));                                      \
                                                                                                                       \
    if (mrb->exc) {                                                                                                    \
      pmilter_mrb_raise_error(mrb, mrb_obj_value(mrb->exc));                                                           \
      pmilter->status = SMFIS_TEMPFAIL;                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    PMILTER_CODE_MRBC_CONTEXT_FREE(mrb, pmilter->mruby_##hook_phase##_handler);                                        \
    pmilter_mruby_code_free(pmilter->mruby_##hook_phase##_handler);                                                    \
    pmilter_mrb_state_clean(mrb);                                                                                      \
    mrb_gc_arena_restore(mrb, ai);                                                                                     \
                                                                                                                       \
    return pmilter->status;                                                                                            \
  }

PMILTER_ADD_MRUBY_HADNLER(connect)
PMILTER_ADD_MRUBY_HADNLER(helo)
PMILTER_ADD_MRUBY_HADNLER(envfrom)
PMILTER_ADD_MRUBY_HADNLER(envrcpt)
PMILTER_ADD_MRUBY_HADNLER(header)
PMILTER_ADD_MRUBY_HADNLER(eoh)
PMILTER_ADD_MRUBY_HADNLER(body)
PMILTER_ADD_MRUBY_HADNLER(eom)
PMILTER_ADD_MRUBY_HADNLER(abort)
PMILTER_ADD_MRUBY_HADNLER(close)
PMILTER_ADD_MRUBY_HADNLER(unknown)
PMILTER_ADD_MRUBY_HADNLER(data)

static inline void pmilter_config_handler_free_inner(mrb_state *mrb, pmilter_mrb_code *code)
{
  PMILTER_CODE_MRBC_CONTEXT_FREE(mrb, code);
  pmilter_mruby_code_free(code);
  pmilter_mrb_state_clean(mrb);
}

static inline int pmilter_config_handler_inner(mrb_state *mrb, pmilter_mrb_code *code)
{
  int status;

  mrb_run(mrb, code->proc, mrb_top_self(mrb));

  if (mrb->exc) {
    pmilter_mrb_raise_error(mrb, mrb_obj_value(mrb->exc));
    status = PMILTER_ERROR;
  } else {
    status = PMILTER_OK;
  }

  return status;
}

static void pmilter_postconfig_handler_free(pmilter_config *c)
{
  pmilter_config_handler_free_inner(c->mrb, c->mruby_postconfig_handler);
}

static int pmilter_postconfig_handler(pmilter_config *c)
{
  return pmilter_config_handler_inner(c->mrb, c->mruby_postconfig_handler);
}

static void pmilter_config_handler_init(pmilter_config *config)
{
  config->mrb = mrb_open();
}

static void pmilter_postconfig_handler_run(pmilter_config *config)
{
  config->mruby_postconfig_handler = pmilter_mrb_code_from_file(config->mruby_postconfig_handler_path);

  if (config->mruby_postconfig_handler != NULL && config->mrb != NULL) {
    int ret;
    ret = pmilter_state_compile_internal(config->mrb, config, config->mruby_postconfig_handler);
    if (ret == PMILTER_ERROR) {
      pmilter_log_error(PMILTER_LOG_ERR, config, "postconfig handler compile failed");
      exit(EX_SOFTWARE);
    }

    ret = pmilter_postconfig_handler(config);
    if (ret == PMILTER_ERROR) {
      pmilter_log_error(PMILTER_LOG_ERR, config, "postconfig handler run failed");
      exit(EX_SOFTWARE);
    }
  }
}

static command_rec *pmilter_command_init()
{
  command_rec *cmd;
  connection_rec *conn;
  smtp_header *header;

  /* need free */
  cmd = (command_rec *)calloc(1, sizeof(command_rec));
  if (cmd == NULL) {
    return NULL;
  }

  /* need free */
  conn = (connection_rec *)calloc(1, sizeof(connection_rec));
  if (conn == NULL) {
    return NULL;
  }

  /* need free */
  header = (smtp_header *)calloc(1, sizeof(smtp_header));
  if (header == NULL) {
    return NULL;
  }

  cmd->conn = conn;
  cmd->conn->ipaddr = NULL;
  cmd->conn->hostname = NULL;
  cmd->connect_daemon = NULL;
  cmd->helohost = NULL;
  cmd->envelope_from = NULL;
  cmd->envelope_to = NULL;
  cmd->receive_time = 0;
  cmd->header = header;
  cmd->header->key = NULL;
  cmd->header->value = NULL;
  cmd->body_chunk = NULL;
  cmd->body_chunk_len = 0;

  return cmd;
}

/* connection info filter */
/* was not called on same connetion */
sfsistat mrb_xxfi_connect(ctx, hostname, hostaddr) SMFICTX *ctx;
char *hostname;
_SOCK_ADDR *hostaddr;
{
  pmilter_state *pmilter;
  pmilter_config *config = smfi_getpriv(ctx);
  int ret;

  pmilter = pmilter_create_conf(config);
  pmilter->status = SMFIS_CONTINUE;
  pmilter->ctx = ctx;
  pmilter->cmd = pmilter_command_init();
  if (pmilter->cmd == NULL) {
    return SMFIS_TEMPFAIL;
  }

  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_connect_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_connect_handler_path);

  if (pmilter->mrb != NULL) {
    pmilter->cmd->connect_daemon = smfi_getsymval(ctx, "{daemon_name}");
    pmilter->cmd->conn->hostaddr = hostaddr;
    pmilter->cmd->conn->hostname = strdup(hostname);
    pmilter->cmd->conn->ipaddr = pmilter_ipaddrdup(pmilter, hostname, hostaddr);
    if (pmilter->cmd->conn->ipaddr == NULL) {
      return SMFIS_TEMPFAIL;
    }
  }

  if (pmilter->mruby_connect_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_connect_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }

    pmilter->status = pmilter_connect_handler(pmilter);
  }

  smfi_setpriv(ctx, pmilter);

  return pmilter->status;
}

/* SMTP HELO command filter */
/* was not called on same connetion */
sfsistat mrb_xxfi_helo(ctx, helohost) SMFICTX *ctx;
char *helohost;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_helo_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_helo_handler_path);

  if (pmilter->mrb != NULL) {
    pmilter->cmd->helohost = strdup(helohost);
  }

  if (pmilter->mruby_helo_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_helo_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_helo_handler(pmilter);
  }

  return pmilter->status;
}

/* envelope sender filter */
/* call from this phase on same connection */
sfsistat mrb_xxfi_envfrom(ctx, argv) SMFICTX *ctx;
char **argv;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_envfrom_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_envfrom_handler_path);

  if (pmilter->mrb != NULL) {
    /* need free */
    pmilter->cmd->envelope_from = strdup(argv[0]);
  }

  if (pmilter->mruby_envfrom_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_envfrom_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_envfrom_handler(pmilter);
  }

  return pmilter->status;
}

/* envelope recipient filter */
sfsistat mrb_xxfi_envrcpt(ctx, argv) SMFICTX *ctx;
char **argv;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_envrcpt_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_envrcpt_handler_path);

  if (pmilter->mrb != NULL) {
    pmilter->cmd->envelope_to = smfi_getsymval(ctx, "{rcpt_addr}");
  }

  if (pmilter->mruby_envrcpt_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_envrcpt_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_envrcpt_handler(pmilter);
  }

  return pmilter->status;
}

/* header filter */
sfsistat mrb_xxfi_header(ctx, headerf, headerv) SMFICTX *ctx;
char *headerf;
unsigned char *headerv;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_header_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_header_handler_path);

  if (pmilter->mrb != NULL) {
    pmilter->cmd->header->key = headerf;
    pmilter->cmd->header->value = headerv;
  }

  if (pmilter->mruby_header_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_header_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_header_handler(pmilter);
  }

  return pmilter->status;
}

/* end of header */
sfsistat mrb_xxfi_eoh(ctx) SMFICTX *ctx;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_eoh_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_eoh_handler_path);

  if (pmilter->mruby_eoh_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_eoh_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_eoh_handler(pmilter);
  }

  return pmilter->status;
}

/* body block filter */
sfsistat mrb_xxfi_body(ctx, bodyp, bodylen) SMFICTX *ctx;
unsigned char *bodyp;
size_t bodylen;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_body_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_body_handler_path);

  if (pmilter->mruby_body_handler != NULL && pmilter->mrb != NULL) {
    pmilter->cmd->body_chunk = bodyp;
    pmilter->cmd->body_chunk_len = bodylen;
    ret = pmilter_state_compile(pmilter, pmilter->mruby_body_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_body_handler(pmilter);
  }

  return pmilter->status;
}

/* free record without conenct and helo phase */
void command_rec_free_per_session(command_rec *cmd)
{
  if (cmd->envelope_from != NULL) {
    free(cmd->envelope_from);
  }
  cmd->envelope_from = NULL;
  cmd->envelope_to = NULL;
  cmd->receive_time = 0;
  cmd->header->key = NULL;
  cmd->header->value = NULL;
}

/* end of message */
sfsistat mrb_xxfi_eom(ctx) SMFICTX *ctx;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;
  time_t accept_time;
  bool ok = TRUE;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_eom_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_eom_handler_path);

  if (pmilter->mrb != NULL) {
    time(&accept_time);
    pmilter->cmd->receive_time = accept_time;
  }

  if (pmilter->mruby_eom_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_eom_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_eom_handler(pmilter);
    command_rec_free_per_session(pmilter->cmd);
  }

  return pmilter->status;
}

/* message aborted */
sfsistat mrb_xxfi_abort(ctx) SMFICTX *ctx;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_abort_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_abort_handler_path);

  if (pmilter->mruby_abort_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_abort_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_abort_handler(pmilter);
  }

  return mrb_xxfi_cleanup(ctx, FALSE);
}

/* session cleanup */
sfsistat mrb_xxfi_cleanup(ctx, ok) SMFICTX *ctx;
bool ok;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);

  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  return pmilter->status;
}

/* connection cleanup */
/* was not called on same connetion */
sfsistat mrb_xxfi_close(ctx) SMFICTX *ctx;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  pmilter_config *config = pmilter->config;
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_close_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_close_handler_path);

  if (pmilter->mruby_close_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_close_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_close_handler(pmilter);
  }

  if (pmilter->mrb != NULL) {
    pmilter_delete_conf(pmilter);
  }
  smfi_setpriv(ctx, config);

  return pmilter->status;
}

/* Once, at the start of each SMTP connection */
sfsistat mrb_xxfi_unknown(ctx, scmd) SMFICTX *ctx;
char *scmd;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_unknown_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_unknown_handler_path);

  if (pmilter->mruby_unknown_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_unknown_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_unknown_handler(pmilter);
  }

  return pmilter->status;
}

/* DATA command */
sfsistat mrb_xxfi_data(ctx) SMFICTX *ctx;
{
  pmilter_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->status = SMFIS_CONTINUE;
  pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "=== %s ===", __func__);

  pmilter->mruby_data_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_data_handler_path);

  if (pmilter->mruby_data_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_state_compile(pmilter, pmilter->mruby_data_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter->status = pmilter_data_handler(pmilter);
  }

  return pmilter->status;
}

/* Once, at the start of each SMTP connection */
sfsistat mrb_xxfi_negotiate(ctx, f0, f1, f2, f3, pf0, pf1, pf2, pf3) SMFICTX *ctx;
unsigned long f0;
unsigned long f1;
unsigned long f2;
unsigned long f3;
unsigned long *pf0;
unsigned long *pf1;
unsigned long *pf2;
unsigned long *pf3;
{
  pmilter_config *config = smfi_getpriv(ctx);

  pmilter_log_error(PMILTER_LOG_DEBUG, config, "=== %s ===", __func__);

  return SMFIS_ALL_OPTS;
}

struct smfiDesc smfilter = {
    "pmilter",                     /* filter name */
    SMFI_VERSION,                  /* version code */
    SMFIF_ADDHDRS | SMFIF_ADDRCPT, /* flags */
    mrb_xxfi_connect,              /* connection info filter */
    mrb_xxfi_helo,                 /* SMTP HELO command filter */
    mrb_xxfi_envfrom,              /* envelope sender filter */
    mrb_xxfi_envrcpt,              /* envelope recipient filter */
    mrb_xxfi_header,               /* header filter */
    mrb_xxfi_eoh,                  /* end of header */
    mrb_xxfi_body,                 /* body block filter */
    mrb_xxfi_eom,                  /* end of message */
    mrb_xxfi_abort,                /* message aborted */
    mrb_xxfi_close,                /* connection cleanup */
    mrb_xxfi_unknown,              /* unknown SMTP commands */
    mrb_xxfi_data,                 /* DATA command */
    mrb_xxfi_negotiate             /* Once, at the start of each SMTP connection */
};

int main(argc, argv) int argc;
char **argv;
{
  pmilter_config *pmilter_config;
  const char *args = "c:h";
  extern char *optarg;
  struct toml_node *toml_root;
  char *file = NULL;
  int c, smfi_status;

  while ((c = getopt(argc, argv, args)) != -1) {
    switch (c) {
    case 'c':
      file = optarg;
      break;
    case 'h':
    default:
      pmilter_usage(argv[0]);
      exit(EX_USAGE);
    }
  }

  toml_root = pmilter_toml_load(file, argv);

  /* pmilter config setup */
  pmilter_config = pmilter_config_init();
  pmilter_config_parse(pmilter_config, toml_root);

  /* libmilter configurations */
  if (smfi_setconn(pmilter_config->listen) == MI_FAILURE) {
    pmilter_log_error(PMILTER_LOG_ERR, pmilter_config, "smfi_setconn failed: port or socket already exists?");
    exit(EX_SOFTWARE);
  }
  if (smfi_settimeout(pmilter_config->timeout) == MI_FAILURE) {
    pmilter_log_error(PMILTER_LOG_ERR, pmilter_config, "smfi_settimeout failed");
    exit(EX_SOFTWARE);
  }
  if (smfi_setbacklog(pmilter_config->listen_backlog) == MI_FAILURE) {
    pmilter_log_error(PMILTER_LOG_ERR, pmilter_config, "smfi_setbacklog failed");
    exit(EX_SOFTWARE);
  }
  if (smfi_setdbg(pmilter_config->debug) == MI_FAILURE) {
    pmilter_log_error(PMILTER_LOG_ERR, pmilter_config, "smfi_setdbg failed");
    exit(EX_SOFTWARE);
  }
  if (smfi_register(smfilter) == MI_FAILURE) {
    pmilter_log_error(PMILTER_LOG_ERR, pmilter_config, "smfi_register failed\n");
    exit(EX_UNAVAILABLE);
  }

  /* config handler init */
  pmilter_config_handler_init(pmilter_config);

  /* postconfig handler phase */
  pmilter_postconfig_handler_run(pmilter_config);

  /* start pmilter */
  pmilter_log_error(PMILTER_LOG_NOTICE, pmilter_config, "%s starting", PMILTER_DESCRIPTION);

  smfi_status = smfi_main(pmilter_config);

  /* cleanup */
  pmilter_postconfig_handler_free(pmilter_config);
  toml_free(toml_root);
  pmilter_config_free(pmilter_config);

  return smfi_status;
}
