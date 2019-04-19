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


struct module_t modules[] = {
#ifndef NOHINTS
	{NULL, init_hints, start_hints, NULL, NULL, reset_hints, true, false, 
		"hint"},
#endif /* NOHINTS */

#ifndef NOEC
	{ecp_init_dependency, init_ecp, start_ecp, ecp_check_dependency, 
		ecp_trigger, reset_ecp, true, false, "ecp"},
#endif /* NOEC */

#ifndef NOMTSP
	{mtsp_init_dependency, init_mtsp, start_mtsp, mtsp_check_dependency, 
		mtsp_trigger, reset_mtsp, true, false, "mtsp"},
#endif /* NOMTSP */

#ifndef NOSOUND
	{NULL, init_sound, NULL, NULL, sound_trigger, NULL, true, false, 
		"snd"},
#endif /* NOSOUND */

#ifndef NOMODBUS
	{NULL, init_modbus, NULL, NULL, modbus_trigger, NULL, true, false, 
		"modbus"},
#endif /* NOMODBUS */

#ifndef NOVIDEO
	{NULL, init_video, NULL, NULL, video_trigger, NULL, true, false, 
		"video"},
#endif /* NOVIDEO */

#ifndef HEADLESS
	{NULL, init_interface, start_interface, NULL, NULL, NULL, true, false, 
		"interface"},
#endif /* HEADLESS */

	{core_init_dependency, init_core, start_core, core_check_dependency, 
		core_trigger, NULL /* Does it itself! */, true, false, "core"}

};

size_t module_count = (sizeof(modules) /  sizeof(struct module_t));

/*
 * FUNCTIONS TO LOAD AND INITIALIZE MODULES
 *
 * All modules may be initialized, stated, and reset in here. Please add your
 * module to the desired functions and add a #IFNDEF entry to be able to disable
 * it in case there is would be a reason to do so.
 *
 */

/*
 * Called on program load. Gives the modules a chance to initialize internal
 * data strutures before needing to access them on startup. If one module failes
 * to init, < 0 is returned and a error message is printed out. On success >= 0
 * is returned. Implementation of module-dependencies is a TODO
 */
int init_modules(){

	println("STEP 1/3: Attempting module initialization", DEBUG);

	println("Getting needed modules...", DEBUG_MORE);

	json_object* needed_modules = json_object_object_get(config_glob,
		"modules");

	if(needed_modules == NULL){

		/* User didn't specify the modules field */		
		println("No mmodules string has been specified, loading modules"
			" by default configuration! This is discouraged", 
			WARNING);

	}else{
		if(json_object_get_type(needed_modules) != json_type_array){
			println("modules field is not an array!!", ERROR);
			return -1;
		}
		/* Disable all modules*/
		for(size_t i = 0; i < module_count; i++){
			struct module_t* mod = &modules[i];
		
			mod->enabled = false;
			
		}
		println("Enableing a total of %i modules...", DEBUG, 
			json_object_array_length(needed_modules));
		
		/* Enable only those needed */
		for(size_t i = 0; i < json_object_array_length(needed_modules);
			i++){
			const char* mod_name = json_object_get_string(
				json_object_array_get_idx(needed_modules, i));
			
			struct module_t* module = get_module_by_name(mod_name);
			if(module == NULL){
				if(mod_name == NULL){
					println("Module name resolved to 0 in "
						"module array!", ERROR);
					return -1;
				}else{
					println("Unknown module specified in "
						"array  [%s]!", ERROR, 
						mod_name);
					return -1;
				}
			}else{
				module->enabled = true;
			}
		}
	}
	println("Enabled modules:", DEBUG);
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}
		println("\t%i/%s", DEBUG, i, mod->name);

	}

	
	/* CONFIGURATION SANITY CHECKS */
	struct module_t* core_mod = get_module_by_name("core");
	if(core_mod != NULL){
		if(!core_mod->enabled){
			println("Core module is NOT enabled!!!", WARNING);
			println("This is probably NOT what you want!!",
				WARNING);
		}
	}


	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->init_module != NULL){
			println("Initializing module %i/%i: %s", DEBUG,
				i+1, module_count, mod->name);
			if(((*mod->init_module)()) < 0){
				println("Failed to init module %s!! Aborting!",
					ERROR, mod->name);
				return -1;
			}
		}

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
	
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->start_module != NULL){
			println("Starting module %i/%i: %s", DEBUG,
				i+1, module_count, mod->name);
			if(((*mod->start_module)()) < 0){
				println("Failed to start module %s!! Aborting!",
					ERROR, mod->name);
				mod->running = false;
				return -1;
			}else{
				mod->running = true;

			}
		}

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
		if(check_dependency(dependency_list[i], NULL) < 0){
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
	
	free(trigs);

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
	
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->reset_module != NULL){
			println("Resetting module %i/%i: %s", DEBUG,
				i+1, module_count, mod->name);
			if(((*mod->reset_module)()) < 0){
				println("Failed to reset module %s!!",
					ERROR, mod->name);
				return -1;
			}
		}

	}
	return 0;
}



/* Checks wether a dependency is met
 * Returns < 0 on error, 0 if not, and > 0 if forfilled.
 * 
 * returns return value of the specific module
 */

int check_dependency(json_object* dependency, float* percentage){

	const char* module_name = json_object_get_string(
				json_object_object_get(dependency,"module"));

	float percent_ret;

	int ret = 0;

	if(module_name == NULL){
		println("Specified no module in dependency! Misconfiguration!\
Attempting dump:",
			ERROR);
		if(dependency != NULL){
			json_object_to_fd(STDOUT_FILENO, dependency,
				JSON_C_TO_STRING_PRETTY);

		}else{
			println("Failed to dump root object! Is NULL!!", ERROR);
		}
		percent_ret = -1;
		ret = -1;
	}
	
	bool found = false;
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->check_dependency != NULL){
			if(strcasecmp(module_name,mod->name) == 0){
				found = true;
				ret = ((*mod->check_dependency)(dependency, 
					&percent_ret));

				break;
			}
		}

	}

	if(!found){
		println("Unknown module specified [%s]!", ERROR, module_name);
		ret = -2;
		percent_ret = ret;
	}
		
	if(dependency != NULL && ret < 0){
		println("Unable to check dependency! Dumping root:", ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
	}

	if(percentage != NULL){
		*percentage =  percent_ret;
	}
	
	return ret;
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
	
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->execute_trigger != NULL){
			if(strcasecmp(module,mod->name) == 0){
				if(((*mod->execute_trigger)(trigger, dry)) < 0){
					
					println("Failed to execute trigger for "
						"module %s!", ERROR, mod->name);
					return -1;
				}else{
					return 0;
				}
					
			}
		}

	}


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
	
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(!mod->enabled){
			continue;
		}

		if(mod->init_dependency != NULL){
			if(strcasecmp(module_name,mod->name) == 0){
				return (*mod->init_dependency)(dependency);
					
			}
		}

	}
		
	println("Unknown module specified in dependency [%s]!", ERROR, 
		module_name);

	return -1;
	
}

/* ############################################################################
 * # HELPER FUNCTIONS
 * ############################################################################
 */

struct module_t* get_module_by_name(const char* name){
	
	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		
		if(mod->name != NULL && strcasecmp(name,mod->name) == 0){
			return mod;
		}

	}
	
	return NULL;

}
