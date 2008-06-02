/**
 * malarm: simple maemo alarm app for Nokia N8xx devices
 * Copyright (C) 2008  Ronald Taneza
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

#ifndef _MALARM_UTIL_H_
#define _MALARM_UTIL_H_

#include "malarm_main.h"

#define DATE_TO_STRING_WDAY  (1 << 0)

void print_alarm_event(cookie_t cookie, alarm_event_t *event);
void print_stm(struct tm *ptm);
void print_itm(time_t itm);

int play_sound(app_data *app, const char *path);
int stop_sound(app_data *app);

time_t get_actual_alarm_time(app_data *app, cookie_t cookie);
void get_next_alarm_time(alarm_event_t *event, struct tm *stm);
void date_to_string(struct tm *stm, char *buf, int flags);

char *cookie_to_gconf_key(cookie_t cookie, char *key);
void show_banner(app_data *app, const char *text);

#endif /* #define _MALARM_UTIL_H_ */

