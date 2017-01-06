/*
** pmilter - A Programmable Mail Filter
**
** See Copyright Notice in LICENSE
*/

#ifndef _PMILTER_CONFIG_H_
#define _PMILTER_CONFIG_H_

#include "toml.h"

#include "pmilter.h"

#define pmilter_mruby_code_free(code)                                                                                  \
  if (code != PMILTER_CONF_UNSET) {                                                                                    \
    free(code);                                                                                                        \
    code = PMILTER_CONF_UNSET;                                                                                         \
  }

pmilter_config *pmilter_config_init();
void pmilter_config_free(pmilter_config *config);
void pmilter_config_parse(pmilter_config *config, struct toml_node *root);
void pmilter_usage(char *prog);

int pmilter_config_get_bool(pmilter_config *config, struct toml_node *root, char *key);
int pmilter_config_get_log_level(struct toml_node *root);

pmilter_state *pmilter_create_conf(pmilter_config *config);
void pmilter_delete_conf(pmilter_state *pmilter);

struct toml_node *pmilter_toml_load(char *file, char **argv);

#endif // _PMILTER_CONFIG_H_
