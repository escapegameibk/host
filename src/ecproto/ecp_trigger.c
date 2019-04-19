/* An EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains **ONLY** objects and other global variables needed to the
 * operation of the program
 *
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

#ifndef NOEC

#include "ecproto.h"
#include "log.h"
#include "serial.h"
#include "config.h"
#include "tools.h"
#include "game.h"
#include "data.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <poll.h>
#include <math.h>

int ecp_trigger(json_object* trigger, bool dry){
	json_object* type = json_object_object_get(trigger, "type");
	const char* type_name;
	/* By default a port trigger is assumed */
	if(type == NULL){
		type_name = "port";
	}else{
		type_name = json_object_get_string(type);
	}

	if(strcasecmp(type_name, "port") == 0){
		return ecp_trigger_port(trigger, dry);
	}else if(strcasecmp(type_name, "secondary_trans") == 0){
		/* Send a specified string */
		return ecp_trigger_secondary_trans(trigger, dry);

	}else if(strcasecmp(type_name, "pwm") == 0){
		/* Set a PWM value */
		return ecp_trigger_pwm(trigger, dry);

	}else{
		println("Failed to execute ECP trigger with invalid type! Dump:"
			, ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	return 0;
}

int ecp_trigger_secondary_trans(json_object* trigger, bool dry){

	json_object* str = json_object_object_get(trigger, "string");
	if(str == NULL){

		println("ECP secondary trigger without string! Dumping root:",
			ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);

		return -1;

	}

	const char* strin = json_object_get_string(str);

	json_object* dev = json_object_object_get(trigger, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	size_t device_id = json_object_get_int(dev);
	if(dry){
		return 0;
	}

	return send_ecp_secondary(device_id, (char*) strin);
}

/* Execute a port trigger. Set a GPIO pin to the desired value */
int ecp_trigger_port(json_object* trigger, bool dry){

	json_object* dev = json_object_object_get(trigger, "device");
	if(dev == NULL){
		println("Device not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* reg = json_object_object_get(trigger, "register");
	if(reg == NULL){
		println("Register not defined in ECP trigger! Dumping root: "
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	/* Theoretically it should be a string, but i don't care and i just take
	 * the first element. No, but seriously, jsons can't save chars, so
	 * there is nothing that i could do.
	 */
	char reg_id = json_object_get_string(reg)[0];

	json_object* bit = json_object_object_get(trigger, "bit");
	if(bit == NULL){
		println("Bit not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -3;
	}

	size_t bit_id = json_object_get_int(bit);

	bool target = true;
	json_object* trgt = json_object_object_get(trigger, "target");
	if(trgt != NULL){
		target = json_object_get_boolean(trgt);
	}
	if(dry){
		/* Don't really do anything on a dry run */
		return 0;
	}

	return send_ecp_port(device_id, reg_id, bit_id, target);

}

int ecp_trigger_pwm(json_object* trigger, bool dry){

	json_object* dev = json_object_object_get(trigger, "device");
	if(dev == NULL){
		println("Device not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* counter = json_object_object_get(trigger, "counter");
	if(counter == NULL){
		println("Counter not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t counter_id = json_object_get_int(counter);

	json_object* output = json_object_object_get(trigger, "output");
	if(output == NULL){
		println("Output not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t output_id = json_object_get_int(output);

	json_object* value = json_object_object_get(trigger, "value");
	if(counter == NULL){
		println("Value not defined in ECP trigger! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t val = json_object_get_int(value);

	uint8_t payload[] = {counter_id, output_id, val};

	if(!dry){
		return ecp_send_message(device_id, SET_PWM, payload,
			sizeof(payload));
	}else{
		return 0;
	}
}

#endif /* NOEC */
