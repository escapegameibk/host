/* sound controller for the escape game innsbruck
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
#include "sound.h"
#include <stdlib.h>
#include "config.h"

int init_sound(){

        vlc_inst = libvlc_new (0, NULL);
        vlc_mp = malloc(0);

        /* If available play startup sound */
        const char* boot_snd = json_object_get_string(
                                json_object_object_get(config_glob,
                                        "boot_sound"));
        if(boot_snd != NULL){

                libvlc_media_t *m = libvlc_media_new_location (vlc_inst, boot_snd);
                vlc_mp = realloc(vlc_mp,++playercnt);
                vlc_mp[0] = libvlc_media_player_new_from_media (m);
                libvlc_media_release(m);
                libvlc_media_player_play(vlc_mp[0]);
        }

        return 0;
}
