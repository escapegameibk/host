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
uint8_t* mtsp_receive_message();

pthread_t mtsp_thread;
int mtsp_fd;

#endif
