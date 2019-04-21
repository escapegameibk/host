/* An EC-Proto implementation for the escape game innsbruck's host
 * Copyright © 2018 tyrolyean
 * 
 * This file contains functions for handleing ecproto frames, parseing them and
 * altering objects accordingly.
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

		case ADC_REG:
		case SET_GPIO_REGS:
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
			
			println("ECP Port update: %i dev %c reg %i bit --> %i",
				DEBUG_MORE,
				recv_frm[ECP_ADDR_IDX], 
				recv_frm[ECP_PAYLOAD_IDX], 
				recv_frm[ECP_PAYLOAD_IDX + 1], 
				recv_frm[ECP_PAYLOAD_IDX + 2]);
			
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
					ecp_get_dev_from_id(
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
				
				println("ECP dev %i notified us of %i regs!", 
					DEBUG_MORE, recv_frm[ECP_ADDR_IDX],
					recv_frm[ECP_PAYLOAD_IDX]
					);
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
					ecp_get_dev_from_id(
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

					println("Reg list dev %i reg %c",
						DEBUG_MORE, recv_frm[ECP_ADDR_IDX],
						recv_frm[ECP_PAYLOAD_IDX + i]);

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
					ecp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);
				
				if(dev == NULL){
					println("Recv 11 from unknown dev! wut?"
						, ERROR);
					return -1;
				}
				
				struct ecproto_port_register_t* reg = 
					ecp_get_reg_from_dev(
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
				println("ECP Port enabled: %i dev %c reg %i bit"
					" --> %i",
					DEBUG_MORE,
					recv_frm[ECP_ADDR_IDX],
					recv_frm[ECP_PAYLOAD_IDX],
					recv_frm[ECP_PAYLOAD_IDX + 1],
					recv_frm[ECP_PAYLOAD_IDX + 2]);

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
			ecp_get_dev_from_id(recv_frm[ECP_ADDR_IDX])
				->analog->value = recv_frm[ECP_PAYLOAD_IDX];
			break;
		
		case ADC_GET2:
			if(recv_payload_len < 2){
				println("Received ecp frame 13 with <2 params",
					ERROR);
				return -1;
			}
			if(ecp_set_device_analog_channel_value(
				ecp_get_dev_from_id(recv_frm[ECP_ADDR_IDX]),
				recv_frm[ECP_PAYLOAD_IDX],
				be16toh(*(uint16_t*)
					&recv_frm[ECP_PAYLOAD_IDX+1])) < 0){
				
				println("ecp analog update failed: dev %i "
					"channel %i: %i", ERROR,
					recv_frm[ECP_ADDR_IDX],
					recv_frm[ECP_PAYLOAD_IDX],
					recv_frm[ECP_PAYLOAD_IDX+1]
					);
			}

			
			

			break;
		case GET_PURPOSE:
		{
			println("Device %i notified us of it's %i "
				"capablilties", DEBUG_MORE, 
				recv_frm[ECP_ADDR_IDX],
				recv_frm[ECP_PAYLOAD_IDX]);
			
			struct ecproto_device_t* dev = ecp_get_dev_from_id(
				recv_frm[ECP_ADDR_IDX]);

			if(dev == NULL){
				println("Received purpose from unknown dev %i!",
					ERROR, recv_frm[ECP_ADDR_IDX]);
				return -1;
			}

			for(size_t i = 0; i < recv_frm[ECP_PAYLOAD_IDX]; i++){
				
				ecp_enable_purpose(dev, 
					recv_frm[ECP_PAYLOAD_IDX + i + 1]);
				

			}

		}
			break;

		case SPECIAL_INTERACT:
		/* This is for special devices which have a non-regular use. */
			
			return ecp_handle_special_interact(recv_frm);
			
			break;

		case GET_GPIO_REGS:
			if(recv_payload_len < 4){
				println("Received ecp frame 19 with <4 params",
					ERROR);
				return -1;
			}
			{
				struct ecproto_device_t* dev = 
					ecp_get_dev_from_id(
					recv_frm[ECP_ADDR_IDX]);
				
				if(dev == NULL){
					println("Recv 11 from unknown dev! wut?"
						, ERROR);
					return -1;
				}
				
				struct ecproto_port_register_t* reg = 
					ecp_get_reg_from_dev(
					recv_frm[ECP_PAYLOAD_IDX], dev);
				
				if(reg == NULL){
					println("Recv 11 with unknown reg! wut?"
						, ERROR);
					return -1;
				}

				return ecp_pin_to_register(reg,
					recv_frm[ECP_PAYLOAD_IDX + 3]);
			}
			break;

		case GET_DISABLED_PINS:
			println("Received disabled pins message! NOT STANDART!",
				ERROR);
			return -1;

		default:
			/* You lazy bastard! */
			println("Unimplemented ECP response: %i!", WARNING,
				recv_frm[ECP_ID_IDX]);
			break;
	}
	return 0;
}

int ecp_enable_purpose(struct ecproto_device_t* const dev, uint8_t purpose){

	switch(purpose){
		
		case SPECIALDEV_GPIO:
			dev->gpio_capable = true;
			println("Device %i: Capable of GPIO!", DEBUG_MORE, 
				dev->id);
			break;
		case SPECIALDEV_MFRC522:
			dev->mfrc522_capable = true;
			println("Device %i: Capable of MFRC522!", DEBUG_MORE, 
				dev->id);
			break;
		case SPECIALDEV_PWM:
			println("Device %i: Capable of PWM!", DEBUG_MORE, 
				dev->id);
			break;
		
		case SPECIALDEV_FAST_GPIO:
			println("Device %i: Capable of fast GPIO!", DEBUG_MORE, 
				dev->id);
			dev->fast_gpio_capable = true;
			break;

		case SPECIALDEV_OLD_ANALOG:
			println("Device %i: Capable of old analog!", DEBUG_MORE, 
				dev->id);
			break;
		case SPECIALDEV_NEW_ANALOG:
			println("Device %i: Capable of new analog!", DEBUG_MORE, 
				dev->id);
			break;

		default:
			println("Unknown ecp purpose received from dev %i: %i",
				WARNING, dev->id, purpose);


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
		case SPECIALDEV_NEW_ANALOG:
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
	
	struct ecproto_device_t* dev = ecp_get_dev_from_id(
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
#endif /* NOEC */
