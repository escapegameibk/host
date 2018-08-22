/* sound controller for the escape game innsbruck
 * Copyright © 2018 tyrolyean
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

#ifndef SOUND_H
#define SOUND_H

#include <vlc/vlc.h>
#include <json-c/json.h>

int init_sound();
int sound_trigger(json_object* trigger);
int init_sound_dependency(json_object * dependency);
int snd_init_dependency(json_object* dependency);
int play_sound(const char* url);
int play_effect(const char* url);
int reset_sounds();

libvlc_instance_t * vlc_inst;
libvlc_media_player_t **vlc_mp;
libvlc_media_player_t *effect_player;
size_t playercnt;

#endif
