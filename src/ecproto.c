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

#include <json-c/json.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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
	
	return ecp_register_input_pin(device_id, reg_id, bit_id, pulled_high);

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
		println("Bit number too high! Dumping root:", 
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency, 
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	struct ecproto_port_t* pn = reg_p->bits[bit];
	pn->ddir = ECP_INPUT;

	/* Enable pullup */
	pn->target = true;

	return 0;
}

int init_ecp(){	

	ecp_fd = open(ECP_DEF_DEV, O_RDWR);
	return 0;
}

/* Returns < 0 on error and 0 < return < 256 on success 
 * Attempts to validate the frame length*/
int get_frame_length(uint8_t* frame ){

	if(frame == NULL){
		
	}

	return 0;
}

uint8_t* recv_frame(int fd ){
	uint8_t * frame = NULL;
	size_t frame_length = 0;

	while(frame_length < 255){
		uint8_t octet;
		if(read(fd, &octet, 1) == 0){
			return NULL;
		}

		frame = realloc(frame, ++frame_length);
		frame[frame_length - 1] = octet;
		if(octet == 0xFF){
			return frame;
		}
	}

	return frame;

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
