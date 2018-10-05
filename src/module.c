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

	println("STEP 1/2: Attempting module initialization", DEBUG);

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
	
	/* The core module may NOT be disabled */
	if(init_core() < 0){
		println("Failed to initilize core!", ERROR);
		return -6;
	}
	

	println("STEP 1/2: Modules successfully initialized", DEBUG);

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
	
	println("STEP 2/2: Attempting module start", DEBUG);

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
#endif
        if(start_core() < 0){
		println("Failed to start interface!", ERROR);
		return -1;
	}
	
	println("STEP 2/2: Module start successfull.", DEBUG);

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

int execute_trigger(json_object* trigger){


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
                if(core_trigger(trigger) < 0){
                        println("Failed to execute trigger for core!",
                                ERROR);
                        return -1;
                }
		return 0;
        }

#ifndef NOMTSP
	else if(strcasecmp(module,"mtsp") == 0){
                /* The mtsp module is concerned. */
                if(mtsp_trigger(trigger) < 0){
                        println("Failed to execute trigger for mtsp!",
                                ERROR);

                        return -2;

                }
		return 0;
	}
#endif
#ifndef NOEC
	else if(strcasecmp(module,"ecp") == 0){
                /* The mtsp module is concerned. */
                if(ecp_trigger(trigger) < 0){
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
		if(sound_trigger(trigger) < 0){
			println("Failed to execute trigger for snd!",
				ERROR);

			return -4;
		}
		return 0;
	}
#endif
        println("UNKNOWN MODULE %s!!",ERROR, module);
	return -5;

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
	
	else if(strcasecmp(module_name,"snd") == 0){
		/* Question the core module */
		return snd_init_dependency(dependency);
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
