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

#define PMILTER_CODE_MRBC_CONTEXT_FREE(mrb, code)                                                                      \
  if (code != PMILTER_CONF_UNSET && mrb && (code)->ctx) {                                                              \
    mrbc_context_free(mrb, (code)->ctx);                                                                               \
    (code)->ctx = NULL;                                                                                                \
  }

#define COMMAND_REC_CTX ((command_rec *)smfi_getpriv(ctx))
#define DEBUG_SMFI_SYMVAL(macro_name)                                                                                  \
  fprintf(stderr, "    " #macro_name ": %s\n", smfi_getsymval(ctx, "{" #macro_name "}"))
#define DEBUG_SMFI_CHAR(val) fprintf(stderr, "    " #val ": %s\n", val)
#define DEBUG_SMFI_INT(val) fprintf(stderr, "    " #val ": %d\n", val)
#define DEBUG_SMFI_HOOK(val) fprintf(stderr, #val "\n")

typedef struct connection_rec_t {

  char *ipaddr;
  const _SOCK_ADDR *hostaddr;

} connection_rec;

typedef struct command_rec_t {

  connection_rec *conn;
  char *connect_daemon;
  char *envelope_from;
  char *envelope_to;
  int receive_time;

} command_rec;

typedef struct pmilter_config_t {

  const char *mruby_connect_handler_path;

} pmilter_config;

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

typedef struct pmilter_mrb_shared_state_t {

  mrb_state *mrb;
  command_rec *cmd;
  pmilter_config *config;
  int status;

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

} pmilter_mrb_shared_state;

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
    fprintf(stderr, "%s:%d: compile info: code->code.file=(%s)", __func__, __LINE__, code->code.file);
  } else {
    fprintf(stderr, "%s:%d: compile info: "
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

  pmilter->mrb = mrb_open();
  if (pmilter->mrb == NULL) {
    return NULL;
  }

  pmilter_mrb_class_init(pmilter->mrb);

  return pmilter;
}

/* pmilter mruby handlers */
static int pmilter_connect_handler(pmilter_mrb_shared_state *pmilter)
{
  mrb_state *mrb = pmilter->mrb;
  mrb_int ai = mrb_gc_arena_save(mrb);

  /* defualt status */
  pmilter->status = SMFIS_CONTINUE;

  /* pmilter object pass to mruby world */
  mrb->ud = pmilter;
  mrb_run(mrb, pmilter->mruby_connect_handler->proc, mrb_top_self(mrb));

  if (mrb->exc) {
    pmilter_mrb_raise_error(mrb, mrb_obj_value(mrb->exc));
    pmilter_mrb_state_clean(mrb);
    mrb_gc_arena_restore(mrb, ai);
    return PMILTER_ERROR;
  }

  pmilter_mrb_state_clean(mrb);
  mrb_gc_arena_restore(mrb, ai);

  /* default SMFIS_CONTINUE*/
  return pmilter->status;
}

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

/* connection info filter */
sfsistat mrb_xxfi_connect(ctx, hostname, hostaddr) SMFICTX *ctx;
char *hostname;
_SOCK_ADDR *hostaddr;
{
  pmilter_mrb_shared_state *pmilter;
  command_rec *cmd;
  connection_rec *conn;
  pmilter_config *config = smfi_getpriv(ctx);
  int ret;

  pmilter = pmilter_mrb_create_conf(config);
  pmilter->mruby_connect_handler = pmilter_mrb_code_from_file(pmilter->config->mruby_connect_handler_path);
  ret = pmilter_mrb_shared_state_compile(pmilter, pmilter->mruby_connect_handler);

  if (ret == PMILTER_ERROR) {
    return SMFIS_TEMPFAIL;
  }

  /* need free */
  cmd = (command_rec *)calloc(1, sizeof(command_rec));
  if (cmd == NULL) {
    return SMFIS_TEMPFAIL;
  }

  /* need free */
  conn = (connection_rec *)calloc(1, sizeof(connection_rec));
  if (conn == NULL) {
    return SMFIS_TEMPFAIL;
  }

  DEBUG_SMFI_HOOK(mrb_xxfi_connect);

  DEBUG_SMFI_CHAR(hostname);

  cmd->conn = conn;
  cmd->conn->hostaddr = hostaddr;
  cmd->conn->ipaddr = ipaddrdup(hostname, hostaddr);
  if (cmd->conn->ipaddr == NULL) {
    return SMFIS_TEMPFAIL;
  }
  cmd->connect_daemon = smfi_getsymval(ctx, "{daemon_name}");

  pmilter->cmd = cmd;

  DEBUG_SMFI_CHAR(cmd->conn->ipaddr);
  DEBUG_SMFI_CHAR(cmd->connect_daemon);

  DEBUG_SMFI_SYMVAL(if_name);
  DEBUG_SMFI_SYMVAL(if_addr);
  DEBUG_SMFI_SYMVAL(j);
  DEBUG_SMFI_SYMVAL(_);

  pmilter_connect_handler(pmilter);

  smfi_setpriv(ctx, pmilter);

  return SMFIS_CONTINUE;
}

/* SMTP HELO command filter */
sfsistat mrb_xxfi_helo(ctx, helohost) SMFICTX *ctx;
char *helohost;
{
  DEBUG_SMFI_HOOK(mrb_xxfi_helo);

  DEBUG_SMFI_CHAR(helohost);

  DEBUG_SMFI_SYMVAL(tls_version);
  DEBUG_SMFI_SYMVAL(cipher);
  DEBUG_SMFI_SYMVAL(cipher_bits);
  DEBUG_SMFI_SYMVAL(cert_subject);
  DEBUG_SMFI_SYMVAL(cert_issuer);

  return SMFIS_CONTINUE;
}

/* envelope sender filter */
sfsistat mrb_xxfi_envfrom(ctx, argv) SMFICTX *ctx;
char **argv;
{
  command_rec *cmd = COMMAND_REC_CTX;

  DEBUG_SMFI_HOOK(mrb_xxfi_envfrom);

  cmd->envelope_from = strdup(argv[0]);
  DEBUG_SMFI_CHAR(cmd->envelope_from);

  DEBUG_SMFI_SYMVAL(i);
  DEBUG_SMFI_SYMVAL(auth_type);
  DEBUG_SMFI_SYMVAL(auth_authen);
  DEBUG_SMFI_SYMVAL(auth_ssf);
  DEBUG_SMFI_SYMVAL(auth_author);
  DEBUG_SMFI_SYMVAL(mail_mailer);
  DEBUG_SMFI_SYMVAL(mail_host);
  DEBUG_SMFI_SYMVAL(mail_addr);

  return SMFIS_CONTINUE;
}

/* envelope recipient filter */
sfsistat mrb_xxfi_envrcpt(ctx, argv) SMFICTX *ctx;
char **argv;
{
  command_rec *cmd = COMMAND_REC_CTX;

  DEBUG_SMFI_HOOK(mrb_xxfi_envrcpt);

  cmd->envelope_to = smfi_getsymval(ctx, "{rcpt_addr}");
  DEBUG_SMFI_CHAR(cmd->envelope_to);

  DEBUG_SMFI_CHAR(argv[0]);

  DEBUG_SMFI_SYMVAL(rcpt_mailer);
  DEBUG_SMFI_SYMVAL(rcpt_host);
  DEBUG_SMFI_SYMVAL(rcpt_addr);

  return SMFIS_CONTINUE;
}

/* header filter */
sfsistat mrb_xxfi_header(ctx, headerf, headerv) SMFICTX *ctx;
char *headerf;
unsigned char *headerv;
{
  DEBUG_SMFI_HOOK(mrb_xxfi_header);
  DEBUG_SMFI_CHAR(headerf);
  DEBUG_SMFI_CHAR(headerv);

  return SMFIS_CONTINUE;
}

/* end of header */
sfsistat mrb_xxfi_eoh(ctx) SMFICTX *ctx;
{
  DEBUG_SMFI_HOOK(mrb_xxfi_eoh);
  return SMFIS_CONTINUE;
}

/* body block filter */
sfsistat mrb_xxfi_body(ctx, bodyp, bodylen) SMFICTX *ctx;
unsigned char *bodyp;
size_t bodylen;
{
  char *body = malloc(bodylen);

  DEBUG_SMFI_HOOK(mrb_xxfi_body);

  memcpy(body, bodyp, bodylen);
  body[bodylen] = '\0';

  DEBUG_SMFI_CHAR(body);

  free(body);

  return SMFIS_CONTINUE;
}

/* end of message */
sfsistat mrb_xxfi_eom(ctx) SMFICTX *ctx;
{
  command_rec *cmd = COMMAND_REC_CTX;
  time_t accept_time;
  bool ok = TRUE;

  DEBUG_SMFI_HOOK(mrb_xxfi_eom);

  time(&accept_time);
  cmd->receive_time = accept_time;

  DEBUG_SMFI_INT(cmd->receive_time);

  DEBUG_SMFI_SYMVAL(msg_id);

  return SMFIS_CONTINUE;
}

/* message aborted */
sfsistat mrb_xxfi_abort(ctx) SMFICTX *ctx;
{
  DEBUG_SMFI_HOOK(mrb_xxfi_abort);
  return mrb_xxfi_cleanup(ctx, FALSE);
}

/* session cleanup */
sfsistat mrb_xxfi_cleanup(ctx, ok) SMFICTX *ctx;
bool ok;
{
  DEBUG_SMFI_HOOK(mrb_xxfi_cleanup);
  return SMFIS_CONTINUE;
}

/* connection cleanup */
sfsistat mrb_xxfi_close(ctx) SMFICTX *ctx;
{
  pmilter_mrb_shared_state *pmilter = smfi_getpriv(ctx);

  DEBUG_SMFI_HOOK(mrb_xxfi_close);

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
  command_rec *cmd;

  DEBUG_SMFI_HOOK(mrb_xxfi_unknown);
  DEBUG_SMFI_CHAR(scmd);
  return SMFIS_CONTINUE;
}

/* DATA command */
sfsistat mrb_xxfi_data(ctx) SMFICTX *ctx;
{
  command_rec *cmd;
  DEBUG_SMFI_HOOK(mrb_xxfi_data);
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
  command_rec *cmd;
  DEBUG_SMFI_HOOK(mrb_xxfi_negotiate);
  return SMFIS_ALL_OPTS;
}

struct smfiDesc smfilter = {
    "pmilter",                     /* filter name */
    SMFI_VERSION,                  /* version code */
    SMFIF_ADDHDRS | SMFIF_ADDRCPT, /* flags */
    mrb_xxfi_connect,              /* connection info filter */
    NULL,                          /* SMTP HELO command filter */
    NULL,                          /* envelope sender filter */
    NULL,                          /* envelope recipient filter */
    NULL,                          /* header filter */
    NULL,                          /* end of header */
    NULL,                          /* body block filter */
    NULL,                          /* end of message */
    NULL,                          /* message aborted */
    mrb_xxfi_close,                /* connection cleanup */
    NULL,                          /* unknown SMTP commands */
    NULL,                          /* DATA command */
    NULL                           /* Once, at the start of each SMTP connection */
                                   // mrb_xxfi_helo,                 /* SMTP HELO command filter */
    // mrb_xxfi_envfrom,              /* envelope sender filter */
    // mrb_xxfi_envrcpt,              /* envelope recipient filter */
    // mrb_xxfi_header,               /* header filter */
    // mrb_xxfi_eoh,                  /* end of header */
    // mrb_xxfi_body,                 /* body block filter */
    // mrb_xxfi_eom,                  /* end of message */
    // mrb_xxfi_abort,                /* message aborted */
    // mrb_xxfi_close,                /* connection cleanup */
    // mrb_xxfi_unknown,              /* unknown SMTP commands */
    // mrb_xxfi_data,                 /* DATA command */
    // mrb_xxfi_negotiate             /* Once, at the start of each SMTP connection */
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

  fprintf(stdout, "pmilter configuration\n=====\n");
  toml_dump(toml_root, stdout);
  fprintf(stdout, "=====\n");

  // toml_tojson(toml_root, stdout);

  /* pmilter config setup */
  pmilter_config = pmilter_config_init();
  node = toml_get(toml_root, "handler.mruby_connect_handler");
  pmilter_config->mruby_connect_handler_path = node->value.string;

  fprintf(stdout, "pmilter run\n=====\n");

  return smfi_main(pmilter_config);

bail:
  toml_free(toml_root);

  exit(exit_code);
}
