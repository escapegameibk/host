/* A EC-Proto implementation for the escape game innsbruck's host
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

#ifndef NOEC

#include "ecproto.h"
#include "log.h"
#include "serial.h"
#include "config.h"
#include "tools.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

size_t ecp_devcnt = 0;
struct ecproto_device_t *ecp_devs = NULL;

int ecp_init_dependency(json_object* dependency){

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
	
	json_object* pulled = json_object_object_get(dependency, "pulled");
	bool pulled_high = true;
	if(pulled != NULL){
		pulled_high = json_object_get_boolean(pulled);
	}
	
	int ret = ecp_register_input_pin(device_id, reg_id, bit_id, 
		pulled_high);
	if(ret < 0){
		println("Failed to register ecp pin! Dumping root: ", ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency, 
			JSON_C_TO_STRING_PRETTY);
		return -4;
	}
	return ret;
}

int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, bool 
	pulled){
	
	struct ecproto_device_t*  device= escp_get_dev_from_id(device_id);
	if(device == NULL){
		/* Add the device to the array */
		ecp_devs = realloc(ecp_devs, ++ecp_devcnt * 
			sizeof(struct ecproto_device_t));
		device = &ecp_devs[ecp_devcnt - 1];
		device->id = device_id;
		device->regcnt = 0;
		device->regs = NULL;
	}
	
	struct ecproto_port_register_t*  reg_p = 
		escp_get_reg_from_dev(reg_id, device);

	if(reg_p == NULL){
		/* Add it to the array */
		device->regs = realloc(device->regs, ++device->regcnt * 
			sizeof(struct ecproto_port_register_t));
		
		reg_p =  &device->regs[device->regcnt - 1];
		reg_p->id = reg_id;
		init_ecp_port_reg(reg_p);
		
	}
	
	if(bit >= ECP_REG_WIDTH){
		println("Bit number too high for ecp dependency!", 
			ERROR);
		return -1;
	}

	struct ecproto_port_t* pn = &reg_p->bits[bit];
	pn->ddir = ECP_INPUT;

	/* Set pullup */
	pn->target = pulled;

	return 0;
}

int init_ecp(){	

	const char* device = ECP_DEF_DEV;

	json_object* dev =  json_object_object_get(config_glob, "ecp_device");
	if(dev != NULL){
		device = json_object_get_string(dev);
	}

	ecp_fd = open(device, O_RDWR);
	if(ecp_fd < 0){
		println("Failed to open ECP device at %s!", ERROR, device);
		return -1;
	}

	json_object* baud = json_object_object_get(config_glob, "ecp_baud");
	int32_t baud_rate_rel = json_object_get_int(baud);

	int baud_rate = get_baud_from_int(baud_rate_rel);
	if(baud_rate == 0){
		return -2;
	}

	if(set_interface_attribs(ecp_fd, baud_rate) < 0){
		println("Failed to set atributes to mtsp serial interface!",
			ERROR);
		return 3;
	}
	/* Should now be ready. Serial init is performed on startup */

	return 0;
}

int start_ecp(){


	return 0;
}

uint8_t* recv_frame(int fd, size_t* len){
	
	*len = 0;

	uint8_t * frame = malloc(sizeof(uint8_t));
	ssize_t n = read(fd, &frame[0], sizeof(uint8_t));
	
	if(n < 0){
		println("Failed to read from ecp devicep Device dead?", ERROR);
		free(frame);
		return NULL;
	}else if(n == 0){
		println("ECP Device didnt respond! possibly timeout.", WARNING);
		return NULL;
	}
	/* Okay,now we should have received a valid length */
	ssize_t frame_length = 1;
	/* This is not an infinity loop, but it is ended in different ways */
	while(true){
		uint8_t octet;

		if(read(fd, &octet, 1) == 0){
			return NULL;
		}

		frame = realloc(frame, ++frame_length);
		frame[frame_length - 1] = octet;
		if((octet == 0xFF) && (frame_length >= (frame[0] - 1))){
			*len = frame_length;
			return frame;
		}
	}

	return NULL;

}

uint16_t ibm_crc(uint8_t* data, size_t len){

	uint16_t crc = 0xFFFF;

	for(size_t pos = 0; pos < len; pos++){
		crc ^= (uint16_t)data[pos];

		for(size_t i = 8; i != 0; i--){
			if((crc & 0x0001) != 0){
				crc >>= 1;
				crc ^= 0xA001;
			}else{
				crc >>= 1;
			}
		}
	}

	return crc;
}

int pasrse_input(uint8_t* frm){

	switch(frm[1]){
		case 8:
			printf("%s\n", &frm[2]);
			break;
		default:

			break;
	}
	return 0;
}


bool validate_crc(uint8_t* frame){


	uint16_t crc_should = ibm_crc(frame, frame[0] - 3);
	uint16_t crc_is = ((frame[frame[0] - 3]) << 8) | ((frame[frame[0] - 2]));

	return crc_is == crc_should;
}

int write_ecp_msg(int fd, uint8_t action_id, uint8_t* payload, 
	size_t payload_length){

	uint8_t frame[255];

	frame[0] = ECPROTO_OVERHEAD + payload_length;
	frame[1] = action_id;
	memcpy(&frame[2], payload, payload_length);
	uint16_t crc = ibm_crc(frame,frame[0] - 3);
	frame[frame[0] - 3] = (crc >> 8) & 0xFF;
	frame[frame[0] - 2] = crc & 0xFF;
	frame[frame[0] - 1] = 0xFF;

	if(!validate_crc(frame)){
		return -1;
	}
	return write(fd, frame, frame[0]);
}

/*
 *	HELPER FUNCTIONS
 */

struct ecproto_device_t* escp_get_dev_from_id(size_t id){
	for(size_t i = 0; i < ecp_devcnt; i++){
		if(ecp_devs[i].id == id){
			/* Found */
			return &ecp_devs[i];
		}
	}
	/* Not found */
	return NULL;
}

struct ecproto_port_register_t* escp_get_reg_from_dev(char id, struct 
	ecproto_device_t* dev){
	
	for(size_t i = 0; i < dev->regcnt; i++){
		if(dev->regs[i].id == id){
			/* Found */
			return &dev->regs[i];
		}
	}
	/* Not found */
	return NULL;
}
/* Initialize the ecproto_port_register_t structure with the default values. */
int init_ecp_port_reg(struct ecproto_port_register_t* prt_reg){
	
	for(uint8_t i = 0; i < ECP_REG_WIDTH; i++){
		struct ecproto_port_t* prt = &prt_reg->bits[i];
		/* This should do the same thing as the µc on the other side
		 * has done */
		prt->bit = i;
		prt->ddir = ECP_OUTPUT;
		prt->target = false;
		/* This will be overwritten by a check at the beginning */
		prt->current = false;
	}

	return 0;
}

#endif /* NOEC */
