#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libmilter/mfapi.h"
#include "libmilter/mfdef.h"

#include "toml.h"
#include "toml_private.h"

#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/string.h"

#include "pmilter.h"
#include "pmilter_log.h"

pmilter_config *pmilter_config_init()
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

void command_rec_free(command_rec *cmd)
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

void pmilter_mrb_delete_conf(pmilter_mrb_shared_state *pmilter)
{

  command_rec_free(pmilter->cmd);

  if (pmilter->mruby_connect_handler != PMILTER_CONF_UNSET) {
    free(pmilter->mruby_connect_handler);
  }

  mrb_close(pmilter->mrb);

  free(pmilter);
}

pmilter_mrb_shared_state *pmilter_mrb_create_conf(pmilter_config *config)
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

struct toml_node *mrb_pmilter_config_init(const char *path)
{
  struct toml_node *root;
  char *buf = "[foo]\nbar = 'fuga'\n";
  size_t len = sizeof(buf);

  /* TODO: file open */
  toml_init(&root);
  toml_parse(root, buf, len);

  return root;
}

void mrb_pmilter_config_free(struct toml_node *root)
{
  toml_free(root);
}

int pmilter_config_get_bool(pmilter_config *config, struct toml_node *root, char *key)
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

int pmilter_config_get_log_level(struct toml_node *root)
{
  int i;
  int log_level = PMILTER_LOG_WARN;
  struct toml_node *node = toml_get(root, "server.log_level");

  if (node != NULL) {
    log_level = pmilter_get_log_level(node->value.string);
  }

  return log_level;
}
