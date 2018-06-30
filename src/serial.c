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

 char* regmap[] = {
         "A","B","C","D","E","F","G","H","J","K","L",NULL
 };

int init_serial(char* port){

#define INT_LEN 256
        char*debug = malloc(INT_LEN);
        memset(debug, 0, INT_LEN);

        serial_buflen = 0;
        serial_buf = malloc(serial_buflen);
        
        serial_fd = open(port,O_RDWR);

        serial_port = malloc(strlen(port));
        strcpy(serial_port, port);

        if(serial_fd < 0){
                sprintf(debug, "failed to open serial connection at port %s",
                                port);
                println(debug,ERROR);
                free(debug);
                return -1;
        }

        if(set_interface_attribs(serial_fd, SERIAL_BAUD) < 0){
                
                sprintf(debug, "failed to set attributes for serial connection\
                                at port %s",
                                port);
                println(debug,ERROR);
                free(debug);
                return -2;
        }
        
        /* count the registers */
        for(size_t i = 0;;i++){
                if(regmap[i] == NULL){
                        ser_regcnt = i;
                        break;
                }
        }

        ser_registers = malloc(sizeof(uint8_t) * ser_regcnt);

        return 0;
}

int serial_ready = 0;
int start_serial(){
        
        println("starting serial connection handler",DEBUG);
       
        if(pthread_create(&serial_thread,NULL,loop_serial,NULL)){
                println("failed to create thread", ERROR);
                return -1;
        }

        /* Stand by for a bit until the receiver is ready and send a init 
         * request to the µc
         */

        while(!serial_ready){/* NOOP*/}
        
        char ini[] = {'2',CMD_DELIMITER};
        write(serial_fd, ini,2);
        return 0;

}

void* loop_serial(){
       
        println("serial connection entering operational state",DEBUG);
        serial_ready = 1;

        while(!shutting_down){
                /* Read input char by char and wait for the termination char */
                char in;
                if(read(serial_fd, &in, 1) < 0){
                        /* the connection no longer exists probably */
                        println("failed to receive input from the serial \
                                        connection! resetting!", ERROR);
                        close(serial_fd);
                        while(0 > init_serial(serial_port) ){
                                sleep(1);
                        }
                }else{
                        serial_buf = realloc(serial_buf,++serial_buflen);
                        serial_buf[serial_buflen-1] = in;
                        
                        if(in == CMD_DELIMITER){
                                
                                /* command completely received */
                                /* remove cmd delimiter char */
                                serial_buf[serial_buflen-1] = 0; 
                                println("received command:",DEBUG);
                                println(serial_buf,DEBUG);
                                
                                /* process the command */
                                if(process_cmd(serial_buf) < 0){
                                        println("Failed to process command!",
                                                        ERROR);
                                }
                                /* clean the buffer */
                                free(serial_buf);
                                serial_buflen = 0;
                                serial_buf = malloc(serial_buflen);
                        }
                }

        }

        close(serial_fd);
        println("stopped serial connection", DEBUG);

        return NULL;

}

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


int process_cmd(char* cmd){
        
        char* delimf = strchr(cmd,PARAM_DELIMITER);
        int action = 0;
        if (delimf == NULL){
                /* A parameterless command... interresting */
        }else{
                *delimf = '\0';
        }

        action = atoi(cmd);

        switch (action) {
        case 0:
              /* If compiled for debug, ignore, otherwise throw error */
#ifndef DEBUG
                println("Recieved debug without debug option %i ", ERROR, action);
#endif
                break;
        case 3:
                
                println("Received update statement.",DEBUG);
                if(update_register(cmd) < 0){
                        println("Failed to process update!!", ERROR);
                }
                break;
        default:
                println("Invalid action received via serial: %i",ERROR, action);
        }

        return 0;

}

int update_register(char* params){

        *strchr(params,PARAM_DELIMITER) = '\0';
        size_t reg = 0;
        
        for(size_t i = 0; i < ser_regcnt; i++){
                if(strcmp(params,regmap[i]) == 0){
                        reg = i;
                        break;
                }else if(i == ser_regcnt){
                        /* This shouldn't really happen. */
                        return -1;
                }
        }

        if(reg == ser_regcnt){
                println("Received invalid register!",ERROR);
                return -1;
        }
        
        char* end_of_bit = strchr(params + (strlen(params) + 1),PARAM_DELIMITER);
        *end_of_bit= '\0';
        int bit = atoi(params + (strlen(params)+1));
        int value = atoi(end_of_bit + 1);
        
        println("Updated register %s bit %i to value %i",DEBUG, params, bit, value);
        
        uint8_t* target = &ser_registers[reg];

        if(value == 0){
                value &= ~(0x1 << bit);
        }else{
                value |= 0x1 << bit ;
        }
       
        *target = value;

        return 0;
}

