/* Serial interface for a connection to a controller
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

#ifndef SERIAL_H
#define SERIAL_H

#include <pthread.h>
#include <termios.h>
#include <stdint.h>
#define CMD_DELIMITER 0x0a
#define PARAM_DELIMITER 0x3B

#define SERIAL_BAUD B115200
#define DEFAULT_SERIAL_PORT "/dev/ttyACM0"

int init_serial(char* port);
int start_serial();
void* loop_serial();
int set_interface_attribs(int fd, int speed);
int process_cmd(char* cmd);
int update_register(char* params);

char * serial_buf;
size_t serial_buflen;
int serial_fd;
char* serial_port;

pthread_t serial_thread;

uint8_t* ser_registers;
size_t ser_regcnt;

/* A null-pointer terminated string array */
char* regmap[] = {
        "A","B","C","D","E","F","G","H","J","K","L",NULL
};    
#endif
