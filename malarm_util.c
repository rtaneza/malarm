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

#include <osso-multimedia-interface.h>

#include "malarm_util.h"

void print_alarm_event(cookie_t cookie, alarm_event_t *event)
{
	malarm_debug("malarm event: cookie %d, time %d\n", cookie, event->alarm_time);
#if 0
	malarm_debug("malarm event: cookie %d\n", cookie);
	malarm_debug("  alarm_time: %d, %s"
			"  recurrence: %u\n"
			"  recurrence_count: %d\n"
			"  snooze: %u\n"
			, event->alarm_time, ctime_r(&event->alarm_time, buf),
			event->recurrence,
			event->recurrence_count,
			event->snooze);
	malarm_debug("  title: %s\n"
			"  message: %s\n"
			"  sound: %s\n"
			"  icon: %s\n"
			, event->title,
			event->message,
			event->sound,
			event->icon);
	malarm_debug("  dbus_interface: %s\n"
			"  dbus_service: %s\n"
			"  dbus_path: %s\n"
			"  dbus_name: %s\n"
			"  exec_name: %s\n"
			"  flags: 0x%08x = %d\n"
			"  snoozed: %u\n"
			, event->dbus_interface,
			event->dbus_service,
			event->dbus_path,
			event->dbus_name,
			event->exec_name,
			event->flags, event->flags,
			event->snoozed);
	malarm_debug("\n");
#endif
}

void print_stm(struct tm *ptm)
{
	malarm_debug("sruct tm: %s", asctime(ptm));
	malarm_debug("  sec:  %d\n"
		   "  min:  %d\n"
		   "  hour: %d\n"
		   "  mday: %d\n"
		   "  mon:  %d\n"
		   "  year: %d\n"
		   , ptm->tm_sec, ptm->tm_min, ptm->tm_hour,
		   ptm->tm_mday, ptm->tm_mon, ptm->tm_year);
}

void print_itm(time_t itm)
{
	malarm_debug("time: %ld: %s", itm, ctime(&itm));
}


int play_sound(app_data *app, const char *path)
{
	osso_return_t ret;
	osso_rpc_t retval;

	/* malarm_debug("playing %s\n", path); */
	ret = osso_rpc_run(app->ctx, OSSO_MULTIMEDIA_SERVICE, 
			OSSO_MULTIMEDIA_OBJECT_PATH, OSSO_MULTIMEDIA_SOUND_INTERFACE, 
			OSSO_MULTIMEDIA_PLAY_SOUND_REQ, &retval,
			DBUS_TYPE_STRING, path,
			DBUS_TYPE_INT32, 1, // what are possible priority values?
			DBUS_TYPE_INVALID);
	if (ret != OSSO_OK) {
		malarm_print("error sending play rpc to osso-multimedia-service: %d\n", ret);
		malarm_print("osso retval: type %d, val %d\n", retval.type, retval.value.i);
		return -1;
	}
	
	app->sound_playing = 1;
	return 0;
}

int stop_sound(app_data *app)
{
	osso_return_t ret;
	osso_rpc_t retval;

	if (!app->sound_playing) {
		return 0;
	}

	ret = osso_rpc_run(app->ctx, OSSO_MULTIMEDIA_SERVICE, 
			OSSO_MULTIMEDIA_OBJECT_PATH, OSSO_MULTIMEDIA_SOUND_INTERFACE, 
			OSSO_MULTIMEDIA_STOP_SOUND_REQ, &retval,
			DBUS_TYPE_INVALID);
	if (ret != OSSO_OK) {
		malarm_print("error sending stop rpc to osso-multimedia-service: %d\n", ret);
		malarm_print("osso retval: type %d, val %d\n", retval.type, retval.value.i);
		return -1;
	}

	app->sound_playing = 0;
	return 0;
}

// get actual time of disabled alarm
time_t get_actual_alarm_time(app_data *app, cookie_t cookie)
{
	time_t actual_time;
	gchar key[100];

	cookie_to_gconf_key(cookie, key);
	actual_time = gconf_client_get_int(app->gconf, key, NULL);
	if (actual_time == 0) {
		malarm_print("error: failed to get gconf key %s\n", key);
		return -1;
	}

	return actual_time;
}

void get_next_alarm_time(alarm_event_t *event, struct tm *stm)
{
	time_t itm;

	// if alarm was snoozed, show next alarm time, not the original time
	itm = event->alarm_time + event->snoozed*60;
	localtime_r(&itm, stm);
	// normalize stm
	mktime(stm);
}

void date_to_string(struct tm *stm, char *buf, int flags)
{
	static char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static char *months[] = { 
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	int off;
	int is_pm = 0;

	g_assert(stm->tm_wday >= 0 && stm->tm_wday <= 6);
	g_assert(stm->tm_mon >= 0 && stm->tm_mon <= 11);

	if (stm->tm_hour == 0) {
		stm->tm_hour = 12;
	} else if (stm->tm_hour > 12) {
		stm->tm_hour = stm->tm_hour - 12;
		is_pm = 1;
	}
	off = sprintf(buf, "%02d:%02d %s  ", stm->tm_hour, stm->tm_min, 
			(is_pm==0) ? "AM" : "PM");
	/* sprintf(buf+off, "  %s %s %d, %d", wday[stm->tm_wday],  */
	if (flags & DATE_TO_STRING_WDAY) {
		off += sprintf(buf+off, "%s ", wday[stm->tm_wday]);
	}
	sprintf(buf+off, "%s %d, %d", months[stm->tm_mon], stm->tm_mday, 
			stm->tm_year+1900);
}

char *cookie_to_gconf_key(cookie_t cookie, char *key)
{
	sprintf(key, "%s%d", MALARM_GCONF_DIR, cookie);
	return key;
}

void show_banner(app_data *app, const char *text)
{
	hildon_banner_show_information(GTK_WIDGET(app->window), NULL, text);
}

