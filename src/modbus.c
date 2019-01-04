/* Freehand Modbus implementation for the escape game system
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
#ifndef NOMODBUS

#include "modbus.h"
#include "log.h"
#include "serial.h"
#include "config.h"
#include "tools.h"
#include "serial.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int init_modbus(){

	/* Open file descriptor, set options and check if succeeded */

	const char* device = MODBUS_DEFAULT_DEV;

	json_object* dev =  json_object_object_get(config_glob, "modbus_device");
	if(dev != NULL){
		device = json_object_get_string(dev);
	}

	modbus_fd = open(device, O_RDWR);
	if(modbus_fd < 0){
		println("Failed to open MODBUS device at %s!", ERROR, device);
		return -1;
	}

	/* Set the timeout for the mtsp devices to reply*/
        struct termios termios;
        tcgetattr(modbus_fd, &termios);
        termios.c_lflag &= ~ICANON; /* Set non-canonical mode */
        termios.c_cc[VTIME] = 1; /* Set timeout of 0.1 seconds */
        termios.c_cc[VMIN] = 0; /* Set timeout of 0.1 seconds */
        tcsetattr(modbus_fd, TCSANOW, &termios);

	int baud_rate = MODBUS_DEFAULT_BAUD;
	
	json_object* baud = json_object_object_get(config_glob, "modbus_baud");
	if(baud != NULL){

		int32_t baud_rate_rel = json_object_get_int(baud);

		baud_rate = get_baud_from_int(baud_rate_rel);
		if(baud_rate == 0){
			return -2;
		}

	}

	if(set_interface_attribs(modbus_fd, baud_rate) < 0){
		println("Failed to set atributes to modbus serial interface!",
			ERROR);
		return -3;
	}
	
	println("modbus module ready!", DEBUG);

	return 0;
}

int modbus_trigger(json_object* trigger, bool dry){

	json_object* device = json_object_object_get(trigger, "device");

	if(device == NULL){
		println("Modbus trigger withour device!! Dumping root:",
			ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);

		return -1;
	}

	uint8_t addr = json_object_get_int(device);

	json_object* port = json_object_object_get(trigger, "register");

	if(port == NULL){
		println("Modbus trigger withour register!! Dumping root:",
			ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);

		return -2;
	}

	uint16_t reg = json_object_get_int(port);
	
	json_object* val = json_object_object_get(trigger, "target");

	if(val == NULL){
		println("Modbus trigger without target!! Dumping root:",
			ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);

		return -2;
	}

	uint16_t target = 0;
	
	if(json_object_is_type(val, json_type_string)){
	
		/* In case it is a string, the value given should be matched
		 * to the appropriate integer values. Modbus  relays are a bit
		 * weired... 
		 */

		const char* sval = json_object_get_string(val);
		if(strcasecmp("ON",sval) == 0){
			target = MODBUS_RELAY_STATE_ON;
		}else if(strcasecmp("OFF",sval) == 0){
			target = MODBUS_RELAY_STATE_OFF;
		}else if(strcasecmp("KEEP",sval) == 0){
			target = MODBUS_RELAY_STATE_KEEP;
		}else{
			println("Modbus trigger with invalid target! Dumping:",
				ERROR);

			json_object_to_fd(STDOUT_FILENO, trigger, 
				JSON_C_TO_STRING_PRETTY);

			return -3;
			
		}
		
	}else{

		/* I assume it should be a number. Pass it through!*/
	
		target = json_object_get_int(val);

	}


	if(dry){
		return 0;
	}

	if(modbus_write_register(addr, reg, target) < 0){
		println("Modbus trigger failed! Dumping root:",
			ERROR);

		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);

		return -4;
	}
	
	return 0;

}

int modbus_write_register(uint8_t addr, uint16_t reg, uint16_t target){

	uint8_t frame[MODBUS_ACTION_SET_REGISTER_LENGTH];

	frame[0] = addr;
	frame[1] = MODBUS_ACTION_SET_REGISTER;
	frame[2] = (reg >> 8) & 0xff;
	frame[3] = (reg) & 0xff;
	frame[4] = (target) & 0xff;
	frame[5] = (target >> 8) & 0xff;
	/* Calculate CRC sum */
	uint16_t crc = ibm_crc(frame, sizeof(frame) - 2);
	frame[6] = (crc) & 0xff;
	frame[7] = (crc >> 8) & 0xff;

	return modbus_write_frame(frame, sizeof(frame), 
		MODBUS_ACTION_SET_REGISTER_LENGTH);
	
}

int modbus_write_frame(uint8_t* frame, size_t length, size_t response_len){

	char* printable = printable_bytes(frame, length);
	println(printable, ERROR);
	free(printable);
	
	int rd = write(modbus_fd, frame, length);

	if(rd <= 0){
		println("Failed to WRITE to modbus!", ERROR);
		return -1;
	}

	uint8_t received[response_len];
	size_t cursor = 0;
	
	for(size_t i = 0; i < response_len; i++){


		rd = read(modbus_fd, &received[cursor], sizeof(received) - 
			cursor);

		if(rd < 0){
			println("Failed to read on modbus FD!", ERROR);
			return -2;
		}else if(rd == 0){
			/* Ran into a timeout */
			break;

		}else{
			cursor += rd;
			if(cursor == response_len){
				break;
			}
		}

	}



	if(cursor < response_len){
		println("Failed to read modbus frame! Too short! %i/%i", ERROR,
			cursor, response_len);

		char* printable = printable_bytes(received, cursor);
		println(printable, ERROR);
		free(printable);

		return -3;
	}

	/* Validate CRC sum */
	uint16_t crc = ibm_crc(received, response_len - 2);

	if(	((crc) & 0xff) == received[response_len - 2] &&
		((crc >> 8) & 0xff) == received[response_len - 1]){
		
		/* For validation we now have to sleep a bit */
		struct timespec tim;
		tim.tv_sec = 0;
		tim.tv_nsec = 10000000;
		if(nanosleep(&tim , NULL) < 0 ){
			println("Delay failed in modbus!", ERROR);
		}
		return 0;

	}else{
		println("CRC sum check failed for modbus!", ERROR);
		return -4;
	}


	return 0;

}

#endif /* NOMODBUS */
