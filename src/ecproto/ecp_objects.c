/* An EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains objects and other global variables needed to the
 * operation of the program and functions altering them.
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

size_t ecp_devcnt = 0;
struct ecproto_device_t *ecp_devs = NULL;
bool ecp_initialized = false;


/* Registers an input pin, there are input pins which are not really inputs,
 * the is_input flag sets the ddir register accordingly */
int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, bool
	pulled, bool is_input){

	ecp_register_register(device_id, reg_id);

	struct ecproto_device_t* device =  ecp_get_dev_from_id(device_id);

	struct ecproto_port_register_t*  reg_p =
		ecp_get_reg_from_dev(reg_id, device);

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

	struct ecproto_device_t*  device = ecp_get_dev_from_id(device_id);

	struct ecproto_port_register_t*  reg_p =
		ecp_get_reg_from_dev(reg_id, device);

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

	struct ecproto_device_t*  device = ecp_get_dev_from_id(id);

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
		if(device->analog == NULL){
			println("Failed to malloc memory for analog struct in "
				"device!", ERROR);
			return -1;
		}
		device->analog->used = false;
		device->analog->value = 0;
		
		/* A device is assumed GPIO-capable, in case it has responded
		 * to be in it's device purpose. On error it is possible that
		 * this is assumed.
		 */
		device->gpio_capable = false;
		
		device->mfrc522_capable = false;
		device->mfrc522_devcnt = 0;
		device->mfrc522_devs = NULL;

		device->analog_channels = NULL;
		device->analog_channel_count = 0;

	}

	return 0;
}

#endif /* NOEC */
