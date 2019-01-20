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

/* Specifies the logger output type
 * This enum IS REQUIRED to start at 0in order to make the array where the
 * actual string representation is stored work
 */
enum log_type { DEBUG=0, INFO, WARNING, ERROR };

int println(const char* output, int type,...);
char* log_generate_prestring(int type);

#endif
