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

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "mtsp.h"
#include "log.h"
#include "data.h"
#include "serial.h"
#include "game.h"


uint16_t crc_modbus (uint8_t *in, size_t len){

        static const uint16_t crc_table[] = {
        0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
        0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
        0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
        0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
        0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
        0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
        0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
        0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
        0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
        0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
        0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
        0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
        0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
        0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
        0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
        0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
        0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
        0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
        0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
        0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
        0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
        0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
        0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
        0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
        0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
        0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
        0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
        0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
        0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
        0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
        0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040 };
        
        uint8_t temp;
        uint16_t crc_word = 0xFFFF;

        while (len--){

                temp = *in++ ^ crc_word;
                crc_word >>= 8;
                crc_word ^= crc_table[temp];
        }

        return crc_word;

}

int init_mtsp(char* device, int baud){

        
        mtsp_fd = open(device,O_RDWR);
        if(mtsp_fd < 0){
                println("Failed to open device at %s!",ERROR,device);
                return -1;
        }

        if(set_interface_attribs(mtsp_fd,baud) < 0){

                println("Failed to set interface attribs for device at %s!",
                                ERROR,device);
                close(mtsp_fd);
                return -1;
        }

        return 0;
}

void* loop_mtsp(){

        println("MTSP entering operational state",DEBUG);
                
        struct timespec tim, tim2;

       while(!shutting_down){
               
                tim.tv_sec = 0;
                tim.tv_nsec = PATROL_INTERVAL_NS;
                
                if(nanosleep(&tim , &tim2) < 0 ){
                         println("interupt in mtsp loop!! sleep failed!",ERROR);
                 }

                if(update_mtsp_states() < 0){
                        println("Failed to update MTSP devices!!",ERROR);
                }

       }

        println("Stopping MTSP connection. may i never see you again",DEBUG);
        
        return NULL;

}

int mtsp_start(){
        
        if(pthread_create(&mtsp_thread,NULL,loop_mtsp,NULL)){               
                 println("failed to create thread", ERROR);                      
                 return -1;                                                      
         }
        return 0;
}

int update_mtsp_states(){

        for(uint8_t i = 0; i < 256; i ++){
                uint8_t* frame = mtsp_assemble_frame(0x21, 2, &i, 1);
                mtsp_write(frame,7);
                
                free(frame);
        }

        return 0;

}

/* Assembles a message from the given parameters */
uint8_t* mtsp_assemble_frame(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t payload_length){
        
        size_t lentot = payload_length + 6;
        uint8_t * frame = malloc(lentot * sizeof(uint8_t));

        frame[0] = MTSP_START_BYTE;
        frame[1] = slave_id; /* The address of the slave to be addressed */
        frame[2] = lentot; /* The length of the entire frame */
        frame[3] = command_id; /* I would call it action, but whatever */
        memcpy(&frame[4], payload, (sizeof(uint8_t) * payload_length));
        
        uint16_t checksum = crc_modbus(frame, lentot - 2); 
        frame[lentot - 2] = 0b11111111 & checksum;
        frame[lentot - 1] = 0b11111111 & (checksum >> 8);

        return frame;
}

/* Locks the write command and writes to the fd */
int mtsp_write(uint8_t* frame, size_t length){
        
        static int lock = 0;
        while(lock){ /* NOOP */}
        lock = 1;
        int n  = write(mtsp_fd, frame, length * sizeof(uint8_t));
        if (n < length){
                println("Failed to write %i bytes to mtsp device! Returned %i!",
                                ERROR, length, n);
                lock = 0;
                return -1;
        }
        
        return 0;
}

uint8_t* mtsp_receive_message(){

        uint8_t* head,* body;
        head = malloc(3*sizeof(uint8_t));
       
        /* Wait for new message to begin */
        for(int i = 0; i < 200; i++){
                read(mtsp_fd,&head[0],sizeof(uint8_t));
                if(head[0] == MTSP_START_BYTE){
                        /* Found start of new message */
                        break;
                }

        }
        
        if(head[0] != MTSP_START_BYTE){
                println("Failed to read MTSP",ERROR);
                return NULL;
        }
        
        read(mtsp_fd,&head[1],1); /* Slave address */
        read(mtsp_fd,&head[2],1); /* Number of total bytes */
        
        if(head[2] > 256){
                /* WTF? */
                println("MTSP received way too large count of bytes!!! %i", 
                                ERROR, head[2]);
                free(head);
                return NULL;
        }
        
        body = malloc((head[2] - 3) * sizeof(uint8_t));
        for(uint8_t i = 0; i < (head[2] - 3); i++){
                
                read(mtsp_fd, &body[i], 1);

        }

        uint8_t* finmsg = malloc((head[2] + 3) * sizeof(uint8_t));
        
        memcpy(finmsg, head, sizeof(uint8_t) * 3);
        memcpy(finmsg, body, sizeof(uint8_t) *  ( head[2] - 3 ) );

        free(head);
        free(body);

        /* validate CRC sum, if invalid return NULL, but print error message */
        uint16_t crcsum_should = crc_modbus(finmsg,(head[2] - 2));
        uint16_t crcsum_is = 0x0;
        /* Cut off everything beyond 8 bit and write it to the lower half */
        crcsum_is |= finmsg[head[2-1]] & 0x11111111; 
        /* Left shift everything 8 bits and write lower half to the sum */
        crcsum_is = crcsum_is << 8;
        crcsum_is |= finmsg[head[2-2]] & 0x11111111; 
        if(crcsum_is != crcsum_should){
                /* FUCK. CRC mismatch */
                println("CRC MISSMATCH IN MTSP! SHOULD: %x IS: %x... GARBAGE", 
                                crcsum_should, crcsum_is);
                free(finmsg);
                return NULL;
        }
        return finmsg;
}
