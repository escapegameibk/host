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

#define MTSP_DEFAULT_BAUD B460800
#define MTSP_DEFAULT_PORT "/dev/ttyUSB0"
#define MTSP_START_BYTE 0xBB

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
pthread_t mtsp_thread;
int mtsp_fd;

typedef struct{
        uint8_t id;
        size_t regcnt;
        uint8_t* registers;
} mtsp_device_t;

/* Even though this seems idiotic, i have to do this to speed things up. this
 * array contains all devices and their registers needed in the entire program.
 * It is allocated in the init process and is never altered afterwards. The
 * first dimension contains pointers 
 * The length tells you, how many devices exist. The first int in every row
 * represents the amount of registers existant, the second one is the device id,
 * which is followed by the registers.*/
size_t mtsp_regmap_length;
mtsp_device_t* mtsp_device_register_map;

#endif
