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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


char** video_current_urls = NULL;
char** video_perma_urls = NULL;
size_t video_device_cnt = 1;

int init_video(){

	pthread_mutex_init(&video_urls_lock, NULL);
	
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
	video_perma_urls = malloc(video_device_cnt * sizeof(char*));
	
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
			video_perma_urls[i] = NULL;
		}

	}else{
		const char* boot_url = json_object_get_string(video_init_url);
		println("Setting all %i monitors to display[%s]", DEBUG, 
			video_device_cnt, boot_url);
		
		for(size_t i = 0; i < video_device_cnt; i++){
			video_current_urls[i] = malloc(strlen(boot_url) + 
				sizeof(char));
			video_perma_urls[i] = malloc(strlen(boot_url) + 
				sizeof(char));
			strcpy(video_current_urls[i], boot_url);
			strcpy(video_perma_urls[i], boot_url);
		}
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

	pthread_mutex_lock(&video_urls_lock);
	if(video_perma_urls[device_no] == NULL && 
		video_current_urls[device_no]){

		/* Both are already NULL, I don't have to do a thing */
	
	}else if(video_perma_urls[device_no] == NULL){
		
		/* Only the permanent URL is NULL, not the current one, I
		 * will set the current one to NULL to let the device know
		 * to play nothing */
		free(video_current_urls[device_no]);
		video_current_urls[device_no] = NULL;

	}else{
		if(strcmp(video_current_urls[device_no], 
			video_perma_urls[device_no]) != 0){
			
			println("Updating video for monitor %i:", DEBUG,
				device_no);

			println("%s --> %s", DEBUG, 
				video_current_urls[device_no], 
				video_perma_urls[device_no]);

			free(video_current_urls[device_no]);
			video_current_urls[device_no] = malloc(strlen(
				video_perma_urls[device_no]) + sizeof(char));
			strcpy(video_current_urls[device_no], 
				video_perma_urls[device_no]);
			
		}
	}
	pthread_mutex_unlock(&video_urls_lock);

	return 0;
}

int video_trigger(json_object* trigger, bool dry){

	json_object* action = json_object_object_get(trigger, "action");
	if(action == NULL){
		println("Video trigger without action! Dumping root:", ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}

	const char* act = json_object_get_string(action);
	
	json_object* device = json_object_object_get(trigger, "monitor");
	size_t dev = 0;

	if(device == NULL){
		println("Video trigger without device! Assuming 0", WARNING);
	}else{
		dev = json_object_get_int(device);
	}

	if(dev >= video_device_cnt){
	println("Attempted to trigger invalid video dev! Dumping root:",
			ERROR);
		json_object_to_fd(STDOUT_FILENO, trigger, 
			JSON_C_TO_STRING_PRETTY);
		return -2;

	}

	if(strcasecmp(act, "immediate") == 0){
	
		json_object* resource = json_object_object_get(trigger, 
			"resource");
		if(resource == NULL){
			println("Video imm with no resource! Dumping root:"
				, ERROR);
			json_object_to_fd(STDOUT_FILENO, trigger, 
				JSON_C_TO_STRING_PRETTY);
			return -3;
		}
		
		const char* res = json_object_get_string(resource);

		println("Immediately replacing video with targret: ", DEBUG);
		println("%s --> %s ", DEBUG, video_current_urls[dev], res);
		
		pthread_mutex_lock(&video_urls_lock);
		if(!dry){
			free(video_current_urls[dev]);
			video_current_urls[dev] = malloc(strlen(res) + 
				sizeof(char));
			strcpy(video_current_urls[dev], res);
		}else{
			println("Not performing due to dry run", DEBUG);
		}
		pthread_mutex_unlock(&video_urls_lock);


	}
	else if(strcasecmp(act, "permanent") == 0){
	
		json_object* resource = json_object_object_get(trigger, 
			"resource");
		if(resource == NULL){
			println("Video perma with no resource! Dumping root:"
				, ERROR);
			json_object_to_fd(STDOUT_FILENO, trigger, 
				JSON_C_TO_STRING_PRETTY);
			return -3;
		}
		
		const char* res = json_object_get_string(resource);

		println("Permanently replacing video with targret: ", DEBUG);
		println("%s --> %s ", DEBUG, video_perma_urls[dev], res);
		
		pthread_mutex_lock(&video_urls_lock);
		
		if(!dry){
			free(video_perma_urls[dev]);
			video_perma_urls[dev] = malloc(strlen(res) + 
				sizeof(char));
			strcpy(video_perma_urls[dev], res);
		}else{
		
			println("Not performing due to dry run", DEBUG);
		}
		pthread_mutex_unlock(&video_urls_lock);


	}
	else if(strcasecmp(act, "reset") == 0){
	
		if( device == NULL){
			println("Immediately halting all video playback.", 
				DEBUG);
		
			pthread_mutex_lock(&video_urls_lock);
			
			for(size_t i = 0; i < video_device_cnt; i++){
				free(video_perma_urls[i]);
				free(video_current_urls[i]);
				
				video_perma_urls[i] = NULL;
				video_current_urls[i] = NULL;
			}
			pthread_mutex_unlock(&video_urls_lock);

		}else{
			println("Immediately halting video playback on dev %i", 
				DEBUG, dev);
			free(video_perma_urls[dev]);
			free(video_current_urls[dev]);
				
			video_perma_urls[dev] = NULL;
			video_current_urls[dev] = NULL;

		}


	}else{
		println("Unknown action for video trigger: \"%s\"", 
			ERROR, act);
		return -1;
		
	}

	return 0;
}

#endif /* NOVIDEO */
