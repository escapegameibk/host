/* A controller for a protocol called "MTSP" from some russian guys.
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

#ifndef NOMTSP

#ifndef MTSP_H
#define MTSP_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <json-c/json.h>

#define MTSP_DEFAULT_BAUD B460800
#define MTSP_DEFAULT_PORT "/dev/ttyUSB0"
#define MTSP_START_BYTE 0xBB
#define MTSP_FRAME_OVERHEAD 6
#define MTSP_TIMEOUT_US 5000000 // the read timeout in µs

uint16_t crc_modbus(uint8_t * in, size_t len);
int init_mtsp(char* device, int baud);

int mtsp_init_dependency(json_object* dependency);
void* loop_mtsp();
int start_mtsp();
int update_mtsp_states();
uint8_t* mtsp_assemble_frame(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t palyoad_length);
int mtsp_write(uint8_t* frame, size_t length);
void* mtsp_listen();
uint8_t* mtsp_receive_message();
int mtsp_process_frame(uint8_t* frame);
int mtsp_send_request(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t palyoad_length);

int mtsp_trigger(json_object* trigger);
int mtsp_check_dependency(json_object* dep);

pthread_t mtsp_thread;
int mtsp_fd;

/* ############################################################################
   # DEFINITION OF MTSP STATE STORAGE					      #
   ############################################################################
   # Two seperate structs exist, 1 for a port, 1 for a device. a device has   #
   # several ports. the list of devices is initialized at the beginning by the#
   # init depedency function. the update function asks all existant devices in#
   # the device
   ############################################################################

 */

/* All values are set to this value before the device has really answered it's
 * state
 */
#define MTSP_DEFAULT_STATE 0

/* The stuff from before was only for non-active storage of stuff. Now we get
 * to the more funny part where things begin to change
 */

typedef struct{

        uint8_t address;
        uint32_t state;
         
} mtsp_port_t;

typedef struct{

        uint8_t device_id;
        size_t port_cnt;
        mtsp_port_t* ports;
} mtsp_device_t;

mtsp_device_t* mtsp_devices;
size_t mtsp_device_count;

#endif /* MTSP_H */

#endif /* NOMTSP */
