/* a command interface via unix socket
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

#ifndef INTERFACE_H
#define INTERFACE_H

#define SOCKET_LOCATION "/var/run/escape/socket"
#include <pthread.h>

int init_interface();
int start_interface();

void * loop_interface();
int handle_comm(int fd);

int unix_fd;
pthread_t interface_thread; 

int execute_command(int sock_fd,char* command);
int print_info_interface(int sockfd);
int print_changeables_interface(int sockfd);
int print_events_interface(int sockfd);
int print_dependencies_interface(int sockfd);
int print_dependenciy_states_interface(int sockfd);

#endif
