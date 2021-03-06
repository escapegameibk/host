/* general data storage
 * Copyright © 2018 tyroylean
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

#ifndef DATA_H
#define DATA_H

/* Relaeses are built like this:
 *      MAJOR:
 *              Increased when majour rebuilds have occured.
 *              0...            ALPHA
 *              1...            BETA
 *		2ff...          RELEASE
 *      MINOR:
 *              Increased when features were added.
 *      RELEASE:
 *              Increased when bugs were fixed or performance increased.
 *
 */
#define VERSION_MAJOR           0
#define VERSION_MINOR           9
#define VERSION_RELEASE         2

#define SHUTDOWN_DELAY 3

int exit_code;

int shutting_down;
#endif
