/* The controller software for the wizard at the escape game ibk
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
#include "interface.h"
#include "data.h"
#include "game.h"
#include "serial.h"
#include "sound.h"
#include "config.h"
#include "mtsp.h"
#include "core.h"
#include "hints.h"
#include "ecproto.h"
#include "module.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
void catch_signal(int sig);

int main(int argc, char * const argv[]){
        
        println("INIT",DEBUG);

        /* register signals */
        signal(SIGINT,&catch_signal);
        signal(SIGKILL,&catch_signal);
        signal(SIGPIPE,&catch_signal);

        /* initialize global variables */
        exit_code = EXIT_SUCCESS;
        shutting_down = 0;

        char* config = NULL;

        int opt;

        while ((opt = getopt (argc, argv, "c:")) != -1){
                switch(opt){
                case 'c':
                        config = optarg;
                        println("updating config file from parameter: %s", 
                                        DEBUG, optarg);

                        break;
                default:
                        break;
                }


        }

        if(load_config(config) < 0){
                println("failed to load config!!",ERROR);
		exit_code = EXIT_FAILURE;
                goto shutdown;
        }
        
	if(init_game() < 0){
                println("failed to init game!!",ERROR);
		exit_code = EXIT_FAILURE;
                goto shutdown;
        }

        if(init_modules() < 0){
                println("failed to init modules!!",ERROR);
		exit_code = EXIT_FAILURE;
                goto shutdown;
        }
	 
        println("INIT DONE",INFO);

        println("strating modules...",DEBUG);
	if(start_modules() < 0){
		println("failed to start modules", ERROR);
		exit_code = EXIT_FAILURE;
                goto shutdown;
	}
	
	if(start_game() < 0){
		println("failed to start game watcher", ERROR);
		exit_code = EXIT_FAILURE;
                goto shutdown;
	}
	
        
	println("Startup done",Debug);
        println("ENTERING REGULAR OPERATION",INFO);
        while(!shutting_down){sleep(1);}

shutdown:
shutting_down = true;
        println("SHUTTING DOWN",WARNING);
        
        sleep(SHUTDOWN_DELAY);
        println("terminating with code %i", INFO, exit_code);

        return exit_code;
}

void catch_signal(int sig){

        switch(sig){

        case SIGKILL:
                /* Aeehhrrmmm wut? how could i catch that?*/
                println("WHAT THE HELL? SIGKILL JUST ARRIVED!",ERROR);
                exit_code = SIGKILL;
                shutting_down = 1;
                break;

        case SIGINT:
                println("SIGINT. shutting down",WARNING);
                exit_code = SIGINT;
                shutting_down = 1;
                break;
        case SIGPIPE:
                
                println("SIGPIPE! ignoring...",WARNING);
                break;

	case SIGHUP:
		
		/* There was an issue where systemd would randomly send me this
		 * signal. I have quite literarily no idea why because the 
		 * interval wasn't normal and nothing should've changed...
		 */
		println("Received SIGHUP. I hope i haven't really hung up...",
			WARNING);
		break;
        default:
                /* ignore */
                break;
        }

        return;

}
