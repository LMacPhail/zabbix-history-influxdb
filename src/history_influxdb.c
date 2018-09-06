/*
**  zabbix-history-influxdb loadable module for Zabbix
    Copyright (C) 2018 Lucy MacPhail

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**/

/******************************************************************************
 *
 *    Module Structure:
 *    	1. Compulsory zabbix module functions 	(60 - 145)
 *      2. write_to_influxdb               	(180)
 *      3. host_item_name_query  		(210)
 *      4. history callback functions           (300)
 *      5. zbx_module_history_write_cbs         (465)
 *
 ******************************************************************************/

#include "common.h"
#include "sysinc.h"
#include "module.h"
#include "log.h"
#include "cfg.h"
#include "db.h"

#include "load_config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>
#include <time.h>

#define METRIC_LEN 1000
#define CURL_LEN 256
#define ITEM_VALUE_LEN 255
#define HOST_NAME_LEN 128

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;
int MODULE_LOG_LEVEL = 0;

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION - version of module.h module is       *
 *               compiled with, in order to load module successfully Zabbix   *
 *               MUST be compiled with the same version of this header file   *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version(void)
{
	return ZBX_MODULE_API_VERSION;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/

static ZBX_METRIC keys[] =
/*	KEY				FLAG		FUNCTION		TEST PARAMETERS */
{
	{NULL}
};

ZBX_METRIC	*zbx_module_item_list(void)
{
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/

char influxdb_write_url[CURL_LEN];

int	zbx_module_init(void)
{
	char		*error = NULL;

	/* Sets up cURL from config file */
	zbx_module_load_config();

	/* This will open the log for debugging */
	MODULE_LOG_LEVEL = (CONFIG_FORCE_MODULE_DEBUG ? LOG_LEVEL_INFORMATION : LOG_LEVEL_DEBUG);

	if(CONFIG_INFLUXDB_USER == NULL){
		zbx_snprintf(influxdb_write_url, CURL_LEN, "%s://%s:%s/write?db=%s", CONFIG_INFLUXDB_PROTOCOL, CONFIG_INFLUXDB_ADDRESS,
						CONFIG_INFLUXDB_PORT, CONFIG_INFLUXDB_NAME);
	} else {
		if(CONFIG_INFLUXDB_PWD == NULL){
			zbx_error("Password missing: %s", error);
			zbx_free(error);
			exit(EXIT_FAILURE);
		}
		zbx_snprintf(influxdb_write_url, CURL_LEN, "%s://%s:%s/write?db=%s&u=%s&p=%s", CONFIG_INFLUXDB_PROTOCOL, CONFIG_INFLUXDB_ADDRESS,
						CONFIG_INFLUXDB_PORT, CONFIG_INFLUXDB_NAME, CONFIG_INFLUXDB_USER, CONFIG_INFLUXDB_PWD);
	}
	zabbix_log(LOG_LEVEL_INFORMATION, "[%s] Initialised History InfluxDB module, target: %s", MODULE_NAME, influxdb_write_url);

	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit(void)
{
	return ZBX_MODULE_OK;
}

/******************************************************************************
 *
 *	Function: write_to_influxdb
 *
 *	Purpose: Sets up an http connection using curl and writes a pre-formatted
 *				string to a specified influxdb url
 *
 ******************************************************************************/

void write_to_influxdb(char *influxdb_data_entry) {
	CURL *curl;
	CURLcode res;
	int i;

	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, influxdb_write_url);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, CONFIG_INFLUXDB_SSL_INSECURE ? 0L : 1L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, CONFIG_INFLUXDB_SSL_INSECURE ? 0L : 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, influxdb_data_entry);
		res = curl_easy_perform(curl);

		if(res != CURLE_OK){
			zabbix_log(LOG_LEVEL_ERR, "[%s] curl_easy_perform() failed: %s", MODULE_NAME, curl_easy_strerror(res));
		}
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     completed write_to_influxdb", MODULE_NAME);
}

/******************************************************************************
 *
 *	Function: host_item_name_query
 *
 *	Purpose: Performs a query to internal database to return human-readable
 *			info
 *
 *	Parameters: itemid
 *
 *	Returns: A string in a format which can be added to influxdb_data_entry for curl
 *			request
 *
 ******************************************************************************/

char *itemid_to_influx_data(zbx_uint64_t itemid)
{
	DB_RESULT	result;
	DB_ROW		row;
	char *ret_string;

	result = DBselect("SELECT replace(replace("
							"coalesce("
							  "replace("
							    "replace("
							      "replace("
							        "replace("
							          "replace("
							            "replace("
							              "replace("
							                "replace("
							                  "replace("
							                    "i.name, '$1',"
							                    "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 1)), '$2',"
							                  "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 2)), '$3',"
							                "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 3)), '$4',"
							              "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 4)), '$5',"
							            "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 5)), '$6',"
							          "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 6)), '$7',"
							        "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 7)), '$8',"
							      "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 8)), '$9',"
							    "split_part(substring(i.key_ FROM '\\[(.+)\\]'), ',', 9)),"
								"i.name"
							"), ',', '\\,'), ' ', '\\ ') ||"

							"',host_name=' || replace(replace(("
							"select h.name from hosts h where h.hostid=i.hostid"
							"), ' ', '\\ '), ',', '\\,') ||"

							"',host_groups=' || coalesce(replace(("
							"select string_agg(g.name, '|') "
							"from groups g "
							"inner join hosts_groups hg on hg.groupid = g.groupid "
							"where hg.hostid=i.hostid"
							"), ' ', '\\ '),'') ||"

							// "',item_key=' || replace(replace(("
							// "i.key_"
							// "), ' ', '\\ '), ',', '\\,') ||"

							"',applications=' || coalesce(replace(replace(("
							"select string_agg(a.name, '|') "
							"from applications a "
							"inner join items_applications ia on ia.applicationid = a.applicationid "
							"where ia.itemid=i.itemid"
							"), ' ', '\\ '), ',', '\\,'),'') "

						"from items i where i.itemid=" ZBX_FS_UI64, itemid);

	if (NULL != (row = DBfetch(result)))
	{
		ret_string = zbx_strdup(NULL, row[0]);
	}
	else {
		zabbix_log(LOG_LEVEL_ERR, "[%s] query result returned null", MODULE_NAME);
		exit(EXIT_FAILURE);
	}
	DBfree_result(result);
	return ret_string;
}

/******************************************************************************
 *
 * Functions: history_general_cb
 *			  history_float_cb
 *            history_integer_cb                                        	  *
 *            history_string_cb                                         	  *
 *            history_text_cb                                           	  *
 *            history_log_cb                                            	  *
 *                                                                            *
 * Purpose: callback functions for storing historical data of types float,    *
 *          integer, string, text and log respectively in external storage    *
 *                                                                            *
 * Parameters: history     - array of historical data                         *
 *             history_num - number of elements in history array              *
 *                                                                            *
 ******************************************************************************/

#define ZBX_ITEM_FLOAT 1
#define ZBX_ITEM_INTEGER 2
#define ZBX_ITEM_STRING 3
#define ZBX_ITEM_TEXT 4
#define ZBX_ITEM_LOG 5

static void history_general_cb(const int item_type, const void *history, int history_num){
	int i;

	ZBX_HISTORY_FLOAT	*history_float = NULL;
	ZBX_HISTORY_INTEGER	*history_integer = NULL;
	ZBX_HISTORY_STRING	*history_string = NULL;
	ZBX_HISTORY_TEXT	*history_text = NULL;
	ZBX_HISTORY_LOG 	*history_log = NULL;

	switch(item_type){
		case  ZBX_ITEM_FLOAT:
			history_float = (ZBX_HISTORY_FLOAT*)history;
			break;
		case  ZBX_ITEM_INTEGER:
			history_integer = (ZBX_HISTORY_INTEGER*)history;
			break;
		case  ZBX_ITEM_STRING:
			history_string = (ZBX_HISTORY_STRING*)history;
			break;
		case  ZBX_ITEM_TEXT:
			history_text = (ZBX_HISTORY_TEXT*)history;
			break;
		case  ZBX_ITEM_LOG:
			history_log = (ZBX_HISTORY_LOG*)history;
			break;
		default:
			THIS_SHOULD_NEVER_HAPPEN;
	}

	char influxdb_data_entry[(METRIC_LEN + ITEM_VALUE_LEN + 20) * history_num];
	influxdb_data_entry[0] = '\0';

	zbx_uint64_t itemid, int_val;
	int cl, ns, log_timestamp, log_logid, log_sev;
	double float_val;
	const char str_val[ITEM_VALUE_LEN], *log_src;
	char timestamp[20];

	char *influx_data = NULL;

	for(i = 0; i < history_num; i++){
		switch(item_type){
			case  ZBX_ITEM_FLOAT:
				itemid = history_float[i].itemid;
				cl = history_float[i].clock;
				ns = history_float[i].ns;
				float_val = history_float[i].value;
				break;
			case  ZBX_ITEM_INTEGER:
				itemid = history_integer[i].itemid;
				cl = history_integer[i].clock;
				ns = history_integer[i].ns;
				int_val = history_integer[i].value;
				break;
			case  ZBX_ITEM_STRING:
				itemid = history_string[i].itemid;
				cl = history_string[i].clock;
				ns = history_string[i].ns;
				break;
			case  ZBX_ITEM_TEXT:
				itemid = history_text[i].itemid;
				cl = history_text[i].clock;
				ns = history_text[i].ns;
				break;
			case  ZBX_ITEM_LOG:
				itemid = history_log[i].itemid;
				cl = history_log[i].clock;
				ns = history_log[i].ns;
				log_timestamp = history_log[i].timestamp;
				log_logid = history_log[i].logeventid;
				log_sev = history_log[i].severity;
				log_src = history_log[i].source;
				break;
			default:
				THIS_SHOULD_NEVER_HAPPEN;
		}

		influx_data = itemid_to_influx_data(itemid);

		char entry[METRIC_LEN + ITEM_VALUE_LEN + 20];
		zbx_snprintf(timestamp, sizeof(timestamp), "%09d%09d", cl, ns);

		switch(item_type){
			case  ZBX_ITEM_FLOAT:
				zbx_snprintf(entry, sizeof(entry), "%s value=%f %s\n", influx_data, float_val, timestamp);
				break;
			case  ZBX_ITEM_INTEGER:
				zbx_snprintf(entry, sizeof(entry), "%s value=%llu %s\n", influx_data, int_val, timestamp);

				break;
			case  ZBX_ITEM_STRING:
				zbx_snprintf(entry, sizeof(entry), "%s value=\"%s\" %s\n", influx_data, history_string[i].value, timestamp);

				break;
			case  ZBX_ITEM_TEXT:
				zbx_snprintf(entry, sizeof(entry), "%s value=\"%s\" %s\n", influx_data, history_text[i].value, timestamp);

				break;
			case  ZBX_ITEM_LOG:
				zbx_snprintf(entry, sizeof(entry), "%s,logeventid=%d,severity=%d,source=%s value=\"%s\" %s\n", influx_data, log_logid, log_sev, log_src, history_log[i].value, log_timestamp);
				break;
			default:
				THIS_SHOULD_NEVER_HAPPEN;
		}
		/* + 2 for new line character */
		zbx_strlcat(influxdb_data_entry, entry, strlen(influxdb_data_entry) + strlen(entry) + 2);

		// clean up
		zbx_free(influx_data);
	}
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     influxdb_data_entry: %s", MODULE_NAME, influxdb_data_entry);
	write_to_influxdb(influxdb_data_entry);

}


static void	history_float_cb(const ZBX_HISTORY_FLOAT *history, int history_num)
{
  	zabbix_log(MODULE_LOG_LEVEL, "[%s] Syncing %d float values", MODULE_NAME, history_num);
	history_general_cb(ZBX_ITEM_FLOAT, (const void *) history, history_num);
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     Finished syncing float values", MODULE_NAME, history_num);
}


static void	history_integer_cb(const ZBX_HISTORY_INTEGER *history, int history_num)
{
  	zabbix_log(MODULE_LOG_LEVEL, "[%s] Syncing %d int values", MODULE_NAME, history_num);
	history_general_cb(ZBX_ITEM_INTEGER, (const void *) history, history_num);
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     Finished syncing int values", MODULE_NAME, history_num);
}

static void	history_string_cb(const ZBX_HISTORY_STRING *history, int history_num)
{
  	zabbix_log(MODULE_LOG_LEVEL, "[%s] Syncing %d string values", MODULE_NAME, history_num);
	history_general_cb(ZBX_ITEM_STRING, (const void *) history, history_num);
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     Finished syncing string values", MODULE_NAME, history_num);
}

static void	history_text_cb(const ZBX_HISTORY_TEXT *history, int history_num)
{
  	zabbix_log(MODULE_LOG_LEVEL, "[%s] Syncing %d text values", MODULE_NAME, history_num);
	history_general_cb(ZBX_ITEM_TEXT, (const void *) history, history_num);
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     Finished syncing history values", MODULE_NAME, history_num);
}

static void	history_log_cb(const ZBX_HISTORY_LOG *history, int history_num)
{
  	zabbix_log(MODULE_LOG_LEVEL, "[%s] Syncing %d log values", MODULE_NAME, history_num);
	history_general_cb(ZBX_ITEM_LOG, (const void *) history, history_num);
	zabbix_log(MODULE_LOG_LEVEL, "[%s]     Finished syncing log values", MODULE_NAME, history_num);
}


/******************************************************************************
 *                                                                            *
 * Function: zbx_module_history_write_cbs                                     *
 *                                                                            *
 * Purpose: returns a set of module functions Zabbix will call to export      *
 *          different types of historical data                                *
 *                                                                            *
 * Return value: structure with callback function pointers (can be NULL if    *
 *               module is not interested in data of certain types)           *
 *                                                                            *
 ******************************************************************************/
ZBX_HISTORY_WRITE_CBS	zbx_module_history_write_cbs(void)
{
	static ZBX_HISTORY_WRITE_CBS	callbacks =
	{
		history_float_cb,
		history_integer_cb,
		history_string_cb,
		NULL, // history_text_cb, (not tested)
		NULL, // history_log_cb, (not tested)
	};

	return callbacks;
}

#
