/* a logger for fancier output
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

#ifndef LOG_H
#define LOG_H

#ifndef DEBUG_LVL
#define DEBUG_LVL 1
#endif
#define DEBUG_NORM 0 
#define DEBUG_MORE 1
#define DEBUG_ALL  2

#define _GNU_SOURCE
#include <pthread.h>

/* Specifies the logger output type
 * This enum IS REQUIRED to start at 0in order to make the array where the
 * actual string representation is stored work
 */
enum log_type { DEBUG=0, INFO, WARNING, ERROR };

int println(const char* output, int type,...);
char* log_generate_prestring(int type);

int init_log();

#ifndef NOMEMLOG

char* get_log();

/* This is the new FD of stdout, after it has been replaced by the pipe. */
extern int stdout_new;

int stdout_replace_pipe[2], pipe_to_console[2];

int init_log_pipes();
pthread_mutex_t tee_ready;
void* loop_log_tee();

#define DEFAULT_LOGFILE "/tmp/log.log"

extern char* log_file;

#ifndef LOG_APPEND
#define LOG_APPEND 0
#endif /* LOG_APPEND */

#endif /* NOMEMLOG */
#endif /* LOG_H */
