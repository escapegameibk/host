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

#include "statistics.h"
#include "log.h"
#include "core.h"
#include "tools.h"
#include "hints.h"
#include "config.h"
#include "game.h"

struct statistics_t get_statistics(){

	struct statistics_t stats;
	stats.start_time = ec_time_to_unix(game_timer_start);
	if(game_timer_end < game_timer_start){
		println("Statistics requested, game not yet finished!", 
			WARNING);
	}
	stats.duration = game_timer_end - game_timer_start;
	stats.lang = language;
	stats.hint_count = get_hint_exec_cnt();
	stat_int exec_events = 0;
	for(size_t i = 0; i < event_cnt; i++){
		if(event_trigger_status[i] >= EVENT_IN_EXECUTION){
			exec_events++;
		}
	}
	stats.executed_events = exec_events;
	stats.overriden_events = event_overrides;

	return stats;

}
