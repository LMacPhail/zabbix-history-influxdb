#ifndef __ZABBIX_LOAD_CONFIG_H
#define __ZABBIX_LOAD_CONFIG_H


#include "sysinc.h"
#include "module.h"
#include "common.h"
#include "log.h"
#include "cfg.h"

#define MODULE_NAME "history_influxdb.so"
#define MODULE_LOCAL_CONFIG_FILE_NAME "history_influxdb_local.conf"
#define MODULE_CONFIG_FILE_NAME "history_influxdb.conf"

#define CONFIG_DISABLE 0
#define CONFIG_ENABLE  1

extern char *CONFIG_LOAD_MODULE_PATH;


extern void zbx_module_load_config(void);
extern void zbx_module_set_defaults(void);

extern char *CONFIG_INFLUXDB_ADDRESS;
extern char *CONFIG_INFLUXDB_NAME;
extern char *CONFIG_INFLUXDB_PORT;
extern char *CONFIG_INFLUXDB_PROTOCOL;
extern char *CONFIG_INFLUXDB_USER;
extern char *CONFIG_INFLUXDB_PWD;
extern int *CONFIG_INFLUXDB_SSL_INSECURE;
extern int *CONFIG_FORCE_MODULE_DEBUG;


#endif /* __ZABBIX_LOAD_CONFIG_H */
