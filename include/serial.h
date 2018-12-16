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

#include <termios.h>
#include <poll.h>
#include <sys/types.h>

int set_interface_attribs(int fd, int speed);

/* Returns -1 on error, 0 on timeout and >0 on success */
int wait_for_data(int, int fd);

uint16_t ibm_crc(uint8_t* data, size_t len);

#endif
