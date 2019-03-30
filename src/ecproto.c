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
#include <math.h>

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
		device->received_regcnt = 0;
		device->regs = NULL;
		device->analog = malloc(sizeof(struct ecproto_analog_t));
		device->analog->used = false;
		device->analog->value = 0;
		/* A device is assumed GPIO-capable, in case it has responded
		 * to be in it's device purpose, or it has errored, while
		 * attempting to receive this.
		 */
		device->gpio_capable = false;
		device->mfrc522_capable = false;
		device->mfrc522_devcnt = 0;
		device->mfrc522_devs = NULL;

	}
	
	return 0;
}

int init_ecp(){	
	pthread_mutex_init(&ecp_lock, NULL);
	pthread_mutex_init(&ecp_readlock, NULL);
	pthread_mutex_init(&ecp_state_lock, NULL);
	

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

	/* Add all devices. */

	json_object* decnt = json_object_object_get(config_glob,
		"ecp_device_count");
	size_t devn_cnt = 1;

	if(decnt != NULL){
		devn_cnt = json_object_get_int(decnt);
	}
	println("initially known device count: %i", DEBUG, devn_cnt);

	for(size_t i = 0; i < devn_cnt; i++){
		if(ecp_register_device(i) < 0){
			println("FAILED TO ADD ECP DEVICE %i!!", ERROR, i);
			return -1;
		}
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
			
			/* TODO: Update old ECPROTO Devices to be able to remove
			 * this code... */
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

int reset_ecp(){
	
	/* The update thread will eventually see this and will throw on the
	 * initialisation routine
	 */
	ecp_initialized = false;
	
	return 0;
}

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
		if(value < escp_get_dev_from_id(device_id)->analog->value){
			return true;
		}else{
			return escp_get_dev_from_id(device_id)->analog->value /
			(float)value;
		}
	}else if(strcasecmp(th, "below") == 0){
		if(value > escp_get_dev_from_id(device_id)->analog->value){
			return true;
		}else{
			return (255 - 
			escp_get_dev_from_id(device_id)->analog->value) /
			(255 - (float)value);
		}
	}else if(strcasecmp(th, "is") == 0){
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
	
	struct ecproto_device_t* dev = escp_get_dev_from_id(device_id);

	if(dev == NULL){
		println("ECP MFRC522 dependency with unknown dev %i! Dumping:",
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
		println("Awaiting bus init due to CRC mismatch!!!!", WARNING);
		ecp_initialized = false;
		return false;
	}
	/* The frame should be valid */
	return true;
}

int parse_ecp_input(uint8_t* recv_frm, size_t recv_len, uint8_t* snd_frm, size_t 
	snd_len){

	/* Validate sent and received frames */
	if(!validate_ecp_frame(recv_frm, recv_len)){
		println("Received invalid ecp frame!", WARNING);
		return -1;
	}
	
	if(snd_frm != NULL && !validate_ecp_frame(snd_frm, snd_len)){
		
		println("Sent invalid ecp frame! WTF?", ERROR);
		println("Critical bug! This shouldn't occure.", ERROR);
		
		return -1;
	}
	
	size_t recv_payload_len = recv_len - ECPROTO_OVERHEAD;
	/* Switch by the receiving frame's id */
	switch(recv_frm[ECP_ID_IDX]){
		case ERROR_ACTION:
			if(recv_frm[recv_len - 4] != 0){
				
				println("ECP received error frame, which isn't"
					" null-terminated!",ERROR);

				return -1;
			}
			/* Print error string */
			println("ECP action aborted due to error response:"
				" \"%s\"",
				ERROR, &recv_frm[ECP_PAYLOAD_IDX]);
			return -1;
			break;
		case INIT_ACTION:
			println("ECP slave %i has requested an initialisation",
				DEBUG, recv_frm[ECP_ADDR_IDX]);

			ecp_initialized = false;
			break;
		case SEND_NOTIFY:
			/* I will now get some more frames. Tell the overlying
			 * function that it should read some more frames */
			if(recv_payload_len >= 1){
				return recv_frm[ECP_PAYLOAD_IDX];
			}
			break;

		case SET_PWM:
		case WRITE_PORT_ACTION:
		case DEFINE_PORT_ACTION:
			/* Just a generic success reply */
			if(recv_payload_len < 1 || !recv_frm[ECP_PAYLOAD_IDX]){
				return -6;
			}

			break;
		case GET_PORT_ACTION:
			/* The reply to a port state get. */
			if(recv_payload_len < 3){
				println("Too few ecp params for action %i!",
					ERROR, recv_frm[ECP_ID_IDX]);
				return -1;
			}
			
#if DEBUG_LVL > DEBUG_NORM
			println("ECP Port update: %i dev %c reg %i bit --> %i",
				DEBUG,
				recv_frm[ECP_ADDR_IDX], 
				recv_frm[ECP_PAYLOAD_IDX], 
				recv_frm[ECP_PAYLOAD_IDX + 1], 
				recv_frm[ECP_PAYLOAD_IDX + 2]);
#endif
			return set_ecp_current_state(recv_frm[ECP_ADDR_IDX], 
				recv_frm[ECP_PAYLOAD_IDX], 
				recv_frm[ECP_PAYLOAD_IDX + 1], 
				recv_frm[ECP_PAYLOAD_IDX + 2]);

			break;
		
		case REGISTER_COUNT:
			/* The target amount of registers should be contained
			 * within the payload.
			 */

			if(recv_payload_len > 0){
				
				struct ecproto_device_t* dev = 
					escp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);

				if(dev == NULL){
					println(
						"Register count reply from" 
						"unknown device!",
						ERROR);
					return -1;
				}


				dev->received_regcnt = 
					recv_frm[ECP_PAYLOAD_IDX];
#if DEBUG_LVL > DEBUG_NORM
				println("ECP dev %i notified us of %i regs!", 
					DEBUG, recv_frm[ECP_ADDR_IDX],
					recv_frm[ECP_PAYLOAD_IDX]
					);
#endif
			}else{
				println("ECP register count without payload!",
					ERROR);
				return -1;
			}
			break;

		case REGISTER_LIST:
			/* 
			 * All registers of a device are in the payload
			 */
			 {

				struct ecproto_device_t* dev = 
					escp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);

				if(dev == NULL){
					println(
						"Register list reply from" 
						"unknown device!",
						ERROR);
					return -1;
				}
				
				for(size_t i = 0; i < dev->received_regcnt; 
					i++){

#if DEBUG_LVL > DEBUG_NORM
					println("Reg list dev %i reg %c",
						DEBUG, recv_frm[ECP_ADDR_IDX],
						recv_frm[ECP_PAYLOAD_IDX + i]);
#endif			
					
					ecp_register_register(
						recv_frm[ECP_ADDR_IDX], 
						recv_frm[ECP_PAYLOAD_IDX + i]);
				}
			}
			break;

		case PIN_ENABLED:

			if(recv_payload_len < 3){
				println("Received too short ecp message 11",
					ERROR);
				return -1;
			}
			{
				struct ecproto_device_t* dev = 
					escp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);
				
				if(dev == NULL){
					println("Recv 11 from unknown dev! wut?"
						, ERROR);
					return -1;
				}
				
				struct ecproto_port_register_t* reg = 
					escp_get_reg_from_dev(
					recv_frm[ECP_PAYLOAD_IDX], dev);
				
				if(reg == NULL){
					println("Recv 11 with unknown reg! wut?"
						, ERROR);
					return -1;
				}
				
				if(recv_frm[ECP_PAYLOAD_IDX + 1]  >= 
					ECP_REG_WIDTH){
					println("Recv 11 with too > bit! wut?"
						, ERROR);
					return -1;
				}

				struct ecproto_port_t* prt = &reg->bits[
					recv_frm[ECP_PAYLOAD_IDX + 1]];

				if(recv_frm[ECP_PAYLOAD_IDX + 2] > 1){
					println("Recv 11 with invld value! wut?"
						, ERROR);
					return -1;
				}
				prt->enabled = recv_frm[ECP_PAYLOAD_IDX + 2];
			}
			break;
		case SECONDARY_PRINT:
			if(recv_payload_len < 1){
				println("Received ecp frame 12 with <1 params",
					ERROR);
				return -1;
			}
			if(!recv_frm[ECP_PAYLOAD_IDX]){
				println("Failed to transmit serial", ERROR);
			}
			break;

		case ADC_GET:
			
			if(recv_payload_len < 1){
				println("Received ecp frame 13 with <1 params",
					ERROR);
				return -1;
			}
			escp_get_dev_from_id(recv_frm[ECP_ADDR_IDX])
				->analog->value = recv_frm[ECP_PAYLOAD_IDX];
			break;

		case GET_PURPOSE:
		{
#if DEBUG_LVL > DEBUG_NORM
			println("Device %i notified us of it's %i "
				"capablilties", DEBUG, recv_frm[ECP_ADDR_IDX],
				recv_frm[ECP_PAYLOAD_IDX]);
#endif			
			struct ecproto_device_t* dev = escp_get_dev_from_id(
				recv_frm[ECP_ADDR_IDX]);

			if(dev == NULL){
				println("Received purpose from unknown dev %i!",
					ERROR, recv_frm[ECP_ADDR_IDX]);
				return -1;
			}

			for(size_t i = 0; i < recv_frm[ECP_PAYLOAD_IDX]; i++){
#if DEBUG_LVL > DEBUG_NORM
				println("Device %i notified us of it's %i "
					"capablilty", DEBUG, 
					recv_frm[ECP_ADDR_IDX],
					recv_frm[ECP_PAYLOAD_IDX + i + 1]);
#endif			
				
				switch(recv_frm[ECP_PAYLOAD_IDX + i + 1]){
					
					case SPECIALDEV_GPIO:
						dev->gpio_capable = true;
						break;
					case SPECIALDEV_MFRC522:
						dev->mfrc522_capable = true;
						break;
					case SPECIALDEV_PWM:
					case SPECIALDEV_OLD_ANALOG:
						break;

					default:
						println("Fancy! A new purpose!"
							"Now would you mind"
							"Implementing it please"
							"? %i ecp purpose recvd"
							,WARNING, 
							recv_frm[ECP_PAYLOAD_IDX 
							+ i + 1]);


				}

			}

		}
			break;

		case SPECIAL_INTERACT:
		/* This is for special devices which have a non-regular use. */
			
			return ecp_handle_special_interact(recv_frm);
			
			break;

		default:
			/* You lazy bastard! */
			println("Unimplemented ECP response: %i!", WARNING,
				recv_frm[ECP_ID_IDX]);
			break;
	}
	return 0;
}

int ecp_handle_special_interact(uint8_t* recv_frm){

	

	switch(recv_frm[ECP_PAYLOAD_IDX]){
		
		case SPECIALDEV_MFRC522:
			return ecp_handle_mfrc522(recv_frm);
			break;
		case SPECIALDEV_PWM:
		case SPECIALDEV_GPIO:
		case SPECIALDEV_OLD_ANALOG:
			println("Special device interact for non-specified id!",
				WARNING);
			return 0;
		default:
			println("Unknown or invalid special interact attempt "
				"for ecp special id %i. Ignoreing", WARNING,
				recv_frm[ECP_PAYLOAD_IDX]);
			break;

	}
	
	return 0;
}

int ecp_handle_mfrc522(uint8_t* recv_frm){
	
	struct ecproto_device_t* dev = escp_get_dev_from_id(
		recv_frm[ECP_ADDR_IDX]);

	if(dev == NULL){
		println("Received mfrc522 purpose from unknown dev %i!",
			ERROR, recv_frm[ECP_ADDR_IDX]);
		return -1;
	}
	
	/* The mfrc522 now has anothr sub-id... So many...*/
	switch(recv_frm[ECP_PAYLOAD_IDX + 1]){
		
		case MFRC522_GET_ALL_DEVS:
			/* This gives me only the total amount 
			 * of devices locally attached.
			 */
			
			if(dev->mfrc522_devcnt == 
				recv_frm[ECP_PAYLOAD_IDX + 2]){
				/* Nothing to do here! */
				return 0;
			}
			/* Update and init! */
			dev->mfrc522_devcnt = 
				recv_frm[ECP_PAYLOAD_IDX + 2];
			
			dev->mfrc522_devs = realloc(dev->mfrc522_devs, 
				(dev->mfrc522_devcnt * 
				sizeof(struct ecproto_mfrc522_dev_t)));
			
			memset(dev->mfrc522_devs, 0, (dev->mfrc522_devcnt * 
				sizeof(struct ecproto_mfrc522_dev_t)));

			break;

		case MFRC522_GET_TAG:

			if(dev->mfrc522_devcnt <= 
				recv_frm[ECP_PAYLOAD_IDX + 2]){
				
				println("ECP MFRC522 id too large: %i/%i!",
					ERROR, recv_frm[ECP_PAYLOAD_IDX + 2],
					dev->mfrc522_devcnt);
				return -1;
			}

			 struct ecproto_mfrc522_dev_t* tag =
				&dev->mfrc522_devs[
				recv_frm[ECP_PAYLOAD_IDX + 2]];
			
			tag->tag = be32toh(*((uint32_t*)
				&recv_frm[ECP_PAYLOAD_IDX + 4]));
			
			tag->tag_present = !!recv_frm[ECP_PAYLOAD_IDX + 3];

			if(tag->tag == 0x45524F52){
				tag->working = false;
			}else{
				tag->working = true;
			}
			println("ECP MFRC522 Update: dev %i tag %i is here "
				"%i working %i -->%lx/%lu",
				DEBUG, recv_frm[ECP_ADDR_IDX],
				recv_frm[ECP_PAYLOAD_IDX + 2],
				recv_frm[ECP_PAYLOAD_IDX + 3],
				tag->working,
				tag->tag, tag->tag);

			break;


		default:
			println("ECP mfrc522 unknown sub-id %i",
				WARNING, 
				recv_frm[ECP_PAYLOAD_IDX + 1]);
			break;
	}

	return 0;
}

int ecp_bus_init(){
	/* Enumerate bus and initialize device structures */	
#define MAX_ERRCNT 100
	
	println("Attempting ecproto bus initialisation...", DEBUG);
	println("During this time no updates will be received from the bus!",
		DEBUG);
	println("Please, for the time beeing, stand by!", DEBUG);
	println("Any ECPROTO activity is now LOCKED", DEBUG);
	pthread_mutex_lock(&ecp_state_lock);
	
	int err = 0;
	
	for(size_t errcnt = 0; errcnt < MAX_ERRCNT; errcnt++){
		/* Initialize devices */
		err = 0;

		for(size_t dev = 0; dev < ecp_devcnt; dev++){
			
			struct ecproto_device_t* device = &ecp_devs[dev];
			
			if((err = init_ecp_device(device)) < 0){
				println("ECPROTO Device init failed for device"
					" %li", ERROR, device->id);
				break;
			}
			

		}
		
		if(err >= 0){
			
			println("ECP INITIALIZED", DEBUG);
			ecp_initialized = true;
			err |= ecp_get_updates();

			break;
		}
		println("ECPROTO Bus init failed at attempt %i/%i", ERROR,
			errcnt, MAX_ERRCNT);

	}
	/* Releasing the bus at ANY outcome */
	pthread_mutex_unlock(&ecp_state_lock);

	return err;
}

int init_ecp_device(struct ecproto_device_t* device){
	int n = 0;
	bool old_fw = true;
	
	/* I'm sending it twice just to be sure it really did come through */
	for(size_t i = 0; i < 2; i++){
		if(ecp_send_message(device->id, GET_PURPOSE, NULL, 0) >= 0){
			old_fw = false;
			break;
		}
		
	}

	/* TODO update old firmware in currently used Escape Game System
	 * installations so this is no longer neded! 
	 */
	if(old_fw){
		println("OUTDATED ECP MICROCONTROLLER VERSION ASSUMED!"
			"PLEASE UPDATE YOUR FIRMWARE TO THE LATEST VERSION!",
			WARNING);
		
		println("The Above errors may be ignored. GPIO capability is"
			"assumed by device %li from this point on.", WARNING,
			device->id);
		device->gpio_capable = true;
		
	}

	if(device->analog->used){

		if(send_ecp_analog_req(device->id) < 0){
			println("Failed to init ecp analog!",
				ERROR);
			return -1;
		}
	}
	if(device->mfrc522_capable){
		uint8_t pay[] = {SPECIALDEV_MFRC522, MFRC522_GET_ALL_DEVS};
		n |= ecp_send_message(device->id, SPECIAL_INTERACT, pay, 
			sizeof(pay));
		
		for(uint8_t i = 0; i < device->mfrc522_devcnt; i++){
			uint8_t pay[] = {SPECIALDEV_MFRC522, 
				MFRC522_GET_TAG, i};
			n |= ecp_send_message(device->id, SPECIAL_INTERACT, pay, 
				sizeof(pay));

		}
	}

	if(device->gpio_capable){
		n |= init_ecp_gpio(device);
	}

	return n;

}

/* Initialize a devices's GPIO subpart. */

int init_ecp_gpio(struct ecproto_device_t* device){
	
	int n = 0;
	
	/* Get a register count */
	n |= ecp_send_message(device->id, REGISTER_COUNT, NULL, 0);
	
	if(n){

		println("Failed to count ecp regs for dev %i", ERROR, 
			device->id);
		return -1;
	}
	
	/* Get a list containing the above received amount of registers */
	n |= ecp_send_message(device->id, REGISTER_LIST, NULL, 0);
	if(n){
		println("Failed to list ecp regs for dev %i",
			ERROR, device->id);
		return -1;
	}
	
	for(size_t reg = 0; reg < device->regcnt; reg++){
		
		struct ecproto_port_register_t* regp = &device->regs[reg];
		
		/* INIT All registers */

		for(size_t bt = 0; bt < ECP_REG_WIDTH; bt++){
			
			struct ecproto_port_t* bit = &regp->bits[bt];
			
			uint8_t enable_dat [] = {regp->id, bit->bit};
			
			n |= ecp_send_message(device->id, PIN_ENABLED,
				enable_dat, sizeof(enable_dat));
			
			if(n){
				println("Failed to get ECP port enable state",
					WARNING);
				return -1;
			}
			if(!bit->enabled){
				/* Skip further initialisation of this port.
				 * It is disabled man, I can't do anything */
				continue;
			}
			
			n |= send_ecp_ddir(device->id, regp->id, 
				bit->bit, bit->ddir); 

			if(n){
				println("Failed to set ECP DDIR",
					WARNING);
				return -1;
			}
			
			if(bit->ddir != ECP_OUTPUT){
			
				n |= send_ecp_port(device->id, regp->id, 
					bit->bit, bit->target); 
				
				if(n){
					println("Failed to set ECP port"
						,WARNING);
					return -1;
				}

			}
			
			n |= get_ecp_port(device->id, regp->id, 
				bit->bit); 
			if(n){
				println("Failed to get ECP Port",
					WARNING);
				return -1;
			}
			
			
			if(bit->ddir == ECP_OUTPUT){
				bit->target = bit->current;
			}
			
			/* In case the targeted pin is not an input pin (in
			 * which case this addresses the pullup-resistors), nor
			 * is the targeted pin other than the default value
			 * for all outputs, it can be assumed, that he pin is
			 * doesn't need a refresh. 
			 */

			if(bit->ddir != ECP_OUTPUT || bit->target != 
				ECP_DEFAULT_TARGET){
			
				n |= send_ecp_port(device->id, regp->id, 
					bit->bit, bit->target); 
				
				if(n){
					println("Failed to set ECP PORT"
						,WARNING);
					return -1;
				}

			}
		}
	
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


	struct ecproto_device_t* dev = escp_get_dev_from_id(device_id);

	struct ecproto_port_register_t* reg = escp_get_reg_from_dev(reg_id, 
		dev);

	if(reg == NULL){
		println("Attempted unknown port write: dev %i reg %c pin%i ->"
		" %i", ERROR, device_id, reg_id, pin_id, port);
		return -1;
	}

	struct ecproto_port_t* prt = &reg->bits[pin_id];
	
	if(prt == NULL){
		println("Attempted unknown port write: dev %i reg %c pin%i ->"
		" %i", ERROR, device_id, reg_id, pin_id, port);
		return -1;
	}


	prt->target = port;
	
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
	
			/* Asynchroneously gets all device states. Hope that
			 * this solves stuff.. */
			ecp_initialized = false;
			println("Refreshing all ECP Devices as resul!",
				WARNING);
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
		prt->target = ECP_DEFAULT_TARGET;
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

/* This function is used to assemble all GPIO pins to the DDIR register */
uint8_t get_ddir_from_register(struct ecproto_port_register_t* reg){
	
	uint8_t ddir = 00;
	for(size_t i = 0; i < ECP_REG_WIDTH; i++){
		struct ecproto_port_t port = reg->bits[i];
		
		ddir |= port.ddir << i;


	}
	return ddir;
}

/* This function is used to assemble all GPIO pins to the DDIR register */
uint8_t get_port_from_register(struct ecproto_port_register_t* reg){
	
	uint8_t prt = 00;
	for(size_t i = 0; i < ECP_REG_WIDTH; i++){
		struct ecproto_port_t port = reg->bits[i];
		
		prt |= port.target << i;


	}

	return prt;
	
}

/* This functions assembles the pin enable states, as if they were a register */
uint8_t get_pins_enabled_from_register(struct ecproto_port_register_t* reg){
	
	uint8_t en = 00;
	for(size_t i = 0; i < ECP_REG_WIDTH; i++){
		struct ecproto_port_t port = reg->bits[i];
		
		en |= port.enabled << i;


	}

	return en;
	
}

#endif /* NOEC */
