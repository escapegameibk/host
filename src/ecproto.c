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

size_t ecp_devcnt = 0;
struct ecproto_device_t *ecp_devs = NULL;

bool ecp_fd_locked = false;

bool ecp_initialized = false;

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

	escp_get_dev_from_id(device_id)->analog->used = true;
	println("Enabling ananlog pin on ECP dev id %i", DEBUG, device_id);

	return 0;
}
/* Registers an input pin, there are input pins which are not really inputs,
 * the is_input flag sets the ddir register accordingly */
int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, bool 
	pulled, bool is_input){
	
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
	if(is_input){
		pn->ddir = ECP_INPUT;
	}else{
		pn->ddir = ECP_OUTPUT;

	}
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
		device->analog = malloc(sizeof(struct ecproto_analog_t));
		device->analog->used = false;
		device->analog->value = 0;
	}
	
	return 0;
}

int init_ecp(){	
	pthread_mutex_init(&ecp_lock, NULL);
	pthread_mutex_init(&ecp_readlock, NULL);
	

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
	
	println("Initial ECP device map:", DEBUG);
	/* Debug output all devices and registers */
	for(size_t dev = 0; dev < ecp_devcnt; dev++){
		struct ecproto_device_t* device = &ecp_devs[dev];
		
		println("%i : %i : ", DEBUG, dev, device->id, device->regcnt);
		for(size_t reg = 0; reg < device->regcnt; reg++){
			println("\t%i : %c", DEBUG, reg, device->regs[reg].id);
		
			/* If you want more debug enable this */
			for(size_t bit = 0; bit < ECP_REG_WIDTH; bit++){
				println("\t\t%i : %i", DEBUG, bit, 
					device->regs[reg].bits[bit].enabled);
				
			}

		}
	}
	
	/* Should now be ready. Initialize ports */
	println("INITIALIZING ECP DEVICES. This might take some time!", INFO);
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
		
			/* If you want more debug enable this */
			/*for(size_t bit = 0; bit < ECP_REG_WIDTH; bit++){
				println("\t\t%i : %i", DEBUG, bit, 
					device->regs[reg].bits[bit].enabled);
				
			} */

		}
	}

	println("ECP init done!", DEBUG);
	
	return 0;
}

int start_ecp(){

	pthread_t ecp_thread;
	if(pthread_create(&ecp_thread,NULL,loop_ecp,NULL)){
                 println("failed to create thread", ERROR);
                 return -2;
        }
	return 0;
}

void* loop_ecp(){
	
	struct timespec tim, tim2;

	println("ECP entering operational state!", DEBUG);

	while(!shutting_down){
               
		tim.tv_sec = 0;
                tim.tv_nsec = PATROL_INTERVAL_NS;
                
                if(nanosleep(&tim , &tim2) < 0 ){
                         println("interupt in ECP loop!! sleep failed!",ERROR);
		}
		if(!ecp_initialized){

			if(ecp_bus_init() < 0){
				println("Failed to initialize ECP bus!", ERROR);
				continue;
			}
		}

                if(ecp_get_updates() < 0){
                        println("Failed to update ECP devices!!",ERROR);
                }
	}
	println("ECP leaving operational state", DEBUG);


	return NULL;

}

int ecp_get_updates(){

	for(size_t errcnt = 0; errcnt < 10; errcnt++){

		/* Yes, updating stuff is that easy */
		for(size_t i = 0; i < ecp_devcnt; i++){
			
			if(ecp_send_message(ecp_devs[i].id, REQ_SEND, NULL, 0) 
				< 0){
				continue;
			}
			
			if(ecp_devs[i].analog->used){

				if(send_ecp_analog_req(ecp_devs[i].id) < 0){
					continue;
				}
			}
		}
		return 0;
	}
	return -1;
}

int ecp_check_dependency(json_object* dependency){
	
	if(!ecp_initialized){
		/* Standby */
		return false;
	}
	
	json_object* type = json_object_object_get(dependency, "type");
	const char* type_name ;
	if(type == NULL){
		type_name = "port";
	}else{
		type_name = json_object_get_string(type);
	}	

	if(strcasecmp(type_name, "port") == 0){
		
		return ecp_check_port_dependency(dependency);
	}else if(strcasecmp(type_name, "analog") == 0){
		
		return ecp_check_analog_dependency(dependency);

	}else{
		println("Invalid type in ECP dependecy! Duming root:"
			, ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency, 
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	return 0;

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

	struct ecproto_device_t* devp = escp_get_dev_from_id(device_id);
	if(devp == NULL){
		println("Uninitialized dependency! This shouldn't happen!",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, dependency, 
			JSON_C_TO_STRING_PRETTY);
		
		return -4;
	}
	
	struct ecproto_port_register_t* regp = 
		escp_get_reg_from_dev(reg_id, devp);
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

int ecp_check_analog_dependency(json_object* dependency){
	
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
	if(dev == NULL){
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
		return value < escp_get_dev_from_id(device_id)->analog->value;
	}
	else if(strcasecmp(th, "below") == 0){
		return value > escp_get_dev_from_id(device_id)->analog->value;
	}
	else if(strcasecmp(th, "is") == 0){
		return value == escp_get_dev_from_id(device_id)->analog->value;
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

int ecp_trigger(json_object* trigger){
	json_object* type = json_object_object_get(trigger, "type");
	const char* type_name;
	/* By default a port trigger is assumed */
	if(type == NULL){
		type_name = "port";
	}else{
		type_name = json_object_get_string(type);
	}

	if(strcasecmp(type_name, "port") == 0){
		return ecp_trigger_port(trigger);
	}else if(strcasecmp(type_name, "secondary_trans") == 0){
		/* Send a specified string  */
		return ecp_trigger_secondary_trans(trigger);
		
	}else{
		println("Failed to execute ECP trigger with invalid type! Dump:"
			, ERROR);
		
		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	return 0;
}

int ecp_trigger_secondary_trans(json_object* trigger){
	
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

	return send_ecp_secondary(device_id, (char*) strin);
}

/* Execute a port trigger. Set a GPIO pin to the desired value */
int ecp_trigger_port(json_object* trigger){
	
	json_object* dev = json_object_object_get(trigger, "device");
	if(dev == NULL){
		println("Device not defined in ECP dependency! Dumping root: ", 
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	size_t device_id = json_object_get_int(dev);

	json_object* reg = json_object_object_get(trigger, "register");
	if(reg == NULL){
		println("Register not defined in ECP dependency! Dumping root: "
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
		println("Bit not defined in ECP dependency! Dumping root: ", 
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

	return send_ecp_port(device_id, reg_id, bit_id, target);

}

uint8_t* recv_ecp_frame(int fd, size_t* len){
 	
	pthread_mutex_lock(&ecp_readlock);

	uint8_t * frame = NULL;
	size_t frame_length = 0;

	while(frame_length < 255){
		
		uint8_t octet;
		int n = wait_for_data(ECP_TIMEOUT, fd);
		if(n == 0){
			println("Timeout in poll on ecp fd recv!", WARNING);
			if(frame != NULL){
				println(printable_bytes(frame, frame_length), WARNING);
			}
			free(frame);
			frame = NULL;
			break;
		}else if(n < 0){
			println("Failed to read from ECP bus! Device dead?", 
				ERROR);
			free(frame);
			frame = NULL;
			break;
		}

		n = read(fd, &octet, sizeof(uint8_t));
		if(n == 0){
			free(frame);
			println("ECP frame receive timed out!", WARNING);
			frame = NULL;
			break;
		}else if(n < 0){
			println("Failed to read from ECP bus! Device dead?", 
				ERROR);
			free(frame);
			frame = NULL;
			break;
		}
	
		frame = realloc(frame, ++frame_length);
		frame[frame_length - 1] = octet;
		if(octet == 0xFF){
			if(frame_length < ECPROTO_OVERHEAD){
				free(frame);
				frame = NULL;
				*len = 0;
				break;
			}
			*len = frame_length;
			break;
		}

	
	}
	pthread_mutex_unlock(&ecp_readlock);
	return frame;
}


bool validate_ecp_frame(uint8_t* frame, size_t len){

	if(frame[len - 1] != ECP_CMD_DEL){
		/* No end char is present at the end! */
		println("ECP frame not terminated!", ERROR);
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
	
	if(snd_frm != NULL && !validate_ecp_frame(snd_frm, snd_len)){
		println("Sent invalid ecp frame! WTF?", ERROR);
		println("Critical bug! This shouldn't occure.", ERROR);
		return -2;
	}

	size_t recv_payload_len = recv_len - ECPROTO_OVERHEAD;
	/* Switch by the receiving frame's id */
	switch(recv_frm[ECP_ID_IDX]){
		case 8:
			if(recv_frm[recv_len - 4] != 0){
				println(
"ECP received error frame, which isn't null-terminated!"
					,ERROR);
				return -3;
			}
			/* Print error string */
			println("ECP action aborted due to error response:",
				ERROR);
			println("%s\n", ERROR, &recv_frm[ECP_PAYLOAD_IDX]);
			return -4;
			break;
		case 0:
			println("The ECP slave has requested an initialisation",
				DEBUG);

			ecp_initialized = false;
			break;
		case 2:
			/* I will now get some more frames. Tell the overlying
			 * function that it should read some more frames */
			if(recv_payload_len >= 1){
				return recv_frm[ECP_PAYLOAD_IDX];
			}
			break;
		case 3:
			/* Register device. If payload is longer than 1 byte,
			 * and the 2nd byte is 0xBB, the
			 * connection is enumerated*/
			
			ecp_register_device(recv_frm[ECP_ADDR_IDX]);
			if(recv_payload_len >= 1 && recv_frm[ECP_PAYLOAD_IDX] == 
				recv_frm[ECP_ADDR_IDX]){
				/* Bus is enumerated */
				return 0;
			}else{
				return 1;
			}

			break;

		case 4:
			/* TODO */
			return 0;
			break;
		case 7:
		case 5:
			/* The reply to a port definition or port write */
			if(recv_payload_len < 1 || !recv_frm[ECP_PAYLOAD_IDX]){
				return -6;
			}

			break;
		case 6:
			/* The reply to a port state get. */
			if(recv_payload_len < 3){
				println("Too few ecp params for action %i!",
					ERROR, recv_frm[ECP_ID_IDX]);
				return -7;
			}
			
			return set_ecp_current_state(recv_frm[ECP_ADDR_IDX], 
				recv_frm[ECP_PAYLOAD_IDX], 
				recv_frm[ECP_PAYLOAD_IDX + 1], 
				recv_frm[ECP_PAYLOAD_IDX + 2]);

			break;


		case 10:
			/* All registers of a device are in the payload
			 */
			for(size_t i = 0; i < recv_payload_len; i++){
				ecp_register_register(recv_frm[ECP_ADDR_IDX], 
					recv_frm[ECP_PAYLOAD_IDX + i]);
			}
			break;

		case 11:

			if(recv_payload_len < 3){
				println("Received too short ecp message 11",
					ERROR);
				return -8;
			}
			{
				struct ecproto_device_t* dev = 
					escp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);
				
				if(dev == NULL){
					println("Recv 11 from unknown dev! wut?"
						, ERROR);
					return -9;
				}
				
				struct ecproto_port_register_t* reg = 
					escp_get_reg_from_dev(
					recv_frm[ECP_PAYLOAD_IDX], dev);
				
				if(reg == NULL){
					println("Recv 11 with unknown reg! wut?"
						, ERROR);
					return -10;
				}
				
				if(recv_frm[ECP_PAYLOAD_IDX + 1]  >= 
					ECP_REG_WIDTH){
					println("Recv 11 with too > bit! wut?"
						, ERROR);
					return -11;
				}

				struct ecproto_port_t* prt = &reg->bits[
					recv_frm[ECP_PAYLOAD_IDX + 1]];

				if(recv_frm[ECP_PAYLOAD_IDX + 2] > 1){
					println("Recv 11 with invld value! wut?"
						, ERROR);
					return -12;
				}
				prt->enabled = recv_frm[ECP_PAYLOAD_IDX + 2];
			}
			break;
		case 12:
			if(recv_payload_len < 1){
				println("Received ecp frame 12 with <1 params",
					ERROR);
				return -13;
			}
			if(!recv_frm[ECP_PAYLOAD_IDX]){
				println("Failed to transmit serial", ERROR);
			}
			break;

		case 13:
			
			if(recv_payload_len < 1){
				println("Received ecp frame 13 with <1 params",
					ERROR);
				return -14;
			}
			escp_get_dev_from_id(recv_frm[ECP_ADDR_IDX])
				->analog->value = recv_frm[ECP_PAYLOAD_IDX];
			break;
		default:
			/* You lazy bastard! */
			println("Unimplemented ECP response: %i!", WARNING,
				recv_frm[ECP_ID_IDX]);
			break;
	}
	return 0;
}

int ecp_bus_init(){
	/* Enumerate bus and initialize device structures */	
	int n = 0;
#define MAX_ERRCNT 100
	
	for(size_t errcnt = 0; errcnt < MAX_ERRCNT; errcnt++){
		n = 0;
		/* Enumerate bus */
		n |= ecp_send_message(0, 3, NULL, 0);
	
		if(n){
			println("Failed to enumerate ecp bus!", ERROR);
			goto error;
		}

		for(size_t dev = 0; dev < ecp_devcnt; dev++){
			
			struct ecproto_device_t* device = &ecp_devs[dev];
			if(device->analog->used){

				if(send_ecp_analog_req(device->id) < 0){
					println("Failed to init ecp analog!",
						ERROR);
					goto error;
				}
			}
			/* Get all registers */
			n |= ecp_send_message(device->id, 
				REGISTER_LIST, NULL, 0);
			
			for(size_t reg = 0; reg < device->regcnt; reg++){
				struct ecproto_port_register_t* regp = 
					&device->regs[reg];
					/* INIT All registers */

				for(size_t bt = 0; bt < ECP_REG_WIDTH; bt++){
					struct ecproto_port_t* bit = 
						&regp->bits[bt];
					
					uint8_t enable_dat [] = {regp->id, 
						bit->bit};
					n |= ecp_send_message(device->id, 
						PIN_ENABLED, enable_dat, 
						sizeof(enable_dat));
					
					if(n){
						println("Failed to get ECP en",
							WARNING);
						goto error;
					}
					if(!bit->enabled){
						continue;
					}
					
					n |= send_ecp_ddir(device->id, regp->id, 
						bit->bit, bit->ddir); 

					if(n){
						println("Failed to set ECP DDIR",
							WARNING);
						goto error;
					}
					
					if(bit->ddir != ECP_OUTPUT){
					
						n |= send_ecp_port(
						device->id, regp->id, 
						bit->bit, bit->target); 
						
						if(n){
							println(
							"Failed to set ECP PORT"
								,WARNING);
							goto error;
						}

					}
					
					n |= get_ecp_port(device->id, regp->id, 
						bit->bit); 
					if(n){
						println("Failed to get ECP PRT",
							WARNING);
						goto error;
					}
					
					
					if(bit->ddir == ECP_OUTPUT){
						bit->target = bit->current;
					}
				}
			}
		}
	break;

error:
		println("Failed to init ECP, retry %i/%i", WARNING, errcnt , 
			MAX_ERRCNT);
		continue;

	}
	if(n == 0){
		println("ECP INITIALIZED", DEBUG);
		ecp_initialized = true;
	}

	return n;
}

/*
 * ECP SEND FUNCTIONS
 *
 * Functions in here are used to send stuff to a µc.
 */

int get_ecp_port(size_t device_id, char reg_id, size_t pin_id){

	uint8_t msg[] = {reg_id, pin_id};
	return ecp_send_message(device_id, GET_PORT_ACTION, msg, sizeof(msg));

	return 0;

}

/* If the specified pin is an output, this sets the pin's value, if the pin
 * is an input, this enables or disables the pullup resistor. */
int send_ecp_port(size_t device_id, char reg_id, size_t pin_id, bool port){

	uint8_t msg[] = {reg_id, pin_id, port};
	return ecp_send_message(device_id, WRITE_PORT_ACTION, msg, 
		sizeof(msg));

	return 0;

}

int send_ecp_ddir(size_t device_id, char reg_id, size_t pin_id, bool ddir){

	uint8_t msg[] = {reg_id, pin_id, ddir};
	return ecp_send_message(device_id, DEFINE_PORT_ACTION, msg, 
		sizeof(msg));

	return 0;

}

/* This sends a null terminated string to a secondary connection, though it 
 * doesn't transmit the nul terminator.
 */
int send_ecp_secondary(size_t device_id, const char* str){

	return ecp_send_message(device_id, SECONDARY_PRINT, (uint8_t*) str, 
		strlen(str) + sizeof(char));

}

int send_ecp_analog_req(size_t device_id){
	
	return ecp_send_message(device_id, ADC_GET, NULL, 0);
	
}

int ecp_send_message(size_t device_id, 
	uint8_t action_id, uint8_t* payload, size_t payload_len){
	
	int ret = 0;

	uint8_t* snd_frame = NULL;
	size_t snd_len = 0;
 	pthread_mutex_lock(&ecp_lock);
	int n = wait_for_data(ecp_fd, 0);
	if(n < 0){
		println("Failed to poll on ECP fd! Device dead?", ERROR);
		pthread_mutex_unlock(&ecp_lock);
		return -1;
#ifndef NORECOVER
	}else if(n > 0){
		println("ECP found data in buffer. Assuming lost frame.",
			WARNING);
		if(ecp_receive_msgs(snd_frame, snd_len) < 0){
			
			println("Failed to receive ECP message!", WARNING);
			println("INVALID DATA IN BUFFER, IGNORING",WARNING);
		}
	}
#else
	}
#endif

	n = write_ecp_msg(device_id, ecp_fd, action_id, payload, payload_len, 
		&snd_frame, &snd_len);

	if(n < 0){
		println("Failed to send ecp message!", ERROR);
		pthread_mutex_unlock(&ecp_lock);
		return -2;
	}

	if(ecp_receive_msgs(snd_frame, snd_len) < 0){
		println("Failed to receive ECP message!", ERROR);
		ret = -3;
	}
	
	if(snd_frame != NULL){
		free(snd_frame);
	}
	pthread_mutex_unlock(&ecp_lock);

	return ret;
}

int write_ecp_msg(size_t dev_id, int fd, uint8_t action_id, uint8_t* payload, 
	size_t payload_length, uint8_t** frme, size_t* frame_length){

	uint8_t frame[255];
	memset(frame, 0, 255);

	frame[ECP_LEN_IDX] = ECPROTO_OVERHEAD + payload_length;
	frame[ECP_ADDR_IDX] = dev_id;
	frame[ECP_ID_IDX] = action_id;
	memcpy(&frame[ECP_PAYLOAD_IDX], payload, payload_length);
	uint16_t crc = 0x0;
	
	uint8_t crc_low = 0, crc_high = 0;
	for(size_t ov = 0; ov < ( 255 - payload_length); ov++){
		
		crc = ibm_crc(frame,frame[ECP_LEN_IDX] - 3);
		crc_high = (crc >> 8) & 0xFF;
		crc_low = crc & 0xFF;
		if(crc_high == 0xFF || crc_low == 0xFF){
			
			frame[ECP_LEN_IDX]++;
			continue;
		}else{
			/* No 0xff bytes have ben found inside the checksum.
			 * Stop 0-padding the frame */
			break;
		}
	}

	frame[frame[ECP_LEN_IDX] - 3] = crc_high;
	frame[frame[ECP_LEN_IDX] - 2] = crc_low;
	frame[frame[ECP_LEN_IDX] - 1] = 0xFF;
	
	if(!validate_ecp_frame(frame, frame[ECP_LEN_IDX])){
		println("Failed to construct ecp frame!", ERROR);
		return -1;
	}

	if(frme != NULL){
		*frame_length = frame[ECP_LEN_IDX] * sizeof(uint8_t);
		*frme = malloc(frame[ECP_LEN_IDX] * sizeof(uint8_t));
		memcpy(*frme, frame, sizeof(uint8_t) * *frame_length);
	}

	int n = write(fd, frame, frame[ECP_LEN_IDX]);	
	return n;
}

int ecp_receive_msgs(uint8_t* snd_frame, size_t snd_len){
	
	size_t reads = 1;
	int n = 0;
	for(size_t i = 0; i < reads; i++){

	size_t recv_len = 0;

		uint8_t* recv_frame = recv_ecp_frame(ecp_fd, &recv_len);
			
		if(recv_frame == NULL){
			println("Failed to receive ecp frame. Sent frame was:",
				 WARNING);
			if(snd_frame == NULL){
				println("Unprintable!", ERROR);
			}else{
				println(printable_bytes(snd_frame,snd_len), 
					ERROR);
			}
			pthread_mutex_unlock(&ecp_lock);
			return -1;
		}

		n = parse_ecp_input(recv_frame, recv_len, snd_frame, snd_len);
		
		if( n < 0 ){
			println("Failed to parse received ecp frame", WARNING);
			char* dat = printable_bytes(recv_frame, recv_len);
			if(dat != NULL){
				println(dat, ERROR);
				free(dat);
			}
			
			free(recv_frame);
			pthread_mutex_unlock(&ecp_lock);
			return -2;
		}
		
		free(recv_frame);
		reads += n;
	}
	return 0;
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
		
		/* This will be overwritten by a check at the beginning */
		prt->current = false;
		prt->enabled = false;
		prt->target = true;
	}

	return 0;
}

int set_ecp_current_state(size_t dev_id, char reg_id, size_t bit, bool state){

	struct ecproto_device_t* dev = escp_get_dev_from_id(dev_id);
	if(dev == NULL){
		println("Received ECP device which is not enumerated: %i", 
			ERROR, dev_id);
		return -1;
	}
	
	struct ecproto_port_register_t* reg = escp_get_reg_from_dev(reg_id, 
		dev);
	
	if(reg == NULL){
		println("Received ECP register wich is not enumerated: %i->%c", 
			ERROR, dev_id, reg_id);
		return -2;
	}

	if(bit >= ECP_REG_WIDTH){
		println("Received ECP bit which is too high: %i->%c->%i", 
			ERROR, dev_id, reg_id, bit);
		return -3;
	}

	struct ecproto_port_t* port = &reg->bits[bit];
	port->current = state;

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
