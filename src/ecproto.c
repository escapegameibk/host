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

bool ecp_fd_locked = false;

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
	
	ecp_register_register(device_id, reg_id);
	
	struct ecproto_device_t* device =  escp_get_dev_from_id(device_id);
	
	struct ecproto_port_register_t*  reg_p = 
		escp_get_reg_from_dev(reg_id, device);
	
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

/* This function guarantees a register to be registered */
int ecp_register_register(size_t device_id, char reg_id){

	
	ecp_register_device(device_id);

	struct ecproto_device_t*  device = escp_get_dev_from_id(device_id);

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

	return 0;
}

int ecp_register_device(size_t id){
	
	struct ecproto_device_t*  device = escp_get_dev_from_id(id);
	
	if(device == NULL){
		/* Add the device to the array */
		ecp_devs = realloc(ecp_devs, ++ecp_devcnt * 
			sizeof(struct ecproto_device_t));
		device = &ecp_devs[ecp_devcnt - 1];
		device->id = id;
		device->regcnt = 0;
		device->regs = NULL;
	}
	
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

	sleep(2);

	/* Set the timeout for the mtsp devices to reply*/
        struct termios termios;
        tcgetattr(ecp_fd, &termios);
        termios.c_lflag &= ~ICANON; /* Set non-canonical mode */
        termios.c_cc[VTIME] = 1; /* Set timeout of 0.1 seconds */
        termios.c_cc[VMIN] = 0; /* Set timeout of 0.1 seconds */
        tcsetattr(ecp_fd, TCSANOW, &termios);

	int baud_rate = B38400;
	
	json_object* baud = json_object_object_get(config_glob, "ecp_baud");
	if(baud != NULL){

		int32_t baud_rate_rel = json_object_get_int(baud);

		baud_rate = get_baud_from_int(baud_rate_rel);
		if(baud_rate == 0){
			return -2;
		}

	}

	if(set_interface_attribs(ecp_fd, baud_rate) < 0){
		println("Failed to set atributes to mtsp serial interface!",
			ERROR);
		return -3;
	}

	/* Should now be ready. */
	if(ecp_bus_init()){
		println("Failed to init ECP bus!!!", ERROR);
		return -4;
	}
	println("ECP device map:", DEBUG);
	/* Debug output all devices and registers */
	for(size_t dev = 0; dev < ecp_devcnt; dev++){
		struct ecproto_device_t* device = &ecp_devs[dev];
		
		println("%i : %i : ", DEBUG, dev, device->id, device->regcnt);
		for(size_t reg = 0; reg < device->regcnt; reg++){
			println("\t%i : %c", DEBUG, reg, device->regs[reg].id);
		}
	}

	println("ECP init done!", DEBUG);
	
	return 0;
}

int start_ecp(){


	return 0;
}

uint8_t* recv_ecp_frame(int fd, size_t* len){
	
	while(ecp_fd_locked){ /* NOP */}
	ecp_fd_locked = true;

	uint8_t * frame = NULL;
	size_t frame_length = 0;

	while(frame_length < 255){
		
		uint8_t octet;
		if(read(fd, &octet, sizeof(uint8_t)) == 0){
			free(frame);
			println("ECp frame receive timed out!", WARNING);
			ecp_fd_locked = false;
			return NULL;
		}
	
		frame = realloc(frame, ++frame_length);
		frame[frame_length - 1] = octet;

		if(octet == 0xFF && frame_length + 1 >= frame[0]){
			ecp_fd_locked = false;
			*len = frame_length;
			return frame;
		}
	
	}
	
	ecp_fd_locked = false;
	free(frame);
	return NULL;
}


bool validate_ecp_frame(uint8_t* frame, size_t len){

	if(frame[len - 1] != ECP_CMD_DEL){
		/* No end char is present at the end! */
		return false;
	}
	
	uint16_t crc_should = ibm_crc(frame, (len - sizeof(uint8_t) * 3));
	uint16_t crc_is = ((frame[len - 3]) << 8) | ((frame[len - 2]));	
	if(crc_is != crc_should){
		println("ECP crc missmatch!", WARNING);
		println("SHOULD BE: %i is %i", WARNING, crc_should, crc_is);
		return false;
	}
	/* The frame should be valid */
	return true;
}

int parse_ecp_input(uint8_t* recv_frm, size_t recv_len, uint8_t* snd_frm, size_t 
	snd_len){

	if(!validate_ecp_frame(recv_frm, recv_len)){
		println("Received invalid ecp frame!", WARNING);
		return -1;
	}
	
	if(!validate_ecp_frame(snd_frm, snd_len)){
		println("Sent invalid ecp frame! WTF?", ERROR);
		println("Critical bug! This shouldn't occure.", ERROR);
		return -2;
	}

	size_t recv_payload_len = recv_len - ECPROTO_OVERHEAD;
	/* Switch by the receiving frame's id */
	switch(recv_frm[1]){
		case 8:
			if(recv_frm[recv_len - 4] != 0){
				println(
"ECP received error frame, which isn't null-terminated!"
					,ERROR);
				return -3;
			}
			/* Print error string */
			println("ECP action %i aborted due to error response:",
				ERROR);
			println("%s\n", ERROR, &recv_frm[2]);
			return -4;
			break;
		case 0:
			println("The ECP slave has requested an initialisation",
				DEBUG);
			if(ecp_bus_init() < 0){
				println("Failed to initialize ECP bus!", ERROR);
				return -5;
			}
			break;
		case 2:
			/* I will now get some more frames. Tell the overlying
			 * function that it should read some more frames */
			if(recv_payload_len >= 1){
				return recv_frm[2];
			}
			break;
		case 3:
			/* Register device. If payload is longer than 1 byte,
			 * connection is enumerated */
			
			if(recv_payload_len < 1){
				return -1;
			}
			ecp_register_device(recv_frm[2]);

			if(recv_payload_len < 2){
				return 1;
			}else{
				return 0;
			}

			break;

		case 4:
			/* TODO */
			return 0;
			break;
		case 5:
			/* The reply to a port definition. */
			if(recv_payload_len < 1 || !recv_frm[2]){
				return -6;
			}
			break;
		case 6:
			/* The reply to a port state get. */
			if(recv_payload_len < 1){
				return -7;
			}
			escp_get_reg_from_dev(snd_frm[2],
				escp_get_dev_from_id(0))->bits[snd_frm[3]]
				.current = recv_frm[2];
			

			break;
		case 10:
			/* All registers of a device are in the payload
			 */
			for(size_t i = 0; i < recv_payload_len; i++){
				ecp_register_register(0, recv_frm[2 + i]);
			}
			break;

		default:
			/* You lazy bastard! */
			println("Unimplemented ECP response: %i!", WARNING,
				recv_frm[1]);
			break;
	}
	return 0;
}

int ecp_bus_init(){
	/* Enumerate bus and initialize device structures */	
	uint8_t start_id = 0;
	int n = 0;
	n |= ecp_send_message(3, &start_id, sizeof(uint8_t));
	n |= ecp_send_message(10, NULL, 0);

	for(size_t dev = 0; dev < ecp_devcnt; dev++){
		struct ecproto_device_t* device = &ecp_devs[dev];
		
		println("%i : %i : ", DEBUG, dev, device->id, device->regcnt);
		for(size_t reg = 0; reg < device->regcnt; reg++){
			struct ecproto_port_register_t* regp = 
				&device->regs[reg];

			for(size_t bt = 0; bt < ECP_REG_WIDTH; bt++){
				struct ecproto_port_t* bit = &regp->bits[bt];

				n |= send_ecp_ddir(device->id, regp->id, 
					bit->bit, bit->ddir); 
				
				n |= send_ecp_port(device->id, regp->id, 
					bit->bit, bit->target); 
				
				n |= get_ecp_port(device->id, regp->id, 
					bit->bit); 
			}
		}
	}

	return 0;
}

/*
 * ECP SEND FUNCTIONS
 *
 * Functions in here are used to send stuff to a µc.
 */

int get_ecp_port(size_t device_id, char reg_id, size_t pin_id){

	if(device_id == 0){
		uint8_t msg[] = {reg_id, pin_id};
		return ecp_send_message(6, msg, sizeof(msg));
	}

	/* Else is a TODO */
	return 0;

}

/* If the specified pin is an output, this sets the pin's value, if the pin
 * is an input, this enables or disables the pullup resistor. */
int send_ecp_port(size_t device_id, char reg_id, size_t pin_id, bool port){

	if(device_id == 0){
		uint8_t msg[] = {reg_id, pin_id, port};
		return ecp_send_message(7, msg, sizeof(msg));
	}

	/* Else is a TODO */
	return 0;

}

int send_ecp_ddir(size_t device_id, char reg_id, size_t pin_id, bool ddir){

	if(device_id == 0){
		uint8_t msg[] = {reg_id, pin_id, ddir};
		return ecp_send_message(5, msg, sizeof(msg));

	}

	/* Else is a TODO */
	return 0;

}

int ecp_send_message(uint8_t action_id, uint8_t* payload, size_t payload_len){
	
	int ret = 0;

	uint8_t* snd_frame = NULL;
	size_t snd_len = 0;
	int n = write_ecp_msg(ecp_fd, action_id, payload, payload_len, 
		&snd_frame, &snd_len);

	if(n < 0){
		println("Failed to send ecp message!", ERROR);
		return -1;
	}

	size_t reads = 1;
	for(size_t i = 0; i < reads; i++){

		size_t recv_len = 0;

		uint8_t* recv_frame = recv_ecp_frame(ecp_fd, &recv_len);
			
		if(recv_frame == NULL){
			println("Failed to received ecp frame", WARNING);
			free(snd_frame);
			return -2;
		}

		n = parse_ecp_input(recv_frame, recv_len, snd_frame, snd_len);
		free(recv_frame);
		
		if( n < 0 ){
			println("Failed to parse received ecp frame", WARNING);
			free(snd_frame);
			return -3;
		}
		reads += n;
	}
	free(snd_frame);

	return ret;
}

int write_ecp_msg(int fd, uint8_t action_id, uint8_t* payload, 
	size_t payload_length, uint8_t** frme, size_t* frame_length){

	uint8_t frame[255];

	frame[0] = ECPROTO_OVERHEAD + payload_length;
	frame[1] = action_id;
	memcpy(&frame[2], payload, payload_length);
	uint16_t crc = ibm_crc(frame,frame[0] - 3);
	frame[frame[0] - 3] = (crc >> 8) & 0xFF;
	frame[frame[0] - 2] = crc & 0xFF;
	frame[frame[0] - 1] = 0xFF;
	
	if(!validate_ecp_frame(frame, frame[0])){
		println("Failed to contruct ecp frame!", ERROR);
		return -1;
	}

	if(frme != NULL){
		*frame_length = frame[0] * sizeof(uint8_t);
		*frme = malloc(frame[0] * sizeof(uint8_t));
		memcpy(*frme, frame, sizeof(uint8_t) * *frame_length);
	}
	
	while(ecp_fd_locked){ /* NOP */}
	ecp_fd_locked = true;

	int n = write(fd, frame, frame[0]);
	
	ecp_fd_locked = false;
	
	return n;
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
		 * has done 
		 */
		prt->bit = i;
		prt->ddir = ECP_OUTPUT;
		prt->target = false;
		/* This will be overwritten by a check at the beginning */
		prt->current = false;
	}

	return 0;
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

#endif /* NOEC */
