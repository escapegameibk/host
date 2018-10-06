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

#ifndef VIDEO_H
#define VIDEO_H

#include "config.h"
#include "video.h"

int init_video();

/* The video module doesn't need a start function, as it only has to modify it's
 * resources. */
/* int start_video(); */

extern char** video_current_urls;
extern char** video_perma_urls;

extern size_t video_device_cnt;

#endif /* VIDEO_H */
#endif /* NOVIDEO */
