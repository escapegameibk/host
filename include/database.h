/* Database connection and game interface
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

#ifndef DATABASE_H
#define DATABASE_H

#ifndef NODATABASE

#include <stddef.h>
#include <json-c/json.h>

#define DB_NOTYPE     0 /* Equivalent to beeing disabled */
#define DB_POSTGRESQL 1

size_t db_type;

char* postgresql_uri;

json_object* statistics_config;

int init_database();

#endif /* NODATABASE */
#endif
