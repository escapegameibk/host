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

int println(const char* output, int type){

        char *pre = log_generate_prestring(type);
        printf("%s %s\n",pre,output);

        return 0;
}

char* log_typestr[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR"
};


char * log_generate_prestring(int type){
        
        char* pre;

#if __linux__

#define BUFFERLENGTH 256
        pre = malloc(BUFFERLENGTH);
        memset(pre,0,BUFFERLENGTH);
        
        time_t t = time(NULL);
        struct tm tim = *localtime(&t);

        sprintf(pre, "[ %i-%i-%i %i:%i:%i  %s\t] ", tim.tm_year+1900, tim.tm_mon+1, 
                        tim.tm_mday, tim.tm_hour, tim.tm_min, tim.tm_sec,
                        log_typestr[type]);
        pre = realloc(pre,strlen(pre));

#endif
        return pre;
}
