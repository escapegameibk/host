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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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





#endif /* LOLPROTO */
