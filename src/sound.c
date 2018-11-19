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
#include "config.h"
#include "log.h"
#include "tools.h"

#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

bool sound_muted = false;

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
	effect_player = libvlc_media_player_new(vlc_inst);

        return 0;
}

int sound_trigger(json_object* trigger, bool dry){

	json_object* action_obj = json_object_object_get(trigger, "action");

	if(action_obj == NULL){
		println("No action specified for sound trigger!", ERROR);
		return -1;
	}
	const char* action = json_object_get_string(action_obj);

	if(strcasecmp(action, "play") == 0){

		json_object* resource_obj = json_object_object_get(trigger, 
			"resource");

		const char* resource = get_lang_resource(resource_obj);

		println("Playing sound file from %s", DEBUG, resource);
		if(dry){
			return 0;
		}
		return play_sound(resource);
	
	}else if(strcasecmp(action, "effect") == 0){

		json_object* resource_obj = json_object_object_get(trigger, 
			"resource");
		
		const char* resource = json_object_get_string(resource_obj);

		println("Playing sound effect from %s", DEBUG, resource);
		
		if(dry){
			return 0;
		}
	
		return play_effect(resource);

	
	} else if(strcasecmp(action, "reset_effect") == 0){
		
		if(dry){
			return 0;
		}

		libvlc_media_player_stop(effect_player);
		return 0;

	} else if(strcasecmp(action, "reset") == 0){

		if(dry){
			return 0;
		}
		
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

int play_sound(const char* url){

	if(sound_muted){
		/* Global mute is on! This means, that no new sounds will be
		 * played. Old sounds will still remain*/
		return 0;
	}
        
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

/* There can only be one effect at once. It will be cleared, if another effect
 * is triggered. This is done to reduce stress on the sound device */
int play_effect(const char* url){
	
	libvlc_media_t *m = libvlc_media_new_location (vlc_inst, 
                url);

	if(m == NULL){
		println("snd failed to get media from resource %s!", ERROR,
			url);
		return -1;
	}
	println("Updating currently playing effect", DEBUG);

	libvlc_media_player_set_media(effect_player, m);
        
	libvlc_media_release(m);
        libvlc_media_player_play(effect_player);

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

/* Pass a json object wich somehow contains a resource field. Please try to only
 * pass the root object in here, aka a trigger */
const char* get_lang_resource(json_object* obj){

	if(json_object_is_type(obj, json_type_string)){
		/* Directly passed single language sound object. Common and
		 * very well possible*/
		return json_object_get_string(obj);
	}

	json_object* lang_obj = NULL;

	if(json_object_is_type(obj, json_type_array)){
		lang_obj = obj;
	}else{
		lang_obj = json_object_object_get(obj, "resource");
	}
	
	if(!json_object_is_type(obj, json_type_array)){
		println("Failed to get multilang resource! Dumping root: ",
			WARNING);
		json_object_to_fd(STDOUT_FILENO, obj, JSON_C_TO_STRING_PRETTY);
		return "ERROR";
	}
	
	/* The get name function can get passed an array, and will return the
	 * array's index at the current language */
	return get_name_from_object(lang_obj);

}
