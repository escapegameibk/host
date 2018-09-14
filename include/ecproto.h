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

#define ECP_REG_WIDTH 8
#define ECPROTO_OVERHEAD 5

#define ECP_INPUT false
#define ECP_OUTPUT true
#define ECP_DEF_DEV "/dev/ttyACM0"

struct ecproto_port_t{
	uint8_t bit;
	bool ddir; /* Datadirection: INPUT = 0, OUTPUT = 1 */

	 /* The default value which will be written to the port register.*/
	bool target;
	bool current; /* The value which is read from the pin register */
};

struct ecproto_port_register_t{
	/* The identifier for that register */
	char id;
	struct ecproto_port_t bits[ECP_REG_WIDTH];
};


struct ecproto_device_t
{
	/* If the id is 0, a direct connection is assumed */
	size_t id;
	size_t regcnt;
	struct ecproto_port_register_t* regs;
};

/* Storage for devices */
extern size_t ecp_devcnt;
struct ecproto_device_t *ecp_devs;

/* THE FILE DESCRIPTOOOOR!! */
int ecp_fd;

int ecp_init_dependency(json_object* dependency);
int ecp_register_input_pin(size_t device_id, char reg_id, size_t bit, 
	bool pulled);
int init_ecp();

int start_ecp();

/* HELPER FUNCTIONS */

struct ecproto_device_t* escp_get_dev_from_id(size_t id);
struct ecproto_port_register_t* escp_get_reg_from_dev(char id, struct 
	ecproto_device_t* dev);
int init_ecp_port_reg(struct ecproto_port_register_t* prt_reg);

#endif /* ECPROTO_H */
#endif /* NOEC */
