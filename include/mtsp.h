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

#ifndef MTSP_H
#define MTSP_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define MTSP_DEFAULT_BAUD B460800
#define MTSP_DEFAULT_PORT "/dev/ttyUSB0"
#define MTSP_START_BYTE 0xBB
#define MTSP_FRAME_OVERHEAD 6
uint16_t crc_modbus(uint8_t * in, size_t len);
int init_mtsp(char* device, int baud);
void* loop_mtsp();
int start_mtsp();
int update_mtsp_states();
uint8_t* mtsp_assemble_frame(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t palyoad_length);
int mtsp_write(uint8_t* frame, size_t length);
void* mtsp_listen();
uint8_t* mtsp_receive_message();
int mtsp_process_frame(uint8_t* frame);


pthread_t mtsp_thread;
bool mtsp_fd_lock;
int mtsp_fd;

/* Be glad this struct exists, before it came to life, there was chaos... */
typedef struct{
        uint8_t id;
        size_t regcnt;
        uint8_t* registers;
} mtsp_device_t;

size_t mtsp_regmap_length;
mtsp_device_t* mtsp_device_register_map;

/* The stuff from before was only for non-active storage of stuff. Now we get
 * to the more funny part where things begin to change
 */

typedef struct{

        uint8_t reg_id;
        unsigned long long int reg_state;
         
} mtsp_register_state;

typedef struct{

        uint8_t device_id;
        size_t register_count;
        mtsp_register_state* register_states;
} mtsp_device_state;

mtsp_device_state* mtsp_device_states;
size_t mtsp_device_count;

#endif
