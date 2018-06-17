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

int println(char* output, int type){

        char * pre = log_generate_prestring(type);
        printf("%s %s\n",pre,output);

        free(pre);
        return 0;
}

char* log_generate_prestring(int type){
        
#define INTERMEDIATE_MAX 30
        char* raw = malloc(INTERMEDIATE_MAX);
        memset(raw,0,INTERMEDIATE_MAX);
#if defined __linux__ || defined __AVR_ARCH__
        time_t t;
        struct tm *tmp;
         t = time(NULL);
           tmp = localtime(&t);
           if (tmp == NULL) {
               printf("failed to get time");
               return NULL;
           }

        strftime(raw,INTERMEDIATE_MAX,"[%Y/%m/%d %T ",tmp);
#endif
        switch (type) {

                case DEBUG:
                        strcat(raw,"DEBUG] ");
                        break;
                case INFO:
                        strcat(raw,"INFO] ");
                        break;
                case WARNING:
                        strcat(raw,"WARNING] ");
                        break;
                case ERROR:
                        strcat(raw,"ERROR] ");
                        break;
                default:
                        strcat(raw,"WUT?] ");        
                        break;
                        
        }

        char* fin = malloc(strlen(raw));
        strcpy(fin,raw);
        free(raw);

#undef INTERMEDIATE_MAX
        return fin;
}
