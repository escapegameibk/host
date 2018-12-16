/* Freehand Modbus implementation for the escape game system
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

#ifndef MODBUS_H
#define MODBUS_H

#ifndef NOMODBUS

#define MODBUS_DEFAULT_DEV "/dev/ttyUSB0"

#define MODBUS_DEFAULT_BAUD B9600

#define MODBUS_ACTION_SET_REGISTER 0x06
#define MODBUS_PROTOCOL_OVERHEAD ( 2 /* Address and id */ + 2 /* Checksum */ )

#define MODBUS_ACTION_SET_REGISTER_LENGTH ( MODBUS_PROTOCOL_OVERHEAD + 4 )

#define MODBUS_RELAY_STATE_KEEP 0x00
#define MODBUS_RELAY_STATE_ON   0x01
#define MODBUS_RELAY_STATE_OFF  0x02

#include <json-c/json.h>
#include <pthread.h>

#include <stdbool.h>

int modbus_fd;

int init_modbus();

int modbus_trigger(json_object* trigger, bool dry);

int modbus_write_register(uint8_t addr, uint16_t reg, uint16_t target);

int modbus_write_frame(uint8_t* frame, size_t length, size_t response_len);

#endif /* NOMODBUS */

#endif /* MODBUS_H */


