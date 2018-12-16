/* A EC-Proto implementation for the escape game innsbruck's host
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
#define ADC_GET2               0x0F
#define EXT_DEV_REG            0x10
#define EXT_DEV_INT            0x11

#define ECP_LEN_IDX 0
#define ECP_ADDR_IDX 1
#define ECP_ID_IDX 2
#define ECP_PAYLOAD_IDX 3

// Timout in ms for fd poll
#define ECP_TIMEOUT 1000

#define ECP_CMD_DEL 0xFF

#define ECP_EXTDEV_MFRC522     0x01

#define ECP_RFID_TAGLEN	       5

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

struct ecp_mfrc522_t{
	/* Car and bit identify the chip select pin */
	char car;
	size_t bit;
	uint8_t id;  /* The bit assigned by the device */
	uint32_t lasttag;

	json_object* dependency;
};

struct ecproto_analog_t{
	bool used;
	uint8_t value;
};

struct ecproto_device_t
{
	size_t id;
	size_t regcnt;
	struct ecproto_port_register_t* regs;
	struct ecproto_analog_t* analog;

	/* MFRC522 devices */
	size_t mfrc522_cnt;
	struct ecp_mfrc522_t* mfrc522s;
};

/* Storage for devices */
extern size_t ecp_devcnt;
struct ecproto_device_t *ecp_devs;

/* THE FILE DESCRIPTOOOOR!! */
int ecp_fd;
/* AAAAND THE IIIINNNNCCREEDDDIIBBBBLE LOOOOOOOOOOOOCK */
pthread_mutex_t ecp_lock, ecp_readlock;

bool ecp_initialized;

int ecp_init_dependency(json_object* dependency);
int ecp_init_port_dependency(json_object* dependency);
int ecp_init_analog_dependency(json_object* dependency);
int ecp_init_extdev_dependency(json_object* dependency);

int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, 
	bool pulled, bool is_input);
int ecp_register_register(size_t device_id, char reg_id);
int ecp_register_device(size_t id);
int init_ecp();

int start_ecp();
void* loop_ecp();
int ecp_get_updates();

int ecp_check_dependency(json_object* dependency);
int ecp_check_port_dependency(json_object* dependency);
int ecp_check_analog_dependency(json_object* dependency);

int ecp_trigger(json_object* trigger, bool dry);
int ecp_trigger_secondary_trans(json_object* trigger, bool dry);
int ecp_trigger_port(json_object* trigger, bool dry);

uint8_t* recv_ecp_frame(int fd, size_t* len);

bool validate_ecp_frame(uint8_t* frame, size_t len);

int parse_ecp_input(uint8_t* recv_frm, size_t recv_len, uint8_t* snd_frm, size_t
	snd_len);

/* May be done at any time to notify devices and detect changes. */
int ecp_bus_init();

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

struct ecproto_device_t* escp_get_dev_from_id(size_t id);
struct ecproto_port_register_t* escp_get_reg_from_dev(char id, struct 
	ecproto_device_t* dev);
int init_ecp_port_reg(struct ecproto_port_register_t* prt_reg);
int set_ecp_current_state(size_t dev_id, char reg_id, size_t bit, bool state);

uint16_t ibm_crc(uint8_t* data, size_t len);

#endif /* ECPROTO_H */
#endif /* NOEC */
