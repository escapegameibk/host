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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdbool.h>
#include <endian.h>

#include "mtsp.h"
#include "log.h"
#include "data.h"
#include "serial.h"
#include "game.h"
#include "config.h"

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
        0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
};

/* I testet this. It is no longer a modbus crc function, but it calculates the
 * correct "checksum" for communication with other mtsp devices, so i guess 
 * it's fine… fuck those russina guys, they are idiots. use the same 
 * polynominal
 */

uint16_t crc_modbus (uint8_t *in, size_t len){
        
        uint16_t temp;
        uint16_t crc_word = 0xFFFF;

        for(size_t i = 0; i < len; i++){
                temp = ((in[i] & 0xFF) ^ crc_word);
                crc_word = (crc_word >> 8);
                crc_word ^= crc_table[(htobe16(temp) >> 8) & 0xFF];
        }

        return crc_word;

}

/* Add a register to the update pool */
int mtsp_register_register(uint8_t device, uint8_t reg){
        size_t row = 0, column = 0;
        bool found = false;
        
        for(; row <  mtsp_regmap_length; row++){
                if(mtsp_device_register_map[row].id == device){
                        found = true;
                        break;
                }
        }
        mtsp_device_t* entry = NULL;
        if(!found){
                /* New device! Append it to the end */
                mtsp_device_register_map = 
                        realloc(mtsp_device_register_map, ++mtsp_regmap_length
                        * sizeof(mtsp_device_t));
                row = mtsp_regmap_length - 1;
                entry = &mtsp_device_register_map[row];
                entry->regcnt = 0;
                entry->id = device;
                entry->registers = malloc(0);

        }else{
                entry = &mtsp_device_register_map[row];
        }

        found = false;
        for(; column < entry->regcnt; column++){
                if(entry->registers[column] == reg){
                        found = true;
                        break;
                }
        }

        if(!found){
                /* New register! Append it to the end */
                entry->registers = 
                        realloc(entry->registers,
                         ++entry->regcnt);
                entry->registers[entry->regcnt - 1] = reg;
        }
        
        return 0;
}

int init_mtsp(char* device, int baud){

        
        /* Look for all mtsp things declared to be input and write them to their
         * designated location in the register_device_map. This is a bit dirty
         * and will probably be replaced in the not too far away future.
         */
        
        mtsp_regmap_length = 0;
        mtsp_device_register_map = malloc(0);
 
        /* Iterate through all events in the config file */
        for(size_t iterator1 = 0;  iterator1 < json_object_array_length(
                json_object_object_get(config_glob,"events")); iterator1++){
                json_object* dependencies= json_object_object_get(
                        json_object_array_get_idx(json_object_object_get(
                        config_glob,"events"),iterator1), "dependencies");
                if(dependencies == NULL){
                        continue;
                }
                /* Iterate through the dependencies*/
                for(size_t iterator2 = 0; iterator2 < json_object_array_length(
                        dependencies); iterator2++){
                                
                         json_object* dependency = 
                                json_object_array_get_idx(dependencies,
                                        iterator2);
                        const char* module = json_object_get_string(
                                json_object_object_get( dependency, "module"));
                        if(module == NULL){
                                continue;
                        }
                         /* Check wether i am concerned or not */
                        if(strncasecmp(module,"mtsp",strlen(module)) == 0){
                                uint8_t device = json_object_get_int(
                                        json_object_object_get(dependency,
                                                "device"));
                                uint8_t reg = json_object_get_int(
                                        json_object_object_get(dependency,
                                                "register"));
                                if(mtsp_register_register(device, reg) < 0){
                                        println("Failed to add MTSP state",
                                                ERROR);
                                }
                        }
                }
        }

        mtsp_device_states = malloc(0);
        mtsp_device_count = 0;

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

int start_mtsp(){
        
        if(pthread_create(&mtsp_thread,NULL,loop_mtsp,NULL)){               
                 println("failed to create thread", ERROR);                      
                 return -1;                                                      
        }
        
        println("MTSP started",DEBUG);
        return 0;
}

/* This sends update requests to all nodes on the bus to send their input
 * states back to me, reads their messages and processes them.
 */
int update_mtsp_states(){

        for(size_t i = 0; i < mtsp_regmap_length; i++){
                mtsp_device_t *dev = &mtsp_device_register_map[i];
                uint8_t* frame = mtsp_assemble_frame(dev->id, 1, dev->registers
                        , dev->regcnt);
                while(mtsp_fd_lock){}
                mtsp_fd_lock = true;
                mtsp_write(frame,dev->regcnt + MTSP_FRAME_OVERHEAD);
                free(frame);
                uint8_t* reply = mtsp_receive_message();
                if(reply == NULL){
                        println("MTSP failed to get reply from device at %i",
                                WARNING, i);
                        i--;
                        continue;
                }
                
                if(mtsp_process_frame(reply) < 0){
                        
                }
                free(reply);
        }
        
        return 0;

}

/* Assembles a message from the given parameters. the length is always the
 * payload length + MTSP_FRAME_OVERHEAD */
uint8_t* mtsp_assemble_frame(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t payload_length){
        
        size_t lentot = payload_length + 6;
        uint8_t * frame = malloc(lentot * sizeof(uint8_t));

        frame[0] = MTSP_START_BYTE;
        frame[1] = slave_id; /* The address of the slave to be addressed */
        frame[2] = lentot; /* The length of the entire frame */
        frame[3] = command_id; /* I would call it action, but whatever*/
        memcpy(&frame[4], payload, (sizeof(uint8_t) * payload_length));
        
        uint16_t checksum = crc_modbus(frame, lentot - 2);
        frame[lentot - 2] = 0b11111111 & checksum;
        frame[lentot - 1] = 0b11111111 & (checksum >> 8);

        return frame;
}

/* Locks the write command and writes to the fd */
int mtsp_write(uint8_t* frame, size_t length){
        
        static bool lock = false;
        while(lock){ /* NOOP */}
        lock = true;
        int n  = write(mtsp_fd, frame, length * sizeof(uint8_t));
        if (n < length){
                println("Failed to write %i bytes to mtsp device! Returned %i!",
                                ERROR, length, n);
                lock = false;
                return -1;
        }
        lock = false;
        
        return 0;
}

/* This function is thread save, by locking itself when called */
uint8_t* mtsp_receive_message(){
        
        static bool recv_lock = false;
        while(recv_lock){ /* NOP */}
        recv_lock = true;

        uint8_t start = 0;
        for (int i = 0; !((i > 1000) || (start == MTSP_START_BYTE)); i ++){
                read(mtsp_fd, &start, sizeof(uint8_t));
        }

        if(start == 0){
                println("MTSP failed to receive ANYTHING!",DEBUG);
                recv_lock = false;
                return NULL;

        }else if(start != MTSP_START_BYTE){
                println("MTSP received too much garbage!",DEBUG);
                recv_lock = true;
                return NULL;
        }

        uint8_t slave_id = 0, frame_length = 0;
        read(mtsp_fd, &slave_id,sizeof(uint8_t));
        read(mtsp_fd, &frame_length,sizeof(uint8_t));
        
        uint8_t *frame = malloc(frame_length * sizeof(uint8_t));
        frame[0] = start;
        frame[1] = slave_id;
        frame[2] = frame_length;
        
        for(size_t i = 3; i < frame_length; i++){
                uint8_t recv  = 0;
                read(mtsp_fd, &recv, sizeof(uint8_t));
                frame[i] = recv;
        }
        uint16_t crcsum_should = crc_modbus(frame, frame_length - 2);
        uint16_t crcsum_is = ((frame[frame_length - 2] & 0b11111111) << 8) |
                (frame[frame_length - 1] & 0b11111111);
        char* mtsp_printable = malloc(frame_length * 4 + 4);
        mtsp_printable[0] = 0;
        for(int i = 0; i < frame_length; i++){
                char tmp[5];
                sprintf(tmp, "<%x>", frame[i]);
                strcat(mtsp_printable,tmp);
        }
        println(mtsp_printable,DEBUG);
        if(crcsum_is != crcsum_should){
                println("MTSP CRC missmatch! should be %x is %x. dropping frame"
                        ,WARNING, crcsum_should,crcsum_is);
                free(frame);
        }
        
        recv_lock = false;
        return frame; 
}

int mtsp_process_frame(uint8_t* frame){
       
        /* Find the right device status from the array */
        mtsp_device_state* device = NULL;
        bool found = false;
        size_t iterator;
        for(iterator = 0; iterator < mtsp_device_count; iterator++){
                if(mtsp_device_states[iterator].device_id == frame[1]){
                        found = true;
                        break;
                }
        }
        if(!found){
                mtsp_device_states = realloc(mtsp_device_states,
                        (mtsp_device_count + 1) * sizeof(mtsp_device_state));
                device = &mtsp_device_states[mtsp_device_count++];
                device->device_id = frame[1];
        }else{

                device = &mtsp_device_states[iterator];
                
        }
        
        return 0;
}
