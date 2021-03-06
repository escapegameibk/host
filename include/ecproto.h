/* A EC-Proto implementation for the escape game innsbruck's host
 *
 * This file is as of now the central header file for the ECPROTO source files.
 * All function declarations should be in here.
 *
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

#ifndef NOEC

#ifndef ECPROTO_H
#define ECPROTO_H

#include <stdint.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <pthread.h>

#define ECP_REG_WIDTH 8
#define ECPROTO_OVERHEAD 6

#define INIT_ACTION            0x00
#define REQ_SEND               0x01
#define SEND_NOTIFY            0x02
#define ENUMERATEACTION        0x03
#define REMOTE_COMMAND         0x04
#define DEFINE_PORT_ACTION     0x05
#define GET_PORT_ACTION        0x06
#define WRITE_PORT_ACTION      0x07
#define ERROR_ACTION           0x08
#define REGISTER_COUNT         0x09
#define REGISTER_LIST          0x0A
#define	PIN_ENABLED            0x0B
#define SECONDARY_PRINT        0x0C
#define ADC_GET                0x0D
#define ADC_REG                0x0E
#define ADC_GET2               0x0F
#define GET_PURPOSE            0x10
#define SPECIAL_INTERACT       0x11
#define SET_PWM                0x12
#define GET_GPIO_REGS          0x13
#define SET_GPIO_REGS          0x14
#define GET_DISABLED_PINS      0x15

#define ECP_LEN_IDX 0
#define ECP_ADDR_IDX 1
#define ECP_ID_IDX 2
#define ECP_PAYLOAD_IDX 3

#define SPECIALDEV_GPIO        0x00
#define SPECIALDEV_OLD_ANALOG  0x01
#define SPECIALDEV_NEW_ANALOG  0x02
#define SPECIALDEV_MFRC522     0x03
#define SPECIALDEV_PWM         0x04
#define SPECIALDEV_FAST_GPIO   0x05


#define MFRC522_GET_ALL_DEVS   0x00
#define MFRC522_GET_TAG	       0x01

// Timout in ms for fd poll
#define ECP_TIMEOUT 1000

#define ECP_CMD_DEL 0xFF

#define ECP_INPUT false
#define ECP_OUTPUT true
#define ECP_DEFAULT_TARGET false
#define ECP_DEF_DEV "/dev/ttyACM0"

struct ecproto_port_t{
	uint8_t bit;
	bool ddir; /* Datadirection: INPUT = 0, OUTPUT = 1 */

	 /* The default value which will be written to the port register.*/
	bool target;
	bool current; /* The value which is read from the pin register */
	bool enabled; /* Wether the pin is disabled or not. Set by the device's
		       * internal blacklist. Transmitted at startup.
		       */
	
	

};

struct ecproto_port_register_t{
	/* The identifier for that register */
	char id;
	struct ecproto_port_t bits[ECP_REG_WIDTH];
};

/* DEPRECATED! To be removed! */
struct ecproto_analog_t{
	bool used;
	uint8_t value;
};

/* It's successor */
struct ecproto_analog_channel_t{
	uint16_t value;
	uint8_t id;
};


struct ecproto_mfrc522_dev_t{
	uint32_t tag;
	bool tag_present;
	bool working;
};

struct ecproto_device_t{

	size_t id;
	bool gpio_capable;
	/* v This contains the register count that the device told us v */
	size_t received_regcnt;
	/* This is the thus far added amount of registers*/
	size_t regcnt;
	struct ecproto_port_register_t* regs;
	
	/* v This contains structures used by the old analog system v */
	struct ecproto_analog_t* analog;
	
	bool mfrc522_capable; /* Either set automatically via the
			       * special device types, or via a dependency.
			       */
	size_t mfrc522_devcnt;
	
	struct ecproto_mfrc522_dev_t* mfrc522_devs;

	bool fast_gpio_capable; /* < Defines weather the device supports the
	                         *   faster GPIO initialisation routines. */
	
	/* Contains the new analog subsystem */
	struct ecproto_analog_channel_t *analog_channels;
	size_t analog_channel_count;

};

/* Storage for devices */
extern size_t ecp_devcnt;
struct ecproto_device_t *ecp_devs;

/* THE FILE DESCRIPTOOOOR!! */
int ecp_fd;
/* AAAAND THE IIIINNNNCCREEDDDIIBBBBLE LOOOOOOOOOOOOCK */
pthread_mutex_t ecp_lock, ecp_readlock, ecp_state_lock;

bool ecp_initialized;

int ecp_init_dependency(json_object* dependency);
int ecp_init_port_dependency(json_object* dependency);
int ecp_init_analog_dependency(json_object* dependency);
int ecp_init_new_analog_dependency(json_object* dependency);

int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, 
	bool pulled, bool is_input);
int ecp_register_register(size_t device_id, char reg_id);
int ecp_register_device(size_t id);
int init_ecp();

int start_ecp();
void* loop_ecp();
int ecp_get_updates();
int reset_ecp();

int ecp_check_dependency(json_object* dependency, float* percentage);
int ecp_check_port_dependency(json_object* dependency);
float ecp_check_analog_dependency(json_object* dependency);
float ecp_check_mfrc522_dependency(json_object* dependency);
float ecp_check_new_analog_dependency(json_object* dependency);

int ecp_trigger(json_object* trigger, bool dry);
int ecp_trigger_secondary_trans(json_object* trigger, bool dry);
int ecp_trigger_port(json_object* trigger, bool dry);
int ecp_trigger_pwm(json_object* trigger, bool dry);


uint8_t* recv_ecp_frame(int fd, size_t* len);

bool validate_ecp_frame(uint8_t* frame, size_t len);

int parse_ecp_input(uint8_t* recv_frm, size_t recv_len, uint8_t* snd_frm, size_t
	snd_len);

int ecp_enable_purpose(struct ecproto_device_t* const dev, uint8_t purpose);
int ecp_handle_special_interact(uint8_t* recv_frm);
int ecp_handle_mfrc522(uint8_t* recv_frm);

/* May be done at any time to notify devices and detect changes. */
int ecp_bus_init();
int init_ecp_device(struct ecproto_device_t* device);
int init_ecp_gpio(struct ecproto_device_t* device);
int init_ecp_gpio_fast(struct ecproto_device_t* device);
int ecp_bitwise_init_gpio_reg(struct ecproto_device_t* device,
	struct ecproto_port_register_t* regp);
int ecp_init_analog_channels(struct ecproto_device_t* device);

int get_ecp_port(size_t device_id, char reg_id, size_t pin_id);
int send_ecp_port(size_t device_id, char reg_id, size_t pin_id, bool port);
int send_ecp_ddir(size_t device_id, char reg_id, size_t pin_id, bool ddir);
int send_ecp_secondary(size_t device_id, const char* str);
int send_ecp_analog_req(size_t device_id);
int ecp_send_message(size_t device_id, uint8_t action_id, uint8_t* payload, 
	size_t payload_len);

int write_ecp_msg(size_t dev_id, int fd, uint8_t action_id, uint8_t* payload, 
	size_t payload_length, uint8_t** frme, size_t* frame_length);

int ecp_receive_msgs(uint8_t* snd_frame, size_t snd_len);

/* HELPER FUNCTIONS */

struct ecproto_device_t* ecp_get_dev_from_id(size_t id);
struct ecproto_port_register_t* ecp_get_reg_from_dev(char id, struct 
	ecproto_device_t* dev);
int init_ecp_port_reg(struct ecproto_port_register_t* prt_reg);
int set_ecp_current_state(size_t dev_id, char reg_id, size_t bit, bool state);
uint8_t get_ddir_from_register(struct ecproto_port_register_t* reg);
uint8_t get_port_from_register(struct ecproto_port_register_t* reg);
uint8_t get_pins_enabled_from_register(struct ecproto_port_register_t* reg);
int ecp_ddir_to_register(struct ecproto_port_register_t* regp, uint8_t reg);
int ecp_pin_to_register(struct ecproto_port_register_t* regp, uint8_t reg);
int ecp_set_device_analog_channel_value(struct ecproto_device_t* dev,           
	uint8_t channel, uint16_t value);

#endif /* ECPROTO_H */
#endif /* NOEC */
