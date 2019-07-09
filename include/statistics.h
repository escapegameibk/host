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

#ifndef STATS_H
#define STATS_H

#include <time.h>

typedef long long int stat_int;
typedef unsigned long long int ustat_int;

struct statistics_t {

	time_t start_time; /* A unix timestamp */
	ustat_int duration;
	size_t lang;
	ustat_int hint_count;
	ustat_int executed_events; /* Amount of executed events */
	ustat_int overriden_events; /* Amount of overrides by game 
					     * leader via interface */

};

struct statistics_t get_statistics();

#endif /* STATS_H */
