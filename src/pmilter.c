#include <arpa/inet.h>
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
#include "pmilter_log.h"

#define PMILTER_CODE_MRBC_CONTEXT_FREE(mrb, code)                                                                      \
  if (code != PMILTER_CONF_UNSET && mrb && (code)->ctx) {                                                              \
    mrbc_context_free(mrb, (code)->ctx);                                                                               \
    (code)->ctx = NULL;                                                                                                \
  }

#define PMILTER_GET_HANDLER_CONFIG_VALUE(root, node, config, phase)                                                    \
  node = toml_get(root, "handler.mruby_" #phase "_handler");                                                           \
  if (node != NULL) {                                                                                                  \
    config->mruby_##phase##_handler_path = node->value.string;                                                         \
  } else {                                                                                                             \
    config->mruby_##phase##_handler_path = NULL;                                                                       \
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

static void pmilter_mruby_cleanup(pmilter_mrb_shared_state *pmilter)
{
  PMILTER_CODE_MRBC_CONTEXT_FREE(pmilter->mrb, pmilter->mruby_connect_handler);

  mrb_close(pmilter->mrb);
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

static int pmilter_mrb_shared_state_compile(pmilter_mrb_shared_state *pmilter, pmilter_mrb_code *code)
{
  FILE *mrb_file;
  struct mrb_parser_state *p;
  mrb_state *mrb = pmilter->mrb;

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
    pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "%s:%d: compile info: code->code.file=(%s)", __func__,
                      __LINE__, code->code.file);
  } else {
    pmilter_log_error(PMILTER_LOG_DEBUG, pmilter->config, "%s:%d: compile info: "
                                                          "code->code.string=(%s)",
                      __func__, __LINE__, code->code.string);
  }

  return PMILTER_OK;
}

static pmilter_config *pmilter_config_init()
{
  pmilter_config *config;

  /* need free */
  config = malloc(sizeof(pmilter_config));
  if (config == NULL) {
    return NULL;
  }

  config->log_level = PMILTER_LOG_WARN;
  config->enable_mruby_handler = 0;

  return config;
}

static void command_rec_free(command_rec *cmd)
{
  if (cmd->envelope_from != NULL) {
    free(cmd->envelope_from);
  }

  /* connecntion_rec free */
  if (cmd->conn->ipaddr != NULL) {
    free(cmd->conn->ipaddr);
  }
  free(cmd->conn);
  free(cmd->header);

  free(cmd);
}

static void pmilter_mrb_delete_conf(pmilter_mrb_shared_state *pmilter)
{

  command_rec_free(pmilter->cmd);

  if (pmilter->mruby_connect_handler != PMILTER_CONF_UNSET) {
    free(pmilter->mruby_connect_handler);
  }

  mrb_close(pmilter->mrb);

  free(pmilter);
}

static pmilter_mrb_shared_state *pmilter_mrb_create_conf(pmilter_config *config)
{
  pmilter_mrb_shared_state *pmilter;

  /* need free */
  pmilter = malloc(sizeof(pmilter_mrb_shared_state));
  if (pmilter == NULL) {
    return NULL;
  }

  pmilter->config = config;

  pmilter->mruby_connect_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_helo_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_envfrom_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_envrcpt_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_header_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_eoh_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_body_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_eom_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_abort_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_close_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_unknown_handler = PMILTER_CONF_UNSET;
  pmilter->mruby_data_handler = PMILTER_CONF_UNSET;

  if (config->enable_mruby_handler) {
    pmilter->mrb = mrb_open();
    if (pmilter->mrb == NULL) {
      return NULL;
    }

    pmilter_mrb_class_init(pmilter->mrb);
  } else {
    pmilter->mrb = NULL;
  }

  return pmilter;
}

/* pmilter mruby handlers */
#define PMILTER_ADD_MRUBY_HADNLER(hook_phase)                                                                          \
  static int pmilter_##hook_phase##_handler(pmilter_mrb_shared_state *pmilter)                                         \
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
      pmilter_mrb_state_clean(mrb);                                                                                    \
      mrb_gc_arena_restore(mrb, ai);                                                                                   \
      return PMILTER_ERROR;                                                                                            \
    }                                                                                                                  \
                                                                                                                       \
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

/* other utils */
static char *ipaddrdup(const char *hostname, const _SOCK_ADDR *hostaddr)
{
  char addr_buf[INET6_ADDRSTRLEN];

  switch (hostaddr->sa_family) {
  case AF_INET: {
    struct sockaddr_in *sin = (struct sockaddr_in *)hostaddr;
    if (NULL == inet_ntop(AF_INET, &(sin->sin_addr), addr_buf, INET_ADDRSTRLEN)) {
      fprintf(stderr, "inet_ntop AF_INET4 failed\n");
      return NULL;
    }
    break;
  }
  case AF_INET6: {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)hostaddr;
    if (NULL == inet_ntop(AF_INET6, &(sin6->sin6_addr), addr_buf, INET6_ADDRSTRLEN)) {
      fprintf(stderr, "inet_ntop AF_INET6 failed\n");
      return NULL;
    }
    break;
  }
  default:
    fprintf(stderr, "Unknown protocol: sa_familyr=%d\n", hostaddr->sa_family);
    return NULL;
  }

  return strdup(addr_buf);
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
  cmd->connect_daemon = NULL;
  cmd->envelope_from = NULL;
  cmd->envelope_to = NULL;
  cmd->receive_time = 0;
  cmd->header = header;
  cmd->header->key = NULL;
  cmd->header->value = NULL;

  return cmd;
}

/* connection info filter */
sfsistat mrb_xxfi_connect(ctx, hostname, hostaddr) SMFICTX *ctx;
char *hostname;
_SOCK_ADDR *hostaddr;
{
  pmilter_mrb_shared_state *pmilter;
  pmilter_config *config = smfi_getpriv(ctx);
  int ret;

  pmilter = pmilter_mrb_create_conf(config);
  pmilter->ctx = ctx;
  pmilter->cmd = pmilter_command_init();
  if (pmilter->cmd == NULL) {
    return SMFIS_TEMPFAIL;
  }

  pmilter->cmd->conn->hostaddr = hostaddr;
  pmilter->cmd->conn->ipaddr = ipaddrdup(hostname, hostaddr);

  if (pmilter->cmd->conn->ipaddr == NULL) {
    return SMFIS_TEMPFAIL;
  }

  pmilter->cmd->connect_daemon = smfi_getsymval(ctx, "{daemon_name}");
  pmilter->mruby_connect_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_connect_handler_path);

  if (pmilter->mruby_connect_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_connect_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }

    pmilter_connect_handler(pmilter);
  }

  smfi_setpriv(ctx, pmilter);

  return SMFIS_CONTINUE;
}

/* SMTP HELO command filter */
sfsistat mrb_xxfi_helo(ctx, helohost) SMFICTX *ctx;
char *helohost;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_helo);

  DEBUG_SMFI_CHAR(helohost);

  DEBUG_SMFI_SYMVAL(tls_version);
  DEBUG_SMFI_SYMVAL(cipher);
  DEBUG_SMFI_SYMVAL(cipher_bits);
  DEBUG_SMFI_SYMVAL(cert_subject);
  DEBUG_SMFI_SYMVAL(cert_issuer);

  pmilter->mruby_helo_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_helo_handler_path);

  if (pmilter->mruby_helo_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_helo_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_helo_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* envelope sender filter */
sfsistat mrb_xxfi_envfrom(ctx, argv) SMFICTX *ctx;
char **argv;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  /* need free */
  pmilter->cmd->envelope_from = strdup(argv[0]);
  pmilter->mruby_envfrom_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_envfrom_handler_path);

  if (pmilter->mruby_envfrom_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_envfrom_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_envfrom_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* envelope recipient filter */
sfsistat mrb_xxfi_envrcpt(ctx, argv) SMFICTX *ctx;
char **argv;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->cmd->envelope_to = smfi_getsymval(ctx, "{rcpt_addr}");
  pmilter->mruby_envrcpt_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_envrcpt_handler_path);

  if (pmilter->mruby_envrcpt_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_envrcpt_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_envrcpt_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* header filter */
sfsistat mrb_xxfi_header(ctx, headerf, headerv) SMFICTX *ctx;
char *headerf;
unsigned char *headerv;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  pmilter->cmd->header->key = headerf;
  pmilter->cmd->header->value = headerv;
  pmilter->mruby_header_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_header_handler_path);

  if (pmilter->mruby_header_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_header_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_header_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* end of header */
sfsistat mrb_xxfi_eoh(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_eoh);

  pmilter->mruby_eoh_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_eoh_handler_path);

  if (pmilter->mruby_eoh_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_eoh_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_eoh_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* body block filter */
sfsistat mrb_xxfi_body(ctx, bodyp, bodylen) SMFICTX *ctx;
unsigned char *bodyp;
size_t bodylen;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;
  char *body = malloc(bodylen);

  DEBUG_SMFI_HOOK(mrb_xxfi_body);

  memcpy(body, bodyp, bodylen);
  body[bodylen] = '\0';

  DEBUG_SMFI_CHAR(body);

  free(body);

  pmilter->mruby_body_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_body_handler_path);

  if (pmilter->mruby_body_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_body_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_body_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* end of message */
sfsistat mrb_xxfi_eom(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;
  time_t accept_time;
  bool ok = TRUE;

  DEBUG_SMFI_HOOK(mrb_xxfi_eom);

  time(&accept_time);
  pmilter->cmd->receive_time = accept_time;

  DEBUG_SMFI_INT(pmilter->cmd->receive_time);

  DEBUG_SMFI_SYMVAL(msg_id);

  pmilter->mruby_eom_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_eom_handler_path);

  if (pmilter->mruby_eom_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_eom_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_eom_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* message aborted */
sfsistat mrb_xxfi_abort(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_abort);

  pmilter->mruby_abort_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_abort_handler_path);

  if (pmilter->mruby_abort_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_abort_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_abort_handler(pmilter);
  }

  return mrb_xxfi_cleanup(ctx, FALSE);
}

/* session cleanup */
sfsistat mrb_xxfi_cleanup(ctx, ok) SMFICTX *ctx;
bool ok;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);

  DEBUG_SMFI_HOOK(mrb_xxfi_cleanup);

  return SMFIS_CONTINUE;
}

/* connection cleanup */
sfsistat mrb_xxfi_close(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_close);

  pmilter->mruby_close_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_close_handler_path);

  if (pmilter->mruby_close_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_close_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_close_handler(pmilter);
  }

  if (pmilter == NULL) {
    smfi_setpriv(ctx, NULL);
    return SMFIS_CONTINUE;
  }

  pmilter_mrb_delete_conf(pmilter);
  smfi_setpriv(ctx, NULL);

  fprintf(stderr, "------------\n");

  return SMFIS_CONTINUE;
}

/* Once, at the start of each SMTP connection */
sfsistat mrb_xxfi_unknown(ctx, scmd) SMFICTX *ctx;
char *scmd;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_unknown);
  DEBUG_SMFI_CHAR(scmd);

  pmilter->mruby_unknown_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_unknown_handler_path);

  if (pmilter->mruby_unknown_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_unknown_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_unknown_handler(pmilter);
  }

  return SMFIS_CONTINUE;
}

/* DATA command */
sfsistat mrb_xxfi_data(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);
  int ret;

  DEBUG_SMFI_HOOK(mrb_xxfi_data);

  pmilter->mruby_data_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_data_handler_path);

  if (pmilter->mruby_data_handler != NULL && pmilter->mrb != NULL) {
    ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_data_handler);
    if (ret == PMILTER_ERROR) {
      return SMFIS_TEMPFAIL;
    }
    pmilter_data_handler(pmilter);
  }

  return SMFIS_CONTINUE;
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
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);

  DEBUG_SMFI_HOOK(mrb_xxfi_negotiate);

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

static void usage(prog) char *prog;
{
  fprintf(stderr, "Usage: %s -p socket-addr -c config.toml [-t timeout]\n", prog);
}

static struct toml_node *mrb_pmilter_config_init(const char *path)
{
  struct toml_node *root;
  char *buf = "[foo]\nbar = 'fuga'\n";
  size_t len = sizeof(buf);

  /* TODO: file open */
  toml_init(&root);
  toml_parse(root, buf, len);

  return root;
}

static void mrb_pmilter_config_free(struct toml_node *root)
{
  toml_free(root);
}

static int pmilter_config_get_bool(pmilter_config *config, struct toml_node *root, char *key)
{
  struct toml_node *node = toml_get(root, key);

  if (!toml_type(node) == TOML_BOOLEAN) {
    pmilter_log_error(PMILTER_LOG_EMERG, config, "%s must be boolen type in config", key);
    exit(1);
  }

  if (node->value.integer) {
    return 1;
  }

  return 0;
}

static int pmilter_config_get_log_level(struct toml_node *root)
{
  int i;
  int log_level = PMILTER_LOG_WARN;
  struct toml_node *node = toml_get(root, "server.log_level");

  if (node != NULL) {
    for (i = 0; i < sizeof(err_levels) / sizeof(const char *); i++) {
      if (strcmp(node->value.string, err_levels[i]) == 0) {
        log_level = i;
      }
    }
  }

  return log_level;
}

int main(argc, argv) int argc;
char **argv;
{
  pmilter_config *pmilter_config;
  bool setconn = FALSE;
  int c;
  const char *args = "c:p:t:h";
  extern char *optarg;
  struct toml_node *toml_root, *node;
  char *file = NULL;
  void *toml_content = NULL;
  int fd, ret, toml_content_size = 0;
  struct stat st;
  int exit_code = EXIT_SUCCESS;
  int i;

  /* Process command line options */
  while ((c = getopt(argc, argv, args)) != -1) {
    switch (c) {
    case 'c':
      file = optarg;
      break;

    case 'p':
      if (optarg == NULL || *optarg == '\0') {
        (void)fprintf(stderr, "Illegal conn: %s\n", optarg);
        exit(EX_USAGE);
      }
      if (smfi_setconn(optarg) == MI_FAILURE) {
        (void)fprintf(stderr, "smfi_setconn failed: port or socket already exists?\n");
        exit(EX_SOFTWARE);
      }
      /*
       * **  If we're using a local socket, make sure it
       * **  doesn't already exist.  Don't ever run this
       * **  code as root!!
       */
      if (strncasecmp(optarg, "unix:", 5) == 0)
        unlink(optarg + 5);
      else if (strncasecmp(optarg, "local:", 6) == 0)
        unlink(optarg + 6);
      setconn = TRUE;
      break;
    case 't':
      if (optarg == NULL || *optarg == '\0') {
        (void)fprintf(stderr, "Illegal timeout: %s\n", optarg);
        exit(EX_USAGE);
      }
      if (smfi_settimeout(atoi(optarg)) == MI_FAILURE) {
        (void)fprintf(stderr, "smfi_settimeout failed\n");
        exit(EX_SOFTWARE);
      }
      break;
    case 'h':
    default:
      usage(argv[0]);
      exit(EX_USAGE);
    }
  }
  if (!setconn) {
    fprintf(stderr, "%s: Missing required -p argument\n", argv[0]);
    usage(argv[0]);
    exit(EX_USAGE);
  }
  if (smfi_register(smfilter) == MI_FAILURE) {
    fprintf(stderr, "smfi_register failed\n");
    exit(EX_UNAVAILABLE);
  }

  if (file) {
    fd = open(file, O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "open: %s\n", strerror(errno));
      exit(1);
    }

    ret = fstat(fd, &st);
    if (ret == -1) {
      fprintf(stderr, "stat: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    toml_content = mmap(NULL, st.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if (!toml_content) {
      fprintf(stderr, "mmap: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    toml_content_size = st.st_size;
  } else {
    fprintf(stderr, "%s: Missing required -c argument\n", argv[0]);
    usage(argv[0]);
    exit(EX_USAGE);
  }

  ret = toml_init(&toml_root);
  if (ret == -1) {
    fprintf(stderr, "toml_init: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  ret = toml_parse(toml_root, toml_content, toml_content_size);
  if (ret) {
    exit_code = EXIT_FAILURE;
    goto bail;
  }

  if (file) {
    ret = munmap(toml_content, toml_content_size);
    if (ret) {
      fprintf(stderr, "munmap: %s\n", strerror(errno));
      exit_code = EXIT_FAILURE;
      goto bail;
    }

    close(fd);
  }

  /* pmilter config setup */
  pmilter_config = pmilter_config_init();
  pmilter_config->log_level = pmilter_config_get_log_level(toml_root);
  pmilter_config->enable_mruby_handler = pmilter_config_get_bool(pmilter_config, toml_root, "server.mruby_handler");

  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, connect);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, helo);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, envfrom);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, envrcpt);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, header);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, eoh);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, body);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, eom);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, abort);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, close);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, unknown);
  PMILTER_GET_HANDLER_CONFIG_VALUE(toml_root, node, pmilter_config, data);

  pmilter_log_error(PMILTER_LOG_INFO, pmilter_config, "pmilter configuration\n=====");
  toml_dump(toml_root, stdout);
  pmilter_log_error(PMILTER_LOG_INFO, pmilter_config, "=====");

  pmilter_log_error(PMILTER_LOG_INFO, pmilter_config, "pmilter %s", "starting");

  return smfi_main(pmilter_config);

bail:
  toml_free(toml_root);

  exit(exit_code);
}
