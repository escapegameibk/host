/* A configuration parser for the escape game innsbruck
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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "log.h"

int load_config(const char* location){

        char * config_path;
        
        if(location == NULL){
                
                /* the default location is a file in the same directory called
                 * config.json
                 */
                config_path = "config.json";
        }else{
                config_path = (char*)location;
        }
	println("parsing config from file: %s",DEBUG, config_path);

        config_glob = json_object_from_file(config_path);

        if(config_glob == NULL){
                println("FAILED TO PARSE CONFIG!",ERROR);
                const char* error = json_util_get_last_err();
                if(error != NULL){

                        println(json_util_get_last_err(),ERROR);
                }
                return -2;
        }

	/* Initialize language */
	json_object* langs = json_object_object_get(config_glob, "langs");
	if(langs == NULL){
		println("No languages have been specified!", DEBUG);
		println("Assuming single language mode!", DEBUG);
		goto end_langs;
	}else{
		size_t langcnt = json_object_array_length(langs);
		println("A total of %i languages have been specified:",
			DEBUG, langcnt);
		for(size_t i = 0; i < langcnt; i++){
			println("\t%i:\t%s", DEBUG, i, 
				json_object_get_string(
				json_object_array_get_idx(langs,i)));
		}
	}

	json_object* lang = json_object_object_get(config_glob, "default_lang");
	if(lang == NULL){
		println("No default language specified!", WARNING);
		println("Assuming first language in definition!", WARNING);
		language = 0;
		goto end_langs;
	}else{
		language = json_object_get_int(lang);
		if(language >= json_object_array_length(langs)){
			println("Are you fucking kidding me?!", ERROR);
			println("There are %i languages defined and you told \
me to use No %i? Think about that for a moment please.", ERROR, 
				json_object_array_length(langs), language + 1);
			return -2;
		}
		println("Setting default language to be %i/%s", DEBUG, language,
			json_object_get_string(json_object_array_get_idx(langs,
			language)));
	}
end_langs:
	println("Done initializing configuration", DEBUG);

        return 0;
}

/* Can get passed an array, which will be indexed and returned, an object of
 * any kind */

const char* get_name_from_object(json_object* obj){

	if(obj == NULL){
		return "UNSPECIFIED";
	}

	if(json_object_is_type(obj, json_type_string)){
		/* Probably got a single-language name DIRECTLY passed. Very
		 * edge case, and i wouldn't really recommend doing this, but it
		 * should be fine. */
		return json_object_get_string(obj);
	}

	json_object* name_obj = obj;
	if(json_object_object_get(obj,"name") != NULL){
		/* I hope i got passed the root-object rather than the name 
		 * itself
		 */
		name_obj = json_object_object_get(obj,"name");
	}

	if(json_object_is_type(name_obj, json_type_string)){
		/* It is a single-language name */
		return json_object_get_string(name_obj);
	}

	if(json_object_is_type(name_obj, json_type_array)){
		/* It is a multi-language name */
		if(language < json_object_array_length(name_obj)){
		
			return json_object_get_string(json_object_array_get_idx(
				name_obj, language));
			
		}

	}
	
	if(name_obj == NULL){
		return "UNSPECIFIED";
	}
	
	println("MISSCONFIGURED NAME OBJECT!!! DUMPING ROOT-OBJECT:",
		WARNING);
	json_object_to_fd(STDOUT_FILENO, obj, JSON_C_TO_STRING_PRETTY);

	return "ERROR";

}
