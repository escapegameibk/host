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

#include "log.h"
#include "config.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <json-c/json.h>

int current_debug_lvl = DEBUG_LVL_DEFAULT;

int println(const char* output, int type, ...){
        
	if(type < current_debug_lvl){
		/* output is surpressed */
		return 0;
	}

        va_list arg;
        va_start (arg, type);
        char* pre = log_generate_prestring(type);
        
        size_t len = strlen(output) + strlen(pre) + 2;
        len ++; // Me wants a newline!

        char* fino = realloc(pre, len);
        strncat(fino,output,len);
        strncat(fino,"\n", len);
        
        vprintf(fino,arg);

        free(fino);
        va_end (arg);
        return 0;

}

#ifndef COLOR
char* log_typestr[] = {
        "INVALID  ",
        "DEBUG+2  ",
        "DEBUG+1  ",
        "DEBUG    ",
        "INFO     ",
        "WARNING  ",
        "ERROR    "
};
#else
char* log_typestr[] = {
        "INVALID     ",
        "DEBUG+2     ",
        "DEBUG+1     ",
        "DEBUG       ",
        "\e[0;32mINFO\e[0m        ",
        "\e[0;33mWARNING\e[0m     ",
        "\e[0;31mERROR\e[0m       "
};
#endif

char * log_generate_prestring(int type){
        
        char* pre;

#if __linux__

#define BUFFERLENGTH 256
        pre = malloc(BUFFERLENGTH);
        memset(pre,0,BUFFERLENGTH);
        
        /* Get time */
        time_t rawtime;
        struct tm *info;
        time( &rawtime );
        info = localtime( &rawtime );
        
#define TIME_LEN 30
        char* datestr = malloc(TIME_LEN); 
        strftime(datestr, TIME_LEN, "%FT%T%z", info);
        snprintf(pre, BUFFERLENGTH, "[ %s %s] ", datestr, log_typestr[type]);
        pre = realloc(pre,strlen(pre) + 1);
        free(datestr);

#endif
        return pre;
}

int init_log(){

#ifndef NOMEMLOG
	if(init_log_pipes() < 0){
		fprintf(stderr, "FAILED TO INIT LOG!");
		return -1;
	}
	pthread_mutex_init(&tee_ready, NULL);
	pthread_mutex_lock(&tee_ready);

	pthread_t log_redir_thread;
	if(pthread_create(&log_redir_thread,NULL,loop_log_tee,NULL)){
		perror("Failed to start tee loop in log");
	}
	
	char* notify = "Wating for main log thread to be ready...\n";
	if(write(stdout_new, notify, strlen(notify)) < 0){
		perror("Write int message failed. ignoreing");
	}

	/* Wait for log to become ready */
	pthread_mutex_lock(&tee_ready);
	
	char* denotify = "Main log thread has become ready!\n";
	if(write(STDOUT_FILENO, denotify, strlen(denotify)) < 0){
		perror("Write int message failed. ignoreing");
	}
	
	stdout = fdopen(STDOUT_FILENO, "w");
	if(stdout == NULL){
		perror("Failed to create new log file pointer");
		return -1;
	}else{
		setlinebuf(stdout);
		fprintf(stdout, "LOG INIT\n");
	}

#endif /* NOMEMLOG */

	return 0;
}

int init_log_config(){

	json_object* log_level = json_object_object_get(config_glob, 
		"log_level");

	if(log_level == NULL){
		println("Log level not configured, assumeing default %i",
			DEBUG, current_debug_lvl);
		return 0;
	}else{
		const char* log_lvl = json_object_get_string(log_level);

		if(strcasecmp(log_lvl,"DEBUG_ALL") == 0){
			current_debug_lvl = DEBUG_ALL;
		}else if(strcasecmp(log_lvl,"DEBUG_MOST") == 0){
			current_debug_lvl = DEBUG_MOST;
		}else if(strcasecmp(log_lvl,"DEBUG_MORE") == 0){
			current_debug_lvl = DEBUG_MORE;
		}else if(strcasecmp(log_lvl,"DEBUG") == 0){
			current_debug_lvl = DEBUG;
		}else if(strcasecmp(log_lvl,"INFO") == 0){
			current_debug_lvl = INFO;
		}else if(strcasecmp(log_lvl,"WARNING") == 0){
			current_debug_lvl = WARNING;
		}else if(strcasecmp(log_lvl,"ERROR") == 0){
			current_debug_lvl = ERROR;
		}else{
			println("INVALID log_level specified: %s:",
				ERROR, log_lvl);
			return -1;
		}
		println("Updated log level to %i", DEBUG, current_debug_lvl);
			
	}

	return 0;

}

#ifndef NOMEMLOG

char* log_file = DEFAULT_LOGFILE;

char* get_log(){

	FILE* fp = fopen(log_file, "r");
	if(fp == NULL){
		return NULL;
	}
	fseek(fp, 0L, SEEK_END);
	long sz = ftell(fp);
	rewind(fp);
	
	char* file = malloc(++sz * sizeof(char));
	if(file == NULL){
		return NULL;
	}
	memset(file, 0, sz);
	
	fread(file, sz, 1, fp);
	
	fclose(fp);

	return file;

}

ssize_t get_log_len(){
	
	FILE* fp = fopen(log_file, "r");
	if(fp == NULL){
		return -1;
	}
	fseek(fp, 0L, SEEK_END);
	long sz = ftell(fp);

	fclose(fp);

	return sz;

}

int stdout_new = STDOUT_FILENO;

int init_log_pipes(){
	

	if(pipe(stdout_replace_pipe) < 0){
		perror("Pipe failed for stdout replacement pipe");
		return -1;
	}
	
	if(pipe(pipe_to_console) < 0){
		perror("Pipe failed for console output");
		return -1;
	}
	
	/* Replace pipe ends with targets */
	stdout_new = dup(STDOUT_FILENO);

	if(stdout_new < 0){
		perror("Failed to dup stdout");
		return -1;
	}else if (stdout_new == STDOUT_FILENO){
		fprintf(stderr, "Failed to set different STDOUT filepointer");
		return -1;
	}else{

	}
	
	if(dup2(stdout_replace_pipe[1], STDOUT_FILENO) < 0){
		perror("Failed to set pipe as new stdout");
		return -1;
	}
	if(close(stdout_replace_pipe[1]) < 0){
		perror("Failed to close old pipe end");
	}
	/* Pipes ready */
	return 0;


}

void* loop_log_tee(){

#if LOG_APPEND
	int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
#else
	int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif

	if(fd < 0){
		perror("Failed to open log file!!");
		goto error_out;
	}

	/* Log should now be ready. Release main thread */
	pthread_mutex_unlock(&tee_ready);
	ssize_t len = 0, len2 = 0, slen = 0;
	for(;;){
		
		len2 = len = tee(stdout_replace_pipe[0], pipe_to_console[1], 
			INT_MAX, 0);

		if(len < 0){
			if(errno == EAGAIN){
				continue;
			}else{
				perror("Tee in log failed!");
				break;
			}
			

		}else if( len == 0){
			/* Write end done */
			break;
		}else{
			/* Write the same data to the file! */
			while(len > 0){
				
				slen = splice(stdout_replace_pipe[0], NULL, fd,
					NULL, len, SPLICE_F_MOVE);
				if(slen < 0){
					perror("Log splice failed!");
					break;
				}else{
					len -= slen;
				}
			}
			
			while(len2 > 0){
				
				/* I'm really really sorry. I didn't mean to be
				 * evil, but sendfile doesn't work in regular
				 * terminals, tee requires pipes and slice wants
				 * me to have the target also in non-append mode
				 * and requires it to be seekable, WHICH A 
				 * TERMINAL ISN'T! If you know a fancier
				 * solution, please tell me! I won't make an
				 * exec on tee though...
				 */

				char buffer[2048];

				slen = read(pipe_to_console[0],
					buffer, sizeof(buffer));
					
				if(slen < 0){
					perror("Log splice failed!");
					break;
				}else{
					len2 -= slen;
				}
				write(stdout_new, buffer, slen);
			}
		}

	}

error_out:
	/* Restore old fds */
	if(dup2(stdout_new, STDOUT_FILENO) < 0){
		perror("Dup Error handling in log failed!!");
	}
	else{
		println("Remapped stdout to direct output!", ERROR);
		println("No longer able to print to log file!", ERROR);

	}
	pthread_mutex_unlock(&tee_ready);

	return NULL;
}

#endif /* NOMEMLOG */
