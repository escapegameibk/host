/* Statistic collection module for the escape game System's host
 * Copyright © 2019 tyrolyean
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "database.h"
#include "log.h"
#include "core.h"
#include "tools.h"
#include "hints.h"
#include "config.h"
#include "game.h"
#include "statistics.h"

char* postgresql_uri = NULL;

#include <string.h>
#include <stdio.h>

#ifndef NODATABASE

int init_database(){

	json_object* database_config = json_object_object_get(config_glob, 
		"database");
	
	if(database_config == NULL){
		println("Database module loaded but no configuration provided!",
			WARNING);
		
		println("Database module disableing", DEBUG);
		db_type = DB_NOTYPE;
		
		return 0;
	}
	
	if(!json_object_is_type(database_config, json_type_object)){
		println("Database configuration has wrong type! Dumping root:",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, database_config, 
			JSON_C_TO_STRING_PRETTY);
		
		return 0;
		
	}

	json_object* database_type = json_object_object_get(database_config, 
		"type");

	if(database_type == NULL){
		println("Database type not specified!", ERROR);
		return -1;
	}

	const char* type = json_object_get_string(database_type);

	if(strcasecmp("postgresql",type) == 0){
		
		db_type = DB_POSTGRESQL;
		println("Using postgresql as database type!", DEBUG);
		
		json_object* username_o;
		json_object*  password_o;
		json_object*  port_o;
		json_object*  host_o;
		json_object*  database_o;
		
		
		username_o = json_object_object_get(database_config, 
			"username");
		
		password_o = json_object_object_get(database_config, 
			"password");
		
		database_o = json_object_object_get(database_config, 
			"database");
		
		host_o = json_object_object_get(database_config, 
			"host");

		port_o = json_object_object_get(database_config, 
			"port");
		
		if(username_o == NULL){
			println("Database username not specified!", ERROR);
			return -1;
		}
		
		if(password_o == NULL){
			println("Database password not specified!", ERROR);
			return -1;
		}
		
		if(database_o == NULL){
			println("Database to use not specified!", ERROR);
			return -1;
		}
		
		if(host_o == NULL){
			println("Database host not specified!", ERROR);
			return -1;
		}
		if(port_o == NULL){
			println("Database port not specified!", ERROR);
			return -1;
		}
		
		const char* username = json_object_get_string(username_o);
		const char* password = json_object_get_string(password_o);
		const char* database = json_object_get_string(database_o);
		const char* host = json_object_get_string(host_o);
		const char* port = json_object_get_string(port_o);
		
		const char* format = "postgresql://%s:%s@%s:%s/%s";
		size_t length = strlen(username) + strlen(password) + 
			strlen(username) + strlen(host) + strlen(port) + 
			strlen(format) + strlen(database) + 1; 
		/* I know that the calculation ignores the %s, but at this point
		 * i don't care */
		
		postgresql_uri = malloc(length);
		memset(postgresql_uri, 0, length);
		snprintf(postgresql_uri, length, format, username, password, 
			host, port, database);

		println(postgresql_uri, DEBUG_MORE);

	}else{
		println("Invalid Database Type Specified: [%s]!", ERROR, type);
		return -1;
	}

	statistics_config = json_object_object_get(database_config, 
		"statistics");
	
	if(statistics_config == NULL){
		println("Statistic collection not configured, therefore not "
			"performed!", DEBUG);
		
		return 0;
	}

	if(!json_object_is_type(statistics_config,json_type_object)){
		println("Database statistics config has wrong type. Dumping!",
			ERROR);
		
		json_object_to_fd(STDOUT_FILENO, database_config, 
			JSON_C_TO_STRING_PRETTY);

		return -1;
	}
	
	return 0;
}

char* sql_generate_stat_insert(struct statistics_t stats){

	if(statistics_config == NULL){
		println("Attempted to generate insert statemnet for statistics "
			"without statistics configured!", ERROR);
		return NULL;
	}

	char * statement_format = "INSERT INTO %s (%s) VALUES (%s)";

	char* columns = malloc(1);
	char* values = malloc(1);
	columns[0] = '\0';
	values[0] = '\0';

	json_object* start_time_column = json_object_object_get(
		statistics_config, "start_time");
	json_object* duration_column = json_object_object_get(
		statistics_config, "duration");
	json_object* language_column = json_object_object_get(
		statistics_config, "language");
	json_object* hint_count_column = json_object_object_get(
		statistics_config, "hint_count");
	json_object* executed_events_column = json_object_object_get(
		statistics_config, "executed_events");
	json_object* forced_executions_column = json_object_object_get(
		statistics_config, "forced_executions");

	if(start_time_column != NULL){
		
	}
	
	if(duration_column != NULL){
		
	}

	if(language_column != NULL){
		
	}

	if(hint_count_column != NULL){
		
	}

	if(executed_events_column != NULL){
		
	}

	if(forced_executions_column != NULL){

		
	}



	json_object* table_o = json_object_object_get(statistics_config,
		"table");

	if(table_o == NULL){
		println("Failed to get database table for statistics!", ERROR);
		return NULL;
	}

	const char* table = json_object_get_string(table_o);

	size_t len = strlen(columns) + strlen(values) + strlen(table) + 
		strlen(statement_format) + 1;

	char* statement = malloc(len);
	memset(statement, 0 , len);

	snprintf(statement, len, statement_format, table, columns, values);
	return statement;
}
#endif /* NODATABASE */
