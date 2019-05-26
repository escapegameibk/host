/* A EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains input and output functions of the host's ecproto
 * implementation.
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
				println("Found valid data in buffer though:",
					WARNING);
				println(printable_bytes(frame, frame_length),
					WARNING);
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
		if(frame == NULL){
			println("Realloc failed in ecp receive frame!", ERROR);
			frame = NULL;
			break;
		}
		frame[frame_length - 1] = octet;

		/* Frame finished */
		if(frame != NULL && frame_length > ECP_LEN_IDX &&
			frame_length >= frame[ECP_LEN_IDX]){

			if(frame_length < ECPROTO_OVERHEAD){
				char* printable = printable_bytes(frame,
					frame_length);
				println("Received too short frame: %s", ERROR,
					printable);
				free(printable);
				free(frame);
				frame = NULL;
				*len = 0;
				break;
			}
			*len = frame_length;
			break;
		}


	}
	
	char buffer[((256 * 4) + 1)];
	memset(buffer, 0, sizeof(buffer));

	if(printable_bytes_buf(frame, *len, buffer, sizeof(buffer)) == NULL){
		println("failed to get printable bytes for ecp frame!!",
			WARNING);
		pthread_mutex_unlock(&ecp_readlock);
		return frame;
	}else{
	
		println("ECP Received: %s", DEBUG_MOST, buffer);	

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
				char * prnt = printable_bytes(snd_frame,
					snd_len);
				println(prnt, ERROR);
				free(prnt);
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


	struct ecproto_device_t* dev = ecp_get_dev_from_id(device_id);

	if(dev == NULL){
		println("Port write to unknown device! ID: %i", ERROR,
			device_id);
		return -1;
	}

	struct ecproto_port_register_t* reg = ecp_get_reg_from_dev(reg_id,
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

	char * prnt = printable_bytes(frame, frame[ECP_LEN_IDX]);
	println("ECP Transmitting: %s", DEBUG_MOST, prnt);
	free(prnt);

	if(!validate_ecp_frame(frame, frame[ECP_LEN_IDX])){
		println("Failed to construct ecp frame!", ERROR);
		return -1;
	}

	if(frme != NULL){
		*frame_length = frame[ECP_LEN_IDX] * sizeof(uint8_t);
		*frme = malloc(frame[ECP_LEN_IDX] * sizeof(uint8_t));
		if(*frme == NULL){
			println("Failed to allocate memory for frame copy in "
				"ecp write!", ERROR);
			return -1;
		}
		memcpy(*frme, frame, sizeof(uint8_t) * *frame_length);
	}

	int n = write(fd, frame, frame[ECP_LEN_IDX]);
	return n;
}

#endif /* NOEC */
