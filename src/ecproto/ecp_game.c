/* A EC-Proto implementation for the escape game innsbruck's host
 *
 * This file contains game related functions. Game related means that these
 * functions are used to connect the hardware to the abstracted dependencies
 * and triggers used by the host configuration.
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
	sleep(1);
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

	/* Should now be ready. Initialize devices */
	println("INITIALIZING ECP DEVICES. This might take some time!", INFO);
	if(ecp_bus_init()){
		println("Failed to init ECP bus!!!", ERROR);
		return -4;
	}
	
	/* All devices should now be synced up and ready for regular operation
	 */
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

	/* Start the update thread which keeps the synced up ecp devices in sync
	 * Regular operation and execution should now be possible. */

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

			/* TODO: Make new analog system work and update al hosts
			*/
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

#endif /* NOEC */
