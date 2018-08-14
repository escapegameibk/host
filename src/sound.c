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
#include "sound.h"
#include <stdlib.h>
#include <strings.h>
#include "config.h"
#include "log.h"
#include "tools.h"

int init_sound(){

        vlc_inst = libvlc_new (0, NULL);
        vlc_mp = malloc(0);

        /* If available play startup sound */
        const char* boot_snd = json_object_get_string(
                                json_object_object_get(config_glob,
                                        "boot_sound"));
        if(boot_snd != NULL){
		play_sound(boot_snd);
        }

        return 0;
}

int sound_trigger(json_object* trigger){

	json_object* action_obj = json_object_object_get(trigger, "action");

	if(action_obj == NULL){
		println("No action specified for sound trigger!", ERROR);
		return -1;
	}
	const char* action = json_object_get_string(action_obj);

	if(strcasecmp(action, "play") == 0){

		json_object* resource_obj = json_object_object_get(trigger, 
			"resource");

		const char* resource = json_object_get_string(resource_obj);

		println("Playing sound file from %s", DEBUG, resource);
	
		return play_sound(resource);

	} else if(strcasecmp(action, "reset") == 0){

		return reset_sounds();

	}else{
		println("Unknown action to trigger for sound module!", ERROR);
	}
        const char* resource_locator = json_object_get_string(
                json_object_object_get(trigger,"resource"));
        if(resource_locator == NULL){
                println("attemted to play sound without proper locator!",
                        ERROR);
                return -1;
        }


        return 0;

}

int snd_init_dependency(json_object* dependency){

	/* Not really nescessary */

	return 0;
}

int play_sound(const char* url){
        
	libvlc_media_t *m = libvlc_media_new_location (vlc_inst, 
                url);

	if(m == NULL){
		println("snd failed to get media from resource %s!", ERROR,
			url);
		return -1;
	}

	println("Updating cue to %i currently playing players.", DEBUG, 
		++playercnt);

        vlc_mp = realloc(vlc_mp, playercnt * sizeof (libvlc_media_player_t *));
        vlc_mp[playercnt-1] = libvlc_media_player_new_from_media (m);
        libvlc_media_release(m);
        libvlc_media_player_play(vlc_mp[playercnt-1]);

	/* Wait for alsa to respond, as the vlc handles everything from now on
	 * in a separate thread.
	 */

	sleep_ms(100);

	return 0;

}

int reset_sounds(){

	for(size_t i = 0; i < playercnt; i++){
		libvlc_media_player_release(vlc_mp[i]);
	}

	free(vlc_mp);
	vlc_mp = NULL;
	playercnt = 0;

	return 0;

}
