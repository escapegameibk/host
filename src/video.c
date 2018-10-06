/* Video module for the escape game's host --> RQ2018/01
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

#ifndef NOVIDEO

#include "video.h"

#include "config.h"
#include "log.h"

char** video_current_urls = NULL;
size_t video_device_cnt = 1;

int init_video(){
	
	json_object* display_count = json_object_object_get(config_glob,
		"monitor_count");

	if(display_count == NULL){

		/* I will assume only one monitor is connected if nothing is
		 * defined. This seems like a sane default to me.
		 */
		println("No monitor count specified. Assuming 1!", WARNING);
		video_device_cnt = 1;

	}else{
		video_device_cnt = json_object_get_int(display_count);
		println("Setting monitor amount to: %i ", DEBUG, 
			video_device_cnt);
	}

	video_current_urls = malloc(video_device_cnt * sizeof(char*));
	
	json_object* video_init_url = json_object_object_get(config_glob, 
		"boot_video");

	if(video_init_url == NULL){
		println("Initial video url not defined, disabling until set...",
			DEBUG);

		/* Set all char* to NULL due to the fact that i have nothing to
		 * set them to.
		 */

		for(size_t i = 0; i < video_device_cnt; i++){
			video_current_urls[i] = NULL;
		}

	}else{
		const char* boot_url = json_object_get_string(video_init_url);
		println("Setting all %i monitors to display[%s]", DEBUG, 
			video_device_cnt, boot_url);
	}


	return 0;
}

/* This function is called when a device has finished playing a video. */
int video_finished(size_t device_no){

	if(device_no >= video_device_cnt){
		/* Nope? */
		println("Attempted to finish video playback for invalid display"
			, WARNING);
		return -1;
	}

	return 0;
}

#endif /* NOVIDEO */
