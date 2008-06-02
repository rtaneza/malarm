/**
 * malarm: simple maemo alarm app for Nokia N8xx devices
 * Copyright (C) 2008  Ronald Taneza
 * 
 * The app framework (autotool files, etc.) is based on hhwX.c sample app by Nokia
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

#include "malarm_main.h"
#include "malarm_ui.h"

static gint cb_osso_rpc(const gchar *interface, const gchar *method, 
		GArray *arguments, gpointer data, osso_rpc_t *retval)
{
	app_data *app = (app_data*)data;

	g_assert(app != NULL);

	malarm_debug("interface=%s, method=%s\n", interface, method);
	populate_tree(app);

	retval->type = DBUS_TYPE_INVALID;
	return OSSO_OK;
}

int main(int argc, char **argv)
{
	app_data app = { };
	osso_return_t osso_ret;

	gtk_init(&argc, &argv);

	app.program = HILDON_PROGRAM(hildon_program_get_instance());
	g_set_application_name(MALARM_FULL_NAME);
	app.window = HILDON_WINDOW(hildon_window_new());
	hildon_program_add_window(app.program, HILDON_WINDOW(app.window));

	/* create a LibOSSO context and attach app to D-Bus */
	app.ctx = osso_initialize(MALARM_DBUS_NAME, MALARM_VERSION, TRUE, NULL);
	if (app.ctx == NULL) {
		malarm_print("error: failed to init LibOSSO\n");
		return -1;
	}

	osso_ret = osso_rpc_set_cb_f(app.ctx, MALARM_DBUS_NAME, MALARM_DBUS_PATH, MALARM_DBUS_NAME, cb_osso_rpc, &app);
	if (osso_ret != OSSO_OK) {
		malarm_print("error: failed to register LibOSSO callback function\n");
		return -1;
	}

	app.gconf = gconf_client_get_default();
	g_assert(GCONF_IS_CLIENT(app.gconf));

	create_ui(&app);

	g_signal_connect(G_OBJECT(app.window), "delete-event", gtk_main_quit, NULL);

	gtk_widget_show_all(GTK_WIDGET(app.window));

	gtk_main();

	osso_deinitialize(app.ctx);

	return 0;
}

