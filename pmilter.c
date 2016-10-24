#include "libmilter/mfapi.h"
#include "libmilter/mfdef.h"
#include "pthread.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#ifndef bool
#define bool int
#define TRUE 1
#define FALSE 0
#endif /* ! bool */

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

#define COMMAND_REC_CTX ((command_rec *)smfi_getpriv(ctx))
#define DEBUG_SMFI_SYMVAL(macro_name) fprintf(stderr, "    " #macro_name ": %s\n", smfi_getsymval(ctx, "{" #macro_name "}"))
#define DEBUG_SMFI_CHAR(val) fprintf(stderr, "    " #val ": %s\n", val)
#define DEBUG_SMFI_INT(val) fprintf(stderr, "    " #val ": %d\n", val)
#define DEBUG_SMFI_HOOK(val) fprintf(stderr, #val "\n")

extern sfsistat mrb_xxfi_cleanup(SMFICTX *, bool);

static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

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
  command_rec *cmd;
  connection_rec *conn;

  cmd = (command_rec *)calloc(1, sizeof(command_rec));
  if (cmd == NULL) {
    return SMFIS_TEMPFAIL;
  }
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

  DEBUG_SMFI_CHAR(cmd->conn->ipaddr);
  DEBUG_SMFI_CHAR(cmd->connect_daemon);

  DEBUG_SMFI_SYMVAL(if_name);
  DEBUG_SMFI_SYMVAL(if_addr);
  DEBUG_SMFI_SYMVAL(j);
  DEBUG_SMFI_SYMVAL(_);

  smfi_setpriv(ctx, cmd);

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
  command_rec *cmd = COMMAND_REC_CTX;

  DEBUG_SMFI_HOOK(mrb_xxfi_close);

  if (cmd == NULL) {
    return SMFIS_CONTINUE;
  }

  if (cmd->conn->ipaddr != NULL) {
    free(cmd->conn->ipaddr);
  }

  if (cmd->envelope_from != NULL) {
    free(cmd->envelope_from);
  }

  if (cmd->envelope_to != NULL) {
    cmd->envelope_to = NULL;
  }

  if (cmd->receive_time != 0) {
    cmd->receive_time = 0;
  }

  free(cmd->conn);
  free(cmd);
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
  fprintf(stderr, "Usage: %s -p socket-addr [-t timeout]\n", prog);
}

int main(argc, argv) int argc;
char **argv;
{
  bool setconn = FALSE;
  int c;
  const char *args = "p:t:h";
  extern char *optarg;
  /* Process command line options */
  while ((c = getopt(argc, argv, args)) != -1) {
    switch (c) {
    case 'p':
      if (optarg == NULL || *optarg == '\0') {
        (void)fprintf(stderr, "Illegal conn: %s\n", optarg);
        exit(EX_USAGE);
      }
      if (smfi_setconn(optarg) == MI_FAILURE) {
        (void)fprintf(stderr, "smfi_setconn failed\n");
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

  return smfi_main();
}
