/* A LOL-Proto implementation for the escape game innsbruck's host
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

#ifndef NOLOL

#include "lolproto.h"
#include "data.h"
#include "config.h"
#include "log.h"
#include "tools.h"
#include "serial.h"

#include <json-c/json.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

uint8_t lol_start_sequence[3] = {'L','O','L'};

int init_lol(){

	println("Initializing LOL-Proto", DEBUG);
	
	pthread_mutex_init(&lol_lock, NULL);
	json_object* port = json_object_object_get(config_glob, "lol_port");
	const char* port_name;
	
	if(port== NULL){
		port_name = LOL_DEFAULT_PORT;
		println("LOL device not defined. assumeing default", WARNING);
	}else{
		port_name = json_object_get_string(port);
	}
	int baud;
	json_object* baud_ob = json_object_object_get(config_glob, "lol_baud");
	if(baud_ob == NULL){
		baud = LOL_DEFAULT_BAUD;
	}else{
		baud = get_baud_from_int(json_object_get_int(baud_ob));
	}

	println("Initialising LOL at port %s", DEBUG, port_name);

	lolfd = open(port_name, O_RDWR);
	if(lolfd < 0){
		println("Failed to open lolfd port. Is it really there?", 
			ERROR);
		return -1;
	}

	if(set_interface_attribs(lolfd, baud) < 0){
		println("Failed to set lolfd attribs!", ERROR);
		close(lolfd);
		return -2;
	}

	struct termios termios;
	tcgetattr(lolfd, &termios);
	termios.c_lflag &= ~ICANON; /* Set non-canonical mode */
	termios.c_cc[VTIME] = 1; /* Set timeout of 0.1 seconds */
	termios.c_cc[VMIN] = 0; /* Set timeout of 0.1 seconds */
	tcsetattr(lolfd, TCSANOW, &termios);

	
	


	println("LOLProto init done", DEBUG);
	return 0;
}

int start_lol(){

	pthread_t lol_watch_thread;

	if(pthread_create(&lol_watch_thread,NULL,lol_listen_fordata,NULL)){
		println("Failed to create lol thread!", ERROR);
		return -1;
	}

	return 0;
}

int init_lol_dependency(json_object* dependency){

	if(dependency == NULL) {
		println("LOL received NULL dependency to init!", ERROR);
		return -1;
	}

	// TODO

	return 0;
}

void* lol_listen_fordata(){

	while(!shutting_down){
		if(wait_for_data(0, lolfd) >= 0 ){
			/* Frame incoming */
			struct lol_frame_t frame;

			if(recieve_lol_frame(&frame) < 0){
				println("Failed to retrieve lol frame!", ERROR);
				continue;
			}

			/* Print frame */
			char* print_frame = printable_bytes(frame.payload, 
				frame.len);
			
			println("LOL FRAME RECEIVED:", DEBUG_MOST);
			println("ADD %x: %s", DEBUG_MOST, frame.address, 
				print_frame);

			free(print_frame);

			/* Process frame */
			// TODO
		}
	}

	println("Lol leaving receiving state", INFO);

	return 0;
}

/*
 * This function reads a complete lol frame from the currently open lol device.
 * The lol-device is required to have data in it's buffer for this operation to
 * succeed. Otherwise it will time out.
 */
int recieve_lol_frame(struct lol_frame_t* frame){

	if(frame == 0){
		println("Passed NULL to lol recv", ERROR);
		return -1;
	}

	memset(frame, 0 , sizeof(struct lol_frame_t));

	pthread_mutex_lock(&lol_lock);

	uint8_t start = 0;
	for (int i = 0; !((i > 1000) || (start == lol_start_sequence[0])); 
		i ++){
		ssize_t rd = read(lolfd, &start, sizeof(uint8_t));
		
		if(rd < 1){
			println("lol fd read < 1! possibly timeout!", WARNING);
			pthread_mutex_unlock(&lol_lock);
			return -1;
		}
	}

	/* Successfully received first byte. Should now receive rest of the
	 * start sequence
	 */

	uint8_t start_sequence[sizeof(lol_start_sequence)] = {start};

	ssize_t rd = read(lolfd, &start_sequence[1], sizeof(start_sequence) - 
		sizeof(start));

	/* Sizeof apparently returns a size_t, but rd is an int and can be <0
	 * ... I've actually never encountered this before. I'll just cast it
	 * a ssize_t
	 */
	if(rd <= (ssize_t)(sizeof(start_sequence) - sizeof(start))){
		println("Failed to read continuation of lol start sequence!",
			ERROR);
		pthread_mutex_unlock(&lol_lock);
		return -2;
	}
	
	/* Now read the status byte. It's structured as follows:
	 *
	 * | SIZE | 0 | ADDR0 | ADDR1 | ADDR2 | ADDR3 | ADDR4 | ADDR5 |
	 *
	 */

	uint8_t status = 0;
	
	rd = read(lolfd, &status, sizeof(status));

	if(rd <= 0){
		println("Failed to read lol status byte!", ERROR);
		pthread_mutex_unlock(&lol_lock);
		return -3;
	}

	bool sb = (status >> 7) & 0x1;
		size_t len = LOLPROTO_OVERHEAD; 
	if(sb){
		len += LOLPROTO_LONG_PAY;
	}else{
		len += LOLPROTO_SHORT_PAY;

	}

	uint8_t address = (status & 0b111111);
	println("lol-frame recv from %x", DEBUG_MOST, address);

	uint8_t * frame_raw = malloc(len);
	memset(frame_raw, 0, len);
	rd = read(lolfd, frame_raw, len - (sizeof(lol_start_sequence) + 1));

	if(rd <= 0){
		println("Failed to receive lol payload", ERROR);
		free(frame_raw);
		pthread_mutex_unlock(&lol_lock);
		return -3;
	}

	/* The raw frame should now be complete. Constructing whole frame */

	frame->len = len;
	frame->address = address;
	frame->payload = frame_raw;
	
	pthread_mutex_unlock(&lol_lock);

	return 0;

}

#endif /* LOLPROTO */
