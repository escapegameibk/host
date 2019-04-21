/* A EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains helper functions for the host's ecproto implementaion.
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

struct ecproto_device_t* ecp_get_dev_from_id(size_t id){
	for(size_t i = 0; i < ecp_devcnt; i++){
		if(ecp_devs[i].id == id){
			/* Found */
			return &ecp_devs[i];
		}
	}
	/* Not found */
	return NULL;
}

struct ecproto_port_register_t* ecp_get_reg_from_dev(char id, struct 
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
		prt->enabled = true;
		
		/* This will be overwritten by a check at the beginning */
		prt->current = false;
		prt->target = ECP_DEFAULT_TARGET;
	}

	return 0;
}

int set_ecp_current_state(size_t dev_id, char reg_id, size_t bit, bool state){

	struct ecproto_device_t* dev = ecp_get_dev_from_id(dev_id);
	if(dev == NULL){
		println("Received ECP device which is not enumerated: %i", 
			ERROR, dev_id);
		return -1;
	}
	
	struct ecproto_port_register_t* reg = ecp_get_reg_from_dev(reg_id, 
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

int ecp_ddir_to_register(struct ecproto_port_register_t* regp, uint8_t reg){

	if(regp == NULL){
		return -1;
	}

	for(size_t i = 0; i < ECP_REG_WIDTH; i++){
		regp->bits[i].ddir = (reg >> i) & 0x1;

	}
	
	return 0;	

}

int ecp_pin_to_register(struct ecproto_port_register_t* regp, uint8_t reg){

	if(regp == NULL){
		return -1;
	}

	for(size_t i = 0; i < ECP_REG_WIDTH; i++){
		regp->bits[i].current = (reg >> i) & 0x1;

	}
	
	return 0;	

}

int ecp_set_device_analog_channel_value(struct ecproto_device_t* dev, 
	uint8_t channel, uint16_t value){
	
	if(dev == NULL){
		return -1;
	}
	
	for(size_t i = 0; i < dev->analog_channel_count; i++){
		struct ecproto_analog_channel_t* channelp = 
			&dev->analog_channels[i];
		if(channelp->id == channel){
			println("ecp analog update: dev %i channel"
				" %i: %i --> %i", DEBUG_MOST, dev->id, channel,
				channelp->value, value);
			channelp->value = value;
			return 0;
		}
	}
	return -1;


}

#endif /* NOEC */
