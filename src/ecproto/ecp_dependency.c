/* An EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains functions related to dependency handleing 
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

int ecp_check_dependency(json_object* dependency, float* percentage){

	pthread_mutex_lock(&ecp_state_lock);

	json_object* type = json_object_object_get(dependency, "type");
	const char* type_name ;
	if(type == NULL){
		type_name = "port";
	}else{
		type_name = json_object_get_string(type);
	}
	float ret = 0;

	if(strcasecmp(type_name, "port") == 0){

		ret = ecp_check_port_dependency(dependency);
	}else if(strcasecmp(type_name, "analog") == 0){

		ret = ecp_check_analog_dependency(dependency);
	}else if(strcasecmp(type_name, "adc") == 0){

		ret = ecp_check_new_analog_dependency(dependency);

	}else if(strcasecmp(type_name, "mfrc522") == 0){

		ret = ecp_check_mfrc522_dependency(dependency);

	}else{
		println("Invalid type in ECP dependecy! Duming root:"
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		ret = -1;
	}
	if(ret == NAN || ret == INFINITY || ret == -INFINITY){

		println("Core dependency return value would be not \
representable as integer: %f",
		ret);
		ret = -1;
	}

	if(percentage != NULL){
		*percentage = ret;
	}
	pthread_mutex_unlock(&ecp_state_lock);
	return floor(ret);

}

int ecp_check_port_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* reg = json_object_object_get(dependency, "register");
	if(reg == NULL){
		println("Register not defined in ECP dependency! Dumping root: "
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	/* Theoretically it should be a string, but i don't care and i just take
	 * the first element. No, but seriously, jsons can't save chars, so
	 * there is nothing that i could do.
	 */
	char reg_id = json_object_get_string(reg)[0];

	json_object* bit = json_object_object_get(dependency, "bit");
	if(bit == NULL){
		println("Bit not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -3;
	}

	size_t bit_id = json_object_get_int(bit);

	bool target = true;
	json_object* trgt = json_object_object_get(dependency, "target");
	if(trgt != NULL){
		target = json_object_get_boolean(trgt);
	}

	struct ecproto_device_t* devp = ecp_get_dev_from_id(device_id);
	if(devp == NULL){
		println("Uninitialized dependency! This shouldn't happen!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);

		return -4;
	}

	struct ecproto_port_register_t* regp =
		ecp_get_reg_from_dev(reg_id, devp);
	if(regp == NULL){
		println("Uninitialized dependency! This shouldn't happen!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);

		return -5;
	}

	if(bit_id >= ECP_REG_WIDTH){
		println("Wrongly inited dependency! This shouldn't happen!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -6;
	}

	return regp->bits[bit_id].current == target;
}

float ecp_check_analog_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* val = json_object_object_get(dependency, "value");
	if(val == NULL){
		println("Value not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	uint8_t value = json_object_get_int(val);

	json_object* threshold = json_object_object_get(dependency,"threshold");
	const char* th;

	if(threshold == NULL){
		th = "above";
	}else{
		th = json_object_get_string(threshold);
	}

	if(strcasecmp(th, "above") == 0){
		if(value < ecp_get_dev_from_id(device_id)->analog->value){
			return true;
		}else{
			return ecp_get_dev_from_id(device_id)->analog->value /
			(float)value;
		}
	}else if(strcasecmp(th, "below") == 0){
		if(value > ecp_get_dev_from_id(device_id)->analog->value){
			return true;
		}else{
			return (255 -
			ecp_get_dev_from_id(device_id)->analog->value) /
			(255 - (float)value);
		}
	}else if(strcasecmp(th, "is") == 0){
		return value == ecp_get_dev_from_id(device_id)->analog->value;
	}
	else{
		println("Unknown ECP analog threshold specified! Dumping root:",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -3;
	}

	return 0;
}

float ecp_check_mfrc522_dependency(json_object* dependency){

	json_object* dev_o = json_object_object_get(dependency, "device");
	if(dev_o == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev_o);

	struct ecproto_device_t* dev = ecp_get_dev_from_id(device_id);

	if(dev == NULL){
		println("ECP MFRC522 dependency with unknown device %i! Dumping:",
			ERROR, device_id);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	json_object* tag_o = json_object_object_get(dependency, "tag");
	if(tag_o == NULL){
		println("tag not defined in ECP mfrc522dependency! "
			"Dumping root: ", ERROR);

		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	uint8_t tag = (uint32_t)json_object_get_int64(tag_o);

	if(tag >= dev->mfrc522_devcnt){
		println("mfrc522 tag no. bigger than device advertised!"
			"Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);

	}

	struct ecproto_mfrc522_dev_t* mfrc522 = &dev->mfrc522_devs[tag];

	json_object* val = json_object_object_get(dependency, "tag_id");
	if(val == NULL){
		println("tag_id not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	uint32_t id = (uint32_t)json_object_get_int64(val);

	if(!mfrc522->tag_present || !mfrc522->working){
		return 0;
	}

	if(mfrc522->tag == id){
		return 1;
	}else{
		return 0.5;
	}


	return 0;
}

float ecp_check_new_analog_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);
	

	struct ecproto_device_t * device = ecp_get_dev_from_id(device_id);

	if(device == NULL){
		println("BUG: UNREGISTERED DEVICE IN DEPENDENCY CHECK!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	json_object* val = json_object_object_get(dependency, "value");
	if(val == NULL){
		println("Value not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	uint8_t value = json_object_get_int(val);
	
	json_object* chan = json_object_object_get(dependency, "channel");
	if(chan == NULL){
		println("channel not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	uint8_t channel_id = json_object_get_int(chan);

	struct ecproto_analog_channel_t* channel = NULL;
	for(size_t i = 0; i < device->analog_channel_count; i++){
		if(device->analog_channels[i].id == channel_id){
			channel = &device->analog_channels[i];
		}
	}

	if(channel == NULL){
		println("BUG: UNREGISTERED CHANNEL IN DEPENDENCY CHECK!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}


	json_object* threshold = json_object_object_get(dependency,"threshold");
	const char* th;

	if(threshold == NULL){
		th = "above";
	}else{
		th = json_object_get_string(threshold);
	}

	if(strcasecmp(th, "above") == 0){
		if(value < channel->value){
			return true;
		}else{
			return channel->value /
			(float)value;
		}
	}else if(strcasecmp(th, "below") == 0){
		if(value > channel->value){
			return true;
		}else{
			return (255 -
			channel->value) /
			(255 - (float)value);
		}
	}else if(strcasecmp(th, "is") == 0){
		return value == channel->value;
	}
	else{
		println("Unknown ECP analog threshold specified! Dumping root:",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -3;
	}

	return 0;
}

int ecp_init_dependency(json_object* dependency){

	json_object* type = json_object_object_get(dependency, "type");
	const char* type_name ;
	if(type == NULL){
		type_name = "port";
	}else{
		type_name = json_object_get_string(type);
	}

	if(strcasecmp(type_name, "port") == 0){

		return ecp_init_port_dependency(dependency);
	}else if(strcasecmp(type_name, "analog") == 0){
		return ecp_init_analog_dependency(dependency);
	}else if(strcasecmp(type_name, "adc") == 0){
		return ecp_init_new_analog_dependency(dependency);

	}else if(strcasecmp(type_name, "mfrc522") == 0){
		/* Nothing to do here. */
	}else{
		println("Invalid type in ECP dependecy! Duming root:"
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	return 0;

}

int ecp_init_port_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}


	size_t device_id = json_object_get_int(dev);

	json_object* reg = json_object_object_get(dependency, "register");
	if(reg == NULL){
		println("Register not defined in ECP dependency! Dumping root: "
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	/* Theoretically it should be a string, but i don't care and i just take
	 * the first element. No, but seriously, jsons can't save chars, so
	 * there is nothing that i could do.
	 */
	char reg_id = json_object_get_string(reg)[0];

	json_object* bit = json_object_object_get(dependency, "bit");
	if(bit == NULL){
		println("Bit not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -3;
	}

	size_t bit_id = json_object_get_int(bit);

	/* Is the bit puled high or low. In case isinput is set below, this
	 * also determines the initial state of a pin.
	 */
	json_object* pulled = json_object_object_get(dependency, "pulled");
	bool pulled_high = true;
	if(pulled != NULL){
		pulled_high = json_object_get_boolean(pulled);
	}

	/* Wether the dependency pin is an input or not. This enabled outputs
	 * to be used as dependencies.
	 */
	json_object* isin = json_object_object_get(dependency, "isinput");
	bool is_input = true;
	if(isin != NULL){
		is_input = json_object_get_boolean(isin);
	}

	int ret = ecp_register_input_pin(device_id, reg_id, bit_id,
		pulled_high, is_input);

	if(ret < 0){
		println("Failed to register ecp pin! Dumping root: ", ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -4;
	}
	return ret;

}

/* DEPRECATED */
int ecp_init_analog_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	if(ecp_register_device(device_id) < 0){
		println("Failed to initialize ECP device! Dumping root:",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -2;
	}

	ecp_get_dev_from_id(device_id)->analog->used = true;
	println("Enabling ananlog pin on ECP dev id %i", DEBUG, device_id);

	return 0;
}

/* New analog subsystem */
int ecp_init_new_analog_dependency(json_object* dependency){

	json_object* dev = json_object_object_get(dependency, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* chan = json_object_object_get(dependency, "channel");
	if(chan == NULL){
		println("Channel not defined in ECP dependency! Dumping root: ",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t channel_id = json_object_get_int(chan);
	
	if(ecp_register_device(device_id) < 0){
		println("Failed to register device for ECP analog dep! dumping:"
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency,
			JSON_C_TO_STRING_PRETTY);
		return -1;

	}
	struct ecproto_device_t* device = ecp_get_dev_from_id(device_id);
	
	struct ecproto_analog_channel_t* channel = NULL;

	for(size_t i = 0; i < device->analog_channel_count; i++){
		if(device->analog_channels[i].id == channel_id){
			channel = &device->analog_channels[i];
		}
	}

	if(channel == NULL){
		device->analog_channels = realloc(device->analog_channels, 
			sizeof(struct ecproto_analog_channel_t) * 
			++device->analog_channel_count);
		channel = &device->analog_channels[
			device->analog_channel_count - 1];
		
		channel->id = channel_id;
		channel->value = 0;

	}

	println("Enabling ananlog channel %i on ECP dev %i", DEBUG, channel_id,
		device_id);

	return 0;
}


#endif /* NOEC */
