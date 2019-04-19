/* A EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains bus initialisation routines in various versions.
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
	 * Last installation left: Escape Game Munich Orphanage WESTPARK 3
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

	if(device->fast_gpio_capable){
		/* The device is capable to initialize a faster way! */
		return init_ecp_gpio_fast(device);
	}

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
		if(ecp_bitwise_init_gpio_reg(device, regp)){
			println("Failed to init ECP GPIO REG %c for device %i!",
				ERROR, device->id, regp->id);
			return -1;
		}


	}

	return n;

}
/* Initialise a device's GPIO subpart, but faster! */
int init_ecp_gpio_fast(struct ecproto_device_t* device){

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

	/* Get a list of disabled pins in order to ignore them */
	n |= ecp_send_message(device->id, GET_DISABLED_PINS, NULL, 0);
	if(n){
		println("Failed to list disabled pins for dev %i",
			ERROR, device->id);
		return -1;
	}
	println("Fastly initializeing %i regs for dev %i!", DEBUG,
		device->regcnt, device->id);

	for(size_t reg = 0; reg < device->regcnt; reg++){

		struct ecproto_port_register_t* regp = &device->regs[reg];

		if(get_pins_enabled_from_register(regp) == 0xFF){
			/* Fast init is possible for this register */
			println("Fast init dev %i reg %c!", DEBUG,
				device->id, regp->id);

			uint8_t payload_set[] = {regp->id,
				get_ddir_from_register(regp),
				get_port_from_register(regp)};

			n |= ecp_send_message(device->id, SET_GPIO_REGS,
				payload_set, sizeof(payload_set));
			if(n){
				println("Failed to set register %c at dev %i!",
					ERROR,regp->id, device->id);
				return -1;
			}
			uint8_t payload_get = regp->id;
			n |= ecp_send_message(device->id, GET_GPIO_REGS,
				&payload_get, sizeof(payload_get));
			if(n){
				println("Failed to set register %c at dev %i!",
					ERROR,regp->id, device->id);
				return -1;
			}


		}else{
			/* Fast init isn't possible for this register */
			println("Slow init dev %i reg %c!", DEBUG,
				device->id, regp->id);
			if(ecp_bitwise_init_gpio_reg(device, regp)){
				println("Failed to init ECP GPIO REG %c for "
					"device %i!",
					ERROR, device->id, regp->id);
				return -1;
			}

		}

	}

	return n;
}

int ecp_bitwise_init_gpio_reg(struct ecproto_device_t* device,
	struct ecproto_port_register_t* regp){

	int n = 0;

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

	return n;
}

#endif /* NOEC */
