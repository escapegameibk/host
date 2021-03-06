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
#include <sys/select.h>
#include <time.h>

#include "mtsp.h"
#include "log.h"
#include "data.h"
#include "serial.h"
#include "game.h"
#include "config.h"
#include "tools.h"

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

struct mtsp_device_t* mtsp_devices = NULL;
size_t mtsp_device_count = 0;

uint16_t crc_modbus (uint8_t *in, size_t len){
        
        uint16_t temp;
        uint16_t crc_word = 0xFFFF;

        for(size_t i = 0; i < len; i++){
                temp = ((in[i] & 0xFF) ^ crc_word);
                crc_word = (crc_word >> 8);
                crc_word ^= crc_table[temp & 0xFF];
        }

        return crc_word;

}

/* Add a register to the update pool / register a port of a device in the
 * update pool
 * This function may ONLY be used locally. It is therefore not included in the
 * header file
 */
int mtsp_register_register(uint8_t device, uint8_t reg){
        struct mtsp_device_t* dev = NULL;
	for(size_t i = 0; i <  mtsp_device_count; i++){
                if(mtsp_devices[i].device_id == device){
			dev = &mtsp_devices[i];
			break;
                }
        }

        if(dev == NULL){
                /* New device! Append it to the end */
		mtsp_devices = realloc(mtsp_devices, ++mtsp_device_count * 
			sizeof(struct mtsp_device_t));
		dev = &mtsp_devices[mtsp_device_count - 1];
		dev->device_id = device;
		dev->port_cnt = 0;
		dev->ports = NULL;

        }

	struct mtsp_port_t* port = NULL;
        for(size_t i = 0; i < dev->port_cnt; i++){
		if(dev->ports[i].address == reg){
			port = &dev->ports[i];
			break;
		}
        }

        if(port == NULL){
                /* New register! Append it to the end */
		dev->ports = realloc(dev->ports, ++(dev->port_cnt) * 
			sizeof(struct mtsp_port_t));
		port = &dev->ports[dev->port_cnt - 1];
		port->address = reg;
		port->state = MTSP_DEFAULT_STATE;

        }else{
		/* The device does already exist. Ignore it and return 1*/
		return 1;
	}
        
        return 0;
}

int init_mtsp(){
	
	pthread_mutex_init(&mtsp_lock, NULL);
	pthread_mutex_init(&mtsp_lock_transmission, NULL);

	/* Attempt to get mtsp config from config file, otherwise use defaults 
	 */
	const char* device = MTSP_DEFAULT_PORT;

	json_object* config_dev = json_object_object_get(config_glob, 
		"mtsp_device");

	if(config_dev != NULL){
		device = json_object_get_string(config_dev);
		println("Configurative update of mtsp device: %s", DEBUG,
			device );
	}
	
	int baud = MTSP_DEFAULT_BAUD;
	
	json_object* config_baud = json_object_object_get(config_glob, 
		"mtsp_baud");

	if(config_baud != NULL){
		int baud_tmp = json_object_get_int(config_baud);
		baud = get_baud_from_int(baud_tmp);
		println("Configurative update of mtsp baud rate: %i", DEBUG,
			baud_tmp);
	}
	
	mtsp_devices_populated = false;

        println("Added a total of %i mtsp input devices:",DEBUG, 
                mtsp_device_count);
        for(size_t i = 0; i < mtsp_device_count; i++){
                println("\t%i: %i : %i",DEBUG, i, mtsp_devices[i].device_id,
			mtsp_devices[i].port_cnt);
		/* Iterate through registers and output them */
		for(size_t j = 0; j < mtsp_devices[i].port_cnt; j++){
			println("\t\t%i: %i", DEBUG, j,
			mtsp_devices[i].ports[j].address);
		}
        }
       
       /* initialize the serial device */
        
        mtsp_fd = open(device,O_RDWR);
        if(mtsp_fd < 0){
                println("Failed to open device at %s!",ERROR,device);
                mtsp_fd = 0;
                return -1;
        }

        if(set_interface_attribs(mtsp_fd,baud) < 0){

                println("Failed to set interface attribs for device at %s!",
                                ERROR,device);
                close(mtsp_fd);
                mtsp_fd = 0;
                return -2;
        }

        /* Set the timeout for the mtsp devices to reply*/
        struct termios termios;
        tcgetattr(mtsp_fd, &termios);
        termios.c_lflag &= ~ICANON; /* Set non-canonical mode */
        termios.c_cc[VTIME] = 1; /* Set timeout of 0.1 seconds */
        termios.c_cc[VMIN] = 0; /* Set timeout of 0.1 seconds */
        tcsetattr(mtsp_fd, TCSANOW, &termios);

	if(mtsp_send_notify() < 0){
		
		println("Failed to send MTSP notify to all devices!!", ERROR);
		return -3;
	}

        return 0;
}

int mtsp_init_dependency(json_object * dependency){

	mtsp_devices_populated = false;

	uint8_t device_id = json_object_get_int(json_object_object_get(
		dependency,"device"));
	
	uint8_t port_id = json_object_get_int(json_object_object_get(
		dependency,"register")) & 0xFF;

	if(mtsp_register_register(device_id, port_id) < 0){
		println("Failed to register MTSP port %i at device %i!",
			ERROR, port_id, device_id);
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

        if(mtsp_fd == 0){
                println("Apparently error handling isn't your thing eh?",INFO);
                
                return -1;
        }

        if(pthread_create(&mtsp_thread,NULL,loop_mtsp,NULL)){               
                 println("failed to create thread", ERROR);                      
                 return -2;                                                      
        }
        
        println("MTSP started",DEBUG);
        return 0;
}

/* This function brings the mtsp stuff into an initial state in order to avoid
 * any errors.
 */
int reset_mtsp(){

	if(mtsp_send_notify() < 0){

		println("MTSP device notify failed!!", ERROR);
		return -1;

	}

	return 0;
}

/* This sends update requests to all nodes on the bus to send their input
 * states back to me, reads their messages and processes them.
 */
int update_mtsp_states(){
        
	for(size_t i = 0; i < mtsp_device_count; i++){
                struct mtsp_device_t *dev = &mtsp_devices[i];

		uint8_t* regs = malloc(dev->port_cnt);
		if(regs == NULL){
			println("Failed to allocate memory for mtsp regs!",
				ERROR);
			return -1;
		}
		for(size_t prt = 0; prt < dev->port_cnt; prt++){
			regs[prt] = dev->ports[prt].address;
		}

                int n = mtsp_send_request(dev->device_id, 1, regs
                        , dev->port_cnt);

		free(regs);
                
                if(n < 0){
                        println("MTSP failed to update device %x",
                                WARNING, dev->device_id);
                        i--;
                        continue;
                }
                
        }

	mtsp_devices_populated = true;
        
        return 0;

}

/* Assembles a message from the given parameters. the length is always the
 * payload length + MTSP_FRAME_OVERHEAD */
uint8_t* mtsp_assemble_frame(uint8_t slave_id, uint8_t command_id, 
        uint8_t* payload, size_t payload_length){
        
        size_t lentot = payload_length + 6;
        uint8_t * frame = malloc(lentot * sizeof(uint8_t));
	if(frame == NULL){
		println("Failed to allocate ram for new mtsp frame!", ERROR);
		return NULL;
	}
        frame[0] = MTSP_START_BYTE;
        frame[1] = slave_id; /* The address of the slave to be addressed */
        frame[2] = lentot; /* The length of the entire frame */
        frame[3] = command_id; /* I would call it action, but whatever*/
        memcpy(&frame[4], payload, (sizeof(uint8_t) * payload_length));
        
        uint16_t checksum = crc_modbus(frame, lentot - 2);
        /* Please note that the checksum is inverted! These idiots and byte
         * ordering... */
        frame[lentot - 2] = 0b11111111 & (checksum >> 8);
        frame[lentot - 1] = 0b11111111 & checksum;

        return frame;
}

/* Locks the write command and writes to the fd */
int mtsp_write(uint8_t* frame, size_t length){

	pthread_mutex_lock(&mtsp_lock);        

	/* wait to give MTSP devices time to react */
	struct timespec sleeper;
	sleeper.tv_sec = 0;
	sleeper.tv_nsec = 5000;
	nanosleep(&sleeper, NULL);

        size_t n  = write(mtsp_fd, frame, length * sizeof(uint8_t));
        if (n < length){
                println("Failed to write %i bytes to mtsp device! Returned %i!",
                                ERROR, length, n);
		pthread_mutex_unlock(&mtsp_lock);        
                return -1;
        }
	
	pthread_mutex_unlock(&mtsp_lock);
        return 0;
}

/* This function is thread save, by locking itself when called */
uint8_t* mtsp_receive_message(){

	pthread_mutex_lock(&mtsp_lock);        

        uint8_t start = 0;
        for (int i = 0; !((i > 1000) || (start == MTSP_START_BYTE)); i ++){
                  
                int rd = read(mtsp_fd, &start, sizeof(uint8_t));
                if(rd < 1){
                        println("Failed to read for mtsp! possibly timeout",
                                WARNING);
			pthread_mutex_unlock(&mtsp_lock);        
                        return NULL;
                }
        }

        if(start == 0){
                println("MTSP failed to receive ANYTHING!",DEBUG);
		pthread_mutex_unlock(&mtsp_lock);        
                return NULL;

        }else if(start != MTSP_START_BYTE){
                println("MTSP received too much garbage!",DEBUG);
		pthread_mutex_unlock(&mtsp_lock);        
                return NULL;
        }

        uint8_t slave_id = 0, frame_length = 0;
        read(mtsp_fd, &slave_id,sizeof(uint8_t));
        read(mtsp_fd, &frame_length,sizeof(uint8_t));
        
        if(frame_length < MTSP_FRAME_OVERHEAD){
                println("too small frame length %i",DEBUG, frame_length);
		pthread_mutex_unlock(&mtsp_lock);        
                return NULL;
        }

        uint8_t *frame = malloc(frame_length * sizeof(uint8_t));
	if(frame == NULL){
		println("Failed to allocate ram for MTSP frame recv!", ERROR);
		return NULL;
	}
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
        if(crcsum_is != crcsum_should){
                println("MTSP CRC missmatch! should be %x is %x. dropping frame"
                        ,WARNING, crcsum_should,crcsum_is);
                free(frame);
                frame = NULL;
        }
	pthread_mutex_unlock(&mtsp_lock);        
        
        return frame; 
}

int mtsp_process_frame(uint8_t* frame){
       
        /* Throw away responses to write calls */
        if(frame[3] == 2){
                return 0;
        }

        /* Find the right device status from the array, if not found, something
	 * has gone terribly wrong. */
        struct mtsp_device_t* device = NULL;
        for(size_t iterator = 0; iterator < mtsp_device_count; iterator++){
                if(mtsp_devices[iterator].device_id == frame[1]){
			device = &mtsp_devices[iterator];
                        break;
                }
        }

	if(device == NULL){
		println("Got invalid mtsp device %i!", WARNING, frame[1]);
		return -1;
	}

	/* Iterate through registers and set states accordingly*/

	for(int iterator = 4; iterator < (frame[2]  - 2);){
		uint8_t address = frame[iterator];
		uint32_t value = 0;

		/* Read the correct amount of bytes from the frame */
		if( address < 0x64){
			value = frame[iterator+1] & 0xFF;
			/* Move itarator to next position */
			iterator += 2;
			
		}else if( address < 0xC8){
			value = ((frame[iterator+1] & 0xFF) << 8) | 
				(frame[iterator+2] & 0xFF);
			iterator += 3;
		}else{
			value = ((frame[iterator+1] & 0xFF) << 24) | 
				((frame[iterator+2] & 0xFF) << 16) |
				((frame[iterator+3] & 0xFF) << 8) |
				(frame[iterator+3] & 0xFF);
			iterator += 5;
		}

		struct mtsp_port_t* port = NULL;
		/* Save the return values to their apropriate addresses */
		for(size_t i = 0; i < device->port_cnt; i++){
			if(device->ports[i].address == address){
				port = &device->ports[i];
				break;
			}
		}

		if(port == NULL){
			println("Recieived invalid mtsp port %i on device %i",
				ERROR, port->address, device->device_id);
		}
		port->state = value;
	}
        
        return 0;
}

/* This function sends a mtsp frame and waitd for a response. on success >=0 is
 * returned, on error < 0 is returned.
 */
int mtsp_send_request(uint8_t slave_id, uint8_t command_id, uint8_t* payload,
        size_t payload_length){
        
        /* Assemble a frame */
        uint8_t* request =  mtsp_assemble_frame(slave_id,command_id, payload, 
                payload_length);
	if(request == NULL){
		println("Failed to generate MTSP request!!", ERROR);
		return -1;
	}
	
	pthread_mutex_lock(&mtsp_lock_transmission);
 
#define ERROR_THRESHOLD 20

	int return_value = -1;
	uint8_t* reply = NULL;

	/* This will try to send the message to the device until ERROR_THRESHOLD
	 * is reached. in that case, it will cancle and error out. 
	 */
	for(size_t i = 0; i < ERROR_THRESHOLD; i++){
		
		/* Send my request */
		int n = mtsp_write(request, payload_length + MTSP_FRAME_OVERHEAD);
        
		if ( n < 0){
			/* Write failed. retry */
			println("Failed to write to mtsp device %i!",
				WARNING, slave_id);
			return_value = -1;
		}
        
		reply = mtsp_receive_message();
        
		if(reply == NULL){
			println("Failed to get reply from mtsp device %i!",
				WARNING, slave_id);
			sleep_ms(100);
			return_value = -2;
			continue;
		}
        
		n = mtsp_process_frame(reply);
        
		if( n < 0){
			println("Failed to get VALID reply from mtsp device %i",
				WARNING, slave_id);
			sleep_ms(100);
			return_value = -3;
			continue;
		}

		return_value = 0;
		break;

	}


        free(request);
        free(reply);
	
	pthread_mutex_unlock(&mtsp_lock_transmission);
        return return_value;

}

/* Execute a mtsp trigger */
int mtsp_trigger(json_object* trigger, bool dry){
        
        uint8_t slave_id = json_object_get_int(json_object_object_get(trigger,
                "device"));
        uint8_t payload[2];

        payload[0]= json_object_get_int(json_object_object_get(trigger,
                "register"));

        payload[1] = json_object_get_int(json_object_object_get(trigger,
                "target"));
        int i = 0;
        for(; i < 10; i ++){
		if(dry){
			break;
		}

                if(mtsp_send_request(slave_id,2,payload,2) >= 0){
                        i = 0;
                        break;
                }
        }
        

        if(i != 0){
                return -1;
        }       
	
        const char* type = json_object_get_string(json_object_object_get(
                trigger, "type"));
        if(type == NULL || (strcasecmp(type,"permanent") == 0)){
                /* Leave unchanged*/
                return 0;
        }
        /* Execute a burst */
        if(strcasecmp(type,"burst") == 0){
                /* To make the json more readable the time is specified in ms */
                sleep_ms(json_object_get_int64(
                        json_object_object_get(trigger, "duration")));
                payload[1] = !payload[1];
                
                for(; i < 10; i ++){
			if(dry){
				break;
			}
                        if(mtsp_send_request(slave_id,2,payload,2) >= 0){
                                i = 0;
                                break;
                        }
                }

                if(i != 0){
                        return -1;
                }
        }
        return 0;
}

/* Checks wether a dependency is fullfilled.
 * Returns <0 on error, 0 if not and > 0 if is fullfilled
 */
int mtsp_check_dependency(json_object* dep, float* percentage){

	/* If the devices have not been populated yet don't trigger! */
	if(!mtsp_devices_populated){
		if(percentage != NULL){
			*percentage = 0;
		}
		return 0;
	}


	/* Due to the fact that the target is UNSIGNED and the return value
	 * of get_int is int32_t i precausiously convert it to 64 bit and strip
	 * the upper 32 bit.
	 */
	uint32_t target = (json_object_get_int64(json_object_object_get(dep,
		"target")) & 0xFFFFFFFF);
	
	uint8_t device_id = (json_object_get_int64(json_object_object_get(dep,
		"device")) & 0xFF);
	
	uint8_t port_id = (json_object_get_int64(json_object_object_get(dep,
		"register")) & 0xFF);

	if(device_id == 0 ){
		if(percentage != NULL){
			*percentage = -1;
		}
		return -1;
	}

	struct mtsp_device_t *device = NULL;
	for(size_t dev_i = 0; dev_i < mtsp_device_count; dev_i++){
		if(mtsp_devices[dev_i].device_id == device_id){
			device = &mtsp_devices[dev_i];
			break;
		}
	}

	if(device == NULL){
		/* The device has not replied yet */
		if(percentage != NULL){
			*percentage = 0;
		}
		return 0;
	}

	/* Search for the register in the device */
	struct mtsp_port_t* port = NULL;
	for(size_t reg = 0; reg < device->port_cnt; reg++){
		if(device->ports[reg].address == port_id){
			port = &device->ports[reg];
		}
	}

	if(port == NULL){
		/* We never got a reply from that port */
		if(percentage != NULL){
			*percentage = 0;
		}
		return 0;
	}
		
	*percentage = (port->state == target);

	return port->state == target;


}

int mtsp_send_notify(){
        /* I have to send a notice to all MP3 / RFID devices on how many
	 * devices are attached to them. I have to count all ports which are
	 * above 199 or C7 and send a count to the device to port 0xA.
         */

	 for(size_t i = 0; i < mtsp_device_count; i++){

		struct mtsp_device_t* dev = &mtsp_devices[i];
		uint8_t register_large_count = 0;

		for(size_t j = 0; j < dev->port_cnt; j++){
			if(dev->ports[j].address >= 200){
				register_large_count++;
			}
		}

		/* Send the device a notice */
		if(register_large_count > 0){
			uint8_t payload[] = {0xA, register_large_count};
			if(mtsp_send_request(dev->device_id, 2, payload, 
					sizeof(payload)) < 0){

				println("Failed to notice mtsp device %i of \
it's rfid count!", ERROR, dev->device_id);
				/* I don't want to error out at this point, but
				 * it seems that i have to */
				 return -1;
			}else{
				println("Notified mtsp device %i of it's %i \
RFID/MP3 Devices",
					DEBUG, dev->device_id, register_large_count);
			}
		}
	 }


	return 0;

}

#endif /* NOMTSP */

