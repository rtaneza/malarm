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

#ifndef _MALARM_MAIN_H_
#define _MALARM_MAIN_H_

#include <hildon/hildon.h>
#include <libosso.h>
#include <gconf/gconf-client.h>
#include <alarmd/alarm_event.h>

#define MALARM_NAME  PACKAGE_NAME
#define MALARM_FULL_NAME  "Maemo alarm"
#define MALARM_VERSION  PACKAGE_VERSION

#define MALARM_DBUS_NAME "org.maemo." MALARM_NAME
#define MALARM_DBUS_PATH "/org/maemo/" MALARM_NAME
#define MALARM_GCONF_DIR  "/apps/maemo/" MALARM_NAME "/"


// #define MALARM_DEBUG

#ifdef MALARM_DEBUG
#define malarm_debug(f, x...) \
	g_print("%s [%d]: " f, __func__,__LINE__, ##x)
#else
#define malarm_debug(f, x...)  do { } while (0)
#endif

#define malarm_print(f, x...) \
	g_print("%s [%d]: " f, __func__,__LINE__, ##x)


typedef struct {
	HildonProgram *program;
	HildonWindow *window;
	osso_context_t *ctx;
	GConfClient *gconf;

	GtkTreeStore *store;
	GtkWidget *view;
	GtkWidget *sound_combo_box;
	GtkWidget *preview_button;
	GtkCellRenderer *toggled_renderer;
	gint sound_idx;
	int sound_playing;
	gulong cb_toggled_handler_id;
	int toggled_processing;
	GtkTreeIter toggled_iter;

	int widget_running;
	int visibility;
	int window_active;
	int window_topmost;
} app_data;


#endif /* #define _MALARM_MAIN_H_ */

