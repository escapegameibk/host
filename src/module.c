/* Module specific functions for the escape game innsbruck's host
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

#include "module.h"
#include "mtsp.h"
#include "ecproto.h"
#include "interface.h"
#include "hints.h"
#include "core.h"
#include "sound.h"
#include "log.h"
#include "game.h"
#include "config.h"
#include "video.h"
#include "lolproto.h"

#include "modbus.h"

#include <string.h>
#include <unistd.h>

/*
 * FUNCTIONS TO LOAD AND INITIALIZE MODULES
 *
 * All modules may be initialized, stated, and reset in here. Please add your
 * module to the desired functions and add a #IFNDEF entry
 *
 */

/*
 * Called on program load. Gives the modules a chance to initialize internal
 * data strutures before needing to access them on startup. If one module failes
 * to init, < 0 is returned and a error message is printed out. On success >= 0
 * is returned. If the order of modules is changed, errors may occure. Please
 * don't attempt to do this. Implementation of module-dependencies is a TODO
 */
int init_modules(){

	println("STEP 1/3: Attempting module initialization", DEBUG);

#ifndef NOHINTS
        if(init_hints() < 0){
                println("failed to init hints!!",ERROR);
		return -1;
        }
	
#endif

#ifndef HEADLESS
        if(init_interface() < 0){
                println("failed to init interface!!",ERROR);
		return -2;
        }
#endif

#ifndef NOSOUND
        if(init_sound() < 0){
                println("failed to init sound module!!",ERROR);
		return -3;
        }
#endif        

#ifndef NOMTSP
        if(init_mtsp() < 0){

                println("failed to init mtsp connection!!",ERROR);
		return -4;

        }
#endif

#ifndef  NOEC
        if(init_ecp() < 0){

                println("failed to init ecp connection!!",ERROR);
		return -5;

        }
#endif

#ifndef NOVIDEO
	
	if(init_video() < 0){
		println("failed to initialize video module", ERROR);
		return -6;
	}
#endif

#ifndef NOLOL
	
	if(init_lol() < 0){
		println("failed to initialize lolproto module", ERROR);
		return -7;
	}
#endif


#ifndef NOMODBUS
	if(init_modbus() < 0){
		println("failed to initialize modbus module", ERROR);
		return -8;
	}


#endif
	
	/* The core module may NOT be disabled */
	if(init_core() < 0){
		println("Failed to initilize core!", ERROR);
		return -9;
	}
	

	println("STEP 1/3: Modules successfully initialized", DEBUG);

	return 0;
}

/*
 * Called after all modules have been loaded. Gives modules the chance to start
 * threads and start connections to remote hosts. If one module failes
 * to start, < 0 is returned and a error message is printed out. On success >= 0
 * is returned. If the order of modules is changed, errors may occure. Please
 * don't attempt to do this. Implementation of module-dependencies is a TODO.
 */

int start_modules(){
	
	println("STEP 2/3: Attempting module start", DEBUG);

#ifndef HEADLESS
        if(start_interface() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}
#endif

#ifndef NOHINTS
	if(start_hints() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}
#endif

#ifndef NOMTSP
        if(start_mtsp() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}
#endif
#ifndef NOEC
	if(start_ecp() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}

#ifndef NOLOL
	
	if(start_lol() < 0){
		println("failed to start lolproto module", ERROR);
		return -7;
	}
#endif

#endif
        if(start_core() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}
	
	println("STEP 2/3: Module start successfull.", DEBUG);

	return 0;
}

/* When all is up and running, perform general module checks, test stuff, 
 * perform dry runs.
 */
int test_modules(){
	
	println("STEP 3/3: Attempting module tests", DEBUG);

	/* Check all dependencies once on startup and check, wether they return
	 * successfully.
	 */
	println("Testing %i dependencies for errors", DEBUG, dependency_count);
	for(size_t i = 0; i < dependency_count; i++){
		if(check_dependency(dependency_list[i]) < 0){
			println("Failed to test dependency no. %i!!",
				ERROR, i);
			println("Aborting Module tests and dumping dependency:",
				 ERROR);
		
			json_object_to_fd(STDOUT_FILENO, dependency_list[i],
				JSON_C_TO_STRING_PRETTY);
			return -1;
		}
	}
	size_t len = 0;
	json_object** trigs = get_root_triggers(&len, NULL);
	println("Attempting dry run for %i triggers", DEBUG, len);

	for(size_t i = 0; i < len; i++){
		
		if(execute_trigger(trigs[i], true) < 0){
			
			println("Failed to test trigger no. %i!!",
				ERROR, i);
			println("Aborting Module tests and dumping trigger:",
				 ERROR);
			json_object_to_fd(STDOUT_FILENO, trigs[i],
				JSON_C_TO_STRING_PRETTY);
			return -1;
		}
	}

	println("STEP 3/3: Module tests successfull.", DEBUG);
	println("Modules finished start up. Reporting ready.", DEBUG);
	
	return 0;
}

/*
 * Called on game reset. Gives all modules the chance to send new notifies to
 * devices, reconnect tcp streams before a timeout occures, or do something 
 * else which shouldn't be run whilest a game is in progress
 */
int reset_modules(){

#ifndef NOSOUND	
	if(reset_sounds() < 0){
		println("Failed to reset sound module!", ERROR);
		return -1;
	}
#endif

#ifndef NOMTSP
	if(reset_mtsp() < 0){
		println("Failed to reset mtsp module!", ERROR);
		return -2;
	}

#endif

#ifndef NOHINTS
	if(reset_hints() < 0){
		println("Failed to reset hinting module!", ERROR);
		return -3;
	}

#endif
	return 0;
}



/* Checks wether a dependency is met
 * Returns < 0 on error, 0 if not, and > 0 if forfilled.
 * 
 * returns return value of the specific module
 */

int check_dependency(json_object* dependency){

	const char* module_name = json_object_get_string(
				json_object_object_get(dependency,"module"));
	if(module_name == NULL){
		println("Specified no module in dependency! Misconfiguration!",
			ERROR);
			json_object_to_fd(STDOUT_FILENO, dependency,
				JSON_C_TO_STRING_PRETTY);
		return -1;
	}
#ifndef NOMTSP
	else if(strcasecmp(module_name,"mtsp") == 0){
		/* Question the MTSP module. yes QUESTION IT! */
		return mtsp_check_dependency(dependency);
	}
#endif
#ifndef NOEC
	else if(strcasecmp(module_name,"ecp") == 0){
		/* Question the ECP module. yes QUESTION IT! */
		return ecp_check_dependency(dependency);
	}
#endif
	else if(strcasecmp(module_name,"core") == 0){
		/* Pass on to the core module */
		return core_check_dependency(dependency);
	}
	else{
		println("Unknown module specified [%s]!", ERROR, module_name);
		return -2;
	}


	return 0;
}

/* Execute a trigger specified in the trigger object. In case the dry flag is
 * set, only pretend to execute it, don't actually execute itm but still perform
 * sanity checks.
 */
int execute_trigger(json_object* trigger, bool dry){


	const char* module = json_object_get_string(json_object_object_get(
                trigger,"module"));
	if(module == NULL){
		println("Trigger without module! dumping:", ERROR);
		json_object_to_fd(STDOUT_FILENO,trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;

	}
	println("Executing trigger %s", DEBUG, get_name_from_object(trigger));

        /* Find out which module is concerned and execute the trigger
         * in the specified function of the module.
         */

        if(strcasecmp(module,"core") == 0){
                /* The core module is concerned. */
                if(core_trigger(trigger, dry) < 0){
                        println("Failed to execute trigger for core!",
                                ERROR);
                        return -1;
                }
		return 0;
        }

#ifndef NOMTSP
	else if(strcasecmp(module,"mtsp") == 0){
                /* The mtsp module is concerned. */
                if(mtsp_trigger(trigger, dry) < 0){
                        println("Failed to execute trigger for mtsp!",
                                ERROR);
                        return -2;

                }
		return 0;
	}
#endif
#ifndef NOEC
	else if(strcasecmp(module,"ecp") == 0){
                /* The ecproto module is concerned. */
                if(ecp_trigger(trigger, dry) < 0){
                        println("Failed to execute trigger for ecp!",
                                ERROR);

                        return -3;

                }
		return 0;
	}
#endif

#ifndef NOSOUND

	else if(strcasecmp(module,"snd") == 0){
		/* The sound module is concerned. */
		if(sound_trigger(trigger, dry) < 0){
			println("Failed to execute trigger for snd!",
				ERROR);

			return -4;
		}
		return 0;
	}
#endif

#ifndef NOVIDEO

	else if(strcasecmp(module,"video") == 0){
		/* The video module is concerned. */
		if(video_trigger(trigger, dry) < 0){
			println("Failed to execute trigger for video!",
				ERROR);

			return -5;
		}
		return 0;
	}
#endif

#ifndef NOMODBUS

	else if(strcasecmp(module,"modbus") == 0){
		/* The modbus module is concerned. */
		if(modbus_trigger(trigger, dry) < 0){
			println("Failed to execute trigger for modbus!",
				ERROR);

			return -6;
		}
		return 0;
	}
#endif

        println("UNKNOWN MODULE %s!!",ERROR, module);
	return -7;

}

/* This function passes the dependency to it's module. It may add metadata,
 * but still prepares the dependency so it may be retrieved via the check 
 * dependency function. */
int init_general_dependency(json_object* dependency){
	
	/* Add metadata to dependency */
	json_object_object_add(dependency, "id", json_object_new_int(
		dependency_count - 1));

	const char* module_name = json_object_get_string(
				json_object_object_get(dependency,"module"));

	if(module_name == NULL){
		println("Specified no module in dependency!", ERROR);
			json_object_to_fd(STDOUT_FILENO, dependency,
				JSON_C_TO_STRING_PRETTY);
		return -1;
	}
#ifndef NOMTSP
	else if(strcasecmp(module_name,"mtsp") == 0){
		/* Question the MTSP module */
		return mtsp_init_dependency(dependency);
	}
#endif
	else if(strcasecmp(module_name,"core") == 0){
		/* Question the core module */
		return core_init_dependency(dependency);
	}
	
#ifndef NOEC
	else if(strcasecmp(module_name,"ecp") == 0){
		/* Question the ecp module */
		return ecp_init_dependency(dependency);
	
	}
#endif
	else{
		println("Unknown module specified [%s]!", ERROR, module_name);
		return -2;
	}

	return 0;
	
}
