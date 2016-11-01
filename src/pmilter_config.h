#ifndef _PMILTER_CONFIG_H_
#define _PMILTER_CONFIG_H_

#include "toml.h"

#include "pmilter.h"

pmilter_config *pmilter_config_init();

void command_rec_free(command_rec *cmd);

void pmilter_mrb_delete_conf(pmilter_mrb_shared_state *pmilter);

pmilter_mrb_shared_state *pmilter_mrb_create_conf(pmilter_config *config);

struct toml_node *mrb_pmilter_config_init(const char *path);

void mrb_pmilter_config_free(struct toml_node *root);

int pmilter_config_get_bool(pmilter_config *config, struct toml_node *root, char *key);

int pmilter_config_get_log_level(struct toml_node *root);

void pmilter_config_parse(pmilter_config *config, struct toml_node *root);

#endif // _PMILTER_CONFIG_H_
