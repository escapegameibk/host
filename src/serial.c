/* Serial interface for a connection to a controller.
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

#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "log.h"
#include "serial.h"
#include "data.h"

/* Basically diables all the tty stuff you don't want on a data-connection,
 * because you don't want escape sequences inside of a fucking serial data
 * connection!
 */
int set_interface_attribs(int fd, int speed){

        struct termios tty;
#define INT_LEN 256
        char* debug = malloc(INT_LEN);
        memset(debug,0,INT_LEN);
        if (tcgetattr(fd, &tty) < 0) {
                sprintf(debug, "Error from tcgetattr: %s", strerror(errno));
                println(debug, ERROR);
                free(debug);
                return -1;
        }

        cfsetospeed(&tty, (speed_t)speed);
        cfsetispeed(&tty, (speed_t)speed);

        tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;         /* 8-bit characters */
        tty.c_cflag &= ~PARENB;     /* no parity bit */
        tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
        tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */
        
        /* setup for non-canonical mode */
        tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        tty.c_oflag &= ~OPOST;
        
        /* fetch bytes as they become available */
        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 1;

        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
                memset(debug,0,INT_LEN);
                sprintf(debug, "Error from tcsetattr: %s\n", strerror(errno));
                println(debug,ERROR);
                free(debug);
                return -2;
        }

#undef INT_LEN

        return 0;
}


