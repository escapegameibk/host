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
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

int println(const char* output, int type, ...){
        
        va_list arg;
        va_start (arg, type);
        char* pre = log_generate_prestring(type);
        
        size_t len = strlen(output) + strlen(pre) + 2;
        
        char* fino = malloc(len+1); /* Me wants a newline! */
        strcpy(fino,pre);
        strcat(fino,output);
        strcat(fino,"\n");
        
        vprintf(fino,arg);

        free(pre);
        free(fino);
        va_end (arg);
        return 0;

}

#ifndef COLOR
char* log_typestr[] = {
        "DEBUG\t",
        "INFO\t\t",
        "WARNING\t",
        "ERROR\t"
};
#else
char* log_typestr[] = {
        "DEBUG\e[0m\t",
        "\e[0;32mINFO\e[0m\t\t",
        "\e[0;33mWARNING\e[0m\t",
        "\e[0;31mERROR\e[0m\t"
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
        sprintf(pre, "[ %s %s] ", datestr, log_typestr[type]);
        pre = realloc(pre,strlen(pre) + 1);
        free(datestr);

#endif
        return pre;
}
