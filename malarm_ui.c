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

#include <limits.h>
#include <time.h>

#include "malarm_main.h"
#include "malarm_ui.h"
#include "malarm_util.h"


#define TIME_T_MAX  (LONG_MAX)

// 0, TIME_T_MAX do not work!
/* #define ALARM_DISABLED  (0) */
/* #define ALARM_DISABLED  (TIME_T_MAX) // gives negative cookie */
#define ALARM_DISABLED  (TIME_T_MAX - 200)

// #sec to add to current time for a new alarm in "new alarm" dialog
#define NEW_ALARM_TIME_INC   (60*60)

// debounce delay when enabling/disabling an alarm
#define KEY_DEBOUNCE_DELAY  200  /* msec */

#define ALARM_EVENT_FLAGS  (ALARM_EVENT_BOOT | ALARM_EVENT_ACTDEAD | \
		ALARM_EVENT_SHOW_ICON | ALARM_EVENT_RUN_DELAYED)
		/* ALARM_EVENT_SHOW_ICON | ALARM_EVENT_POSTPONE_DELAYED) */
 
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define SNOOZE_STRING(snoozed) ((snoozed) ? "S" : " ")


// update create_tree() when modifying this enum
enum {
	SNOOZE_COLUMN,
	ENABLED_COLUMN,
	TIME_STRING_COLUMN,
	REPEAT_COLUMN,
	MESSAGE_COLUMN,
	COOKIE_COLUMN,
	ALARM_TIME_COLUMN,
	N_COLUMNS
};

enum {
	REPEAT_ONCE,
	REPEAT_DAILY,
	REPEAT_WEEKLY,
};

struct repeat_info {
	uint32_t val;
	char *text;
} repeat_list[] = {
	{ 0, "Once" },
	{ 60*24, "Daily" },
	{ 60*24*7, "Weekly" },
};

static char *sounds_list[] = {
	"file:///usr/share/sounds/ui-clock_alarm.mp3",
	"file:///usr/share/sounds/ui-clock_alarm2.mp3",
	"file:///usr/share/sounds/ui-clock_alarm3.mp3",
	"file:///usr/share/sounds/malarm_silent.mp3",
};


static void cb_row_activated(GtkTreeView *view, GtkTreePath *path,
		GtkTreeViewColumn *column, app_data *app);
static int alarm_dialog(app_data *app, cookie_t old_cookie, 
		cookie_t *new_cookie, alarm_event_t *event);
static int add_alarm_to_tree(app_data *app, cookie_t cookie, alarm_event_t *event);


static void cb_action_add(GtkWidget *widget, app_data *app)
{
	int ret;
	cookie_t cookie;
	alarm_event_t event;

	g_assert(app != NULL);

	/* malarm_debug("add alarm event\n"); */
	ret = alarm_dialog(app, 0, &cookie, &event);
	if (ret == 0) {
		add_alarm_to_tree(app, cookie, &event);
		/* populate_tree(app); */
		free(event.message);
		show_banner(app, "Added alarm");
	}
}

static void remove_item(app_data *app, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeIter next_iter;
	int nitems;
	int ret;

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(app->store), iter);
	g_assert(path);

	next_iter = *iter;
	gtk_tree_store_remove(GTK_TREE_STORE(app->store), iter);

	if (gtk_tree_model_iter_next(GTK_TREE_MODEL(app->store), &next_iter) == FALSE) {
		// no next item, so get prev iter
		nitems = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(app->store), NULL);
		if (nitems == 0) {
			// empty list, so just exit
			return;
		}
		ret = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(app->store), 
				&next_iter, NULL, nitems-1);
		g_assert(ret == TRUE);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(app->store), &next_iter);
		g_assert(path);
	}

	gtk_tree_view_set_cursor(GTK_TREE_VIEW(app->view), path, NULL, FALSE);
}

static void cb_action_remove(GtkWidget *widget, app_data *app)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkWidget *dialog;
	GtkWidget *label;
	cookie_t cookie;
	gchar key[100];
	gint ret;

	g_assert(app != NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->view));
	if (gtk_tree_selection_get_selected(selection, NULL, &iter)==FALSE) {
		return;
	}

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(app->window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE,
		"Remove alarm?");

	gtk_dialog_add_buttons(GTK_DIALOG(dialog), 
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		NULL);

	gtk_widget_show_all(GTK_WIDGET(dialog));

	app->widget_running = 1;
	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	app->widget_running = 0;
	if (ret != GTK_RESPONSE_OK) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, 
				COOKIE_COLUMN, &cookie,
				-1);
	alarm_event_del(cookie);
	cookie_to_gconf_key(cookie, key);
	gconf_client_unset(app->gconf, key, NULL);
	malarm_debug("removed alarm cookie %ld\n", cookie);
	remove_item(app, &iter);
}

static void cb_action_edit(GtkWidget *widget, app_data *app)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;

	g_assert(app != NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->view));
	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(app->store), &iter);
		cb_row_activated(GTK_TREE_VIEW(app->view), path, NULL, app);
		gtk_tree_path_free(path);
	}
}

static void cb_action_about(GtkWidget *widget, app_data *app)
{
	g_assert(app != NULL);

	gtk_show_about_dialog(GTK_WINDOW(app->window),
			"version", MALARM_VERSION,
			"copyright", "(c) 2008 Ronald Taneza <ronald.taneza@gmail.com>",
			NULL);
}

static gboolean toggled_timeout(gpointer data)
{
	GtkTreeIter iter;
	cookie_t cookie;
	cookie_t new_cookie;
	alarm_event_t *event = NULL;
	gchar key[100];
	int new_state;

	app_data *app = (app_data*)data;
	g_assert(app != NULL);

	iter = app->toggled_iter;
	gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter,
			ENABLED_COLUMN, &new_state,
			-1);
	new_state ^= 1;
	malarm_debug("new toggle state %d\n", new_state);

	gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, 
				COOKIE_COLUMN, &cookie,
				-1);
	event = alarm_event_get(cookie);
	if (event == NULL) {
		malarm_debug("error: unable to get alarm event of cookie %ld\n", cookie);
		goto cb_toggled_out;
	}

	if (new_state == FALSE) {
		time_t orig_time = event->alarm_time + event->snoozed*60;

		event->alarm_time = ALARM_DISABLED;
		event->flags = 0;
		event->snoozed = 0;
		new_cookie = alarm_event_add(event);
		if (new_cookie <= 0) {
			malarm_print("error setting alarm event, error code: '%d'\n", 
					alarmd_get_error());
			goto cb_toggled_out;
		}
		/* malarm_debug("new cookie %ld\n", new_cookie); */
		alarm_event_del(cookie);

		cookie_to_gconf_key(new_cookie, key);
		if (!gconf_client_set_int(app->gconf, key, orig_time, NULL)) {
			malarm_print("error: failed to set gconf key %s to %ld\n", 
					key, orig_time);
			goto cb_toggled_out;
		}

		show_banner(app, "Disabled alarm");
		malarm_debug("disabled cookie %ld, time %ld\n", cookie, orig_time);
		malarm_debug("set gconf key %s to %ld\n", key, orig_time);
		print_alarm_event(new_cookie, event);

	} else {

		cookie_to_gconf_key(cookie, key);
		event->alarm_time = gconf_client_get_int(app->gconf, key, NULL);
		if (event->alarm_time == 0) {
			malarm_print("error: failed to get gconf key %s\n", key);
			goto cb_toggled_out;
		}

		event->flags = ALARM_EVENT_FLAGS;
		new_cookie = alarm_event_add(event);
		if (new_cookie <= 0) {
			malarm_print("error setting alarm event, error code: '%d'\n", 
					alarmd_get_error());
			goto cb_toggled_out;
		}
		alarm_event_del(cookie);
		gconf_client_unset(app->gconf, key, NULL);

		show_banner(app, "Enabled alarm");
		malarm_debug("enabled cookie %ld, time %ld\n", cookie, event->alarm_time);
		print_alarm_event(new_cookie, event);
	}

	gtk_tree_store_set(GTK_TREE_STORE(app->store), &iter, 
			SNOOZE_COLUMN, SNOOZE_STRING(event->snoozed),
			ENABLED_COLUMN, new_state,
			COOKIE_COLUMN, new_cookie,
			ALARM_TIME_COLUMN, event->alarm_time,
			-1);

cb_toggled_out:
	if (event) alarm_event_free(event);
	gtk_widget_set_sensitive(GTK_WIDGET(app->view), TRUE);
	g_object_set(GTK_OBJECT(app->toggled_renderer), "activatable", TRUE, NULL);
	app->toggled_processing = 0;
	malarm_debug("toggled finished\n");

	return FALSE;
}

static void cb_toggled(GtkCellRendererToggle *renderer, gchar *path, app_data *app)
{
	g_assert(app != NULL);

	if (app->toggled_processing) return;

	malarm_debug("item %s toggled\n", path);

	g_object_set(GTK_OBJECT(app->toggled_renderer), "activatable", FALSE, NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(app->view), FALSE);

	if (!gtk_tree_model_get_iter_from_string(
				GTK_TREE_MODEL(app->store), &app->toggled_iter, path)) {
		malarm_print("error: unable to get iter from path: %s\n", path);
		return;
	}
 
	// for "key debouncing"
	app->toggled_processing = 1;
	g_timeout_add(KEY_DEBOUNCE_DELAY, toggled_timeout, app);
}

static gboolean cb_visibility(GtkWidget *widget, GdkEventVisibility *visibility, 
		app_data *app)
{
	g_assert(app != NULL);

	malarm_debug("visibility=%d, window is %s, %s\n", visibility->state,
			gtk_window_is_active(GTK_WINDOW(app->window)) ? "active" : "not active",
			hildon_window_get_is_topmost(HILDON_WINDOW(app->window)) ? 
			"topmost" : "not topmost");

	/* Repopulate only if:
	 * - app was previously completely not visible, and is now fully visible
	 * - some external widget (such as the alarm trigger dialog) obscured
	 *   the app, and app is now fully visible
	 */
	if (!app->widget_running &&

		(((app->visibility == GDK_VISIBILITY_FULLY_OBSCURED) &&
		(visibility->state == GDK_VISIBILITY_UNOBSCURED)) ||

		// old
		(((app->visibility == GDK_VISIBILITY_PARTIAL) &&
			app->window_active && app->window_topmost) &&
		// new
		((visibility->state == GDK_VISIBILITY_UNOBSCURED) &&
		 	!gtk_window_is_active(GTK_WINDOW(app->window)) &&
			hildon_window_get_is_topmost(HILDON_WINDOW(app->window)))))) {

		populate_tree(app);

	}

	app->visibility = visibility->state;
	app->window_active = gtk_window_is_active(GTK_WINDOW(app->window));
	app->window_topmost = hildon_window_get_is_topmost(HILDON_WINDOW(app->window));

	return TRUE;
}

static void cb_row_activated(GtkTreeView *view, GtkTreePath *path,
		GtkTreeViewColumn *column, app_data *app)
{
	GtkTreeIter iter;
	cookie_t old_cookie, new_cookie;
	alarm_event_t event;
	int ret;

	g_assert(app != NULL);

	/* malarm_debug("item %s activated\n", gtk_tree_path_to_string(path)); */
	if (!gtk_tree_model_get_iter(
				GTK_TREE_MODEL(app->store), &iter, path)) {
		malarm_print("error: unable to get iter from path: %s\n", 
				gtk_tree_path_to_string(path));
		return;
	}
	gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, 
				COOKIE_COLUMN, &old_cookie,
				-1);
	ret = alarm_dialog(app, old_cookie, &new_cookie, &event);
	if (ret == 0) {
		gtk_tree_store_remove(app->store, &iter);
		add_alarm_to_tree(app, new_cookie, &event);
		free(event.message);
		show_banner(app, "Updated alarm");
		malarm_debug("item %s: updated alarm: old cookie %ld, new_cookie %ld\n", 
				gtk_tree_path_to_string(path), old_cookie, new_cookie);
	}
}

static void stop_preview_sound(app_data *app)
{
	stop_sound(app);
	gtk_button_set_label(GTK_BUTTON(app->preview_button), GTK_STOCK_MEDIA_PLAY);
}

static void cb_sound_popup(GtkComboBox *combo_box, app_data *app)
{
	g_assert(app != NULL);
	stop_preview_sound(app);
}

static void cb_preview_clicked(GtkButton *button, app_data *app)
{
	int idx;

	g_assert(app != NULL);

	idx = gtk_combo_box_get_active(GTK_COMBO_BOX(app->sound_combo_box));
	g_assert((idx >=0) && (idx < ARRAY_SIZE(sounds_list)));

	if (!app->sound_playing) {
		play_sound(app, sounds_list[idx]);
		gtk_button_set_label(GTK_BUTTON(button), GTK_STOCK_MEDIA_STOP);
	} else {
		stop_preview_sound(app);
	}
}

static time_t get_new_alarm_time(void)
{
	time_t itm;
	time(&itm);
	if ((TIME_T_MAX - itm) < NEW_ALARM_TIME_INC) {
		itm = TIME_T_MAX;
	} else {
		itm += NEW_ALARM_TIME_INC;
	}
	return itm;
}

// if fcn returns 0, caller must free event->message after use
static int alarm_dialog(app_data *app, cookie_t old_cookie, 
		cookie_t *new_cookie, alarm_event_t *event)
{
	int ret = 0;
	GtkWidget *dialog;
	GtkWidget *caption;
	GtkWidget *hbox;
	GtkWidget *date_editor;
	GtkWidget *time_editor;
	GtkWidget *repeat_combo_box;
	GtkWidget *sound_combo_box;
	GtkWidget *preview_button;
	GtkWidget *message_entry;
	GtkSizeGroup *size_group;
	GtkSizeGroup *caption_size_group;
	gint result;
	time_t now;
	struct tm tnow;
	int is_old_event_disabled = 0;
	int i;
	GtkWidget *time_now_label;
	char time_now_buf[100];
	gchar *time_now_string;

	g_assert(app != NULL);
	g_assert(old_cookie >= 0);
	g_assert(new_cookie != NULL);
	g_assert(event != NULL);

	dialog = gtk_dialog_new_with_buttons(
			(old_cookie == 0) ? "Add alarm" : "Edit alarm", 
			GTK_WINDOW(app->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NULL);

	// to align widgets
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	caption_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	// time now
	now = time(NULL);
	localtime_r(&now, &tnow);
	date_to_string(&tnow, time_now_buf, 0);
	time_now_string = g_strconcat(
			"<span size='smaller' style='italic'>Time now: ", 
			time_now_buf, "</span>", NULL);
	time_now_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(time_now_label), time_now_string);
	/* gtk_size_group_add_widget(size_group, time_now_label); */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), time_now_label, FALSE, FALSE, 2);
	g_free(time_now_string);
	
	// time editor
	hbox = gtk_hbox_new(FALSE, 0);
	time_editor = hildon_time_editor_new();
	now = get_new_alarm_time();
	localtime_r(&now, &tnow);
	hildon_time_editor_set_time(HILDON_TIME_EDITOR(time_editor), 
			tnow.tm_hour, tnow.tm_min, tnow.tm_sec);
	gtk_size_group_add_widget(size_group, time_editor);
	caption = hildon_caption_new(caption_size_group, "Time", time_editor, 
			NULL, HILDON_CAPTION_MANDATORY);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, FALSE, FALSE, 2);

	// date editor
	hbox = gtk_hbox_new(FALSE, 0);
	date_editor = hildon_date_editor_new();
	hildon_date_editor_set_date(HILDON_DATE_EDITOR(date_editor), 
			tnow.tm_year+1900, tnow.tm_mon+1, tnow.tm_mday);
	gtk_size_group_add_widget(size_group, date_editor);
	caption = hildon_caption_new(caption_size_group, "Date", date_editor, 
			NULL, HILDON_CAPTION_MANDATORY);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, FALSE, FALSE, 2);

	// repeat
	hbox = gtk_hbox_new(FALSE, 0);
	repeat_combo_box = gtk_combo_box_new_text();
	for (i=0; i<ARRAY_SIZE(repeat_list); i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(repeat_combo_box), repeat_list[i].text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(repeat_combo_box), 0);
	gtk_size_group_add_widget(size_group, repeat_combo_box);
	caption = hildon_caption_new(caption_size_group, "Repeat", repeat_combo_box, 
			NULL, HILDON_CAPTION_MANDATORY);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, FALSE, FALSE, 2);

	// sound
	hbox = gtk_hbox_new(FALSE, 0);
	sound_combo_box = gtk_combo_box_new_text();
	app->sound_combo_box = sound_combo_box;
	gtk_combo_box_append_text(GTK_COMBO_BOX(sound_combo_box), "Alarm 1");
	gtk_combo_box_append_text(GTK_COMBO_BOX(sound_combo_box), "Alarm 2");
	gtk_combo_box_append_text(GTK_COMBO_BOX(sound_combo_box), "Alarm 3");
	gtk_combo_box_append_text(GTK_COMBO_BOX(sound_combo_box), "None");
	gtk_combo_box_set_active(GTK_COMBO_BOX(sound_combo_box), app->sound_idx);
	g_signal_connect(G_OBJECT(sound_combo_box), "popup", 
			G_CALLBACK(cb_sound_popup), app);

	// todo: display icon instead of text?
	// browse for other sound files
	preview_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);
	app->preview_button = preview_button;
	g_signal_connect(G_OBJECT(preview_button), "clicked", 
			G_CALLBACK(cb_preview_clicked), app);
	gtk_box_pack_start(GTK_BOX(hbox), sound_combo_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), preview_button, FALSE, FALSE, 2);
	gtk_size_group_add_widget(size_group, hbox);
	caption = hildon_caption_new(caption_size_group, "Sound", hbox, NULL, 
			HILDON_CAPTION_MANDATORY);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, FALSE, FALSE, 2);

	// message
	hbox = gtk_hbox_new(FALSE, 0);
	message_entry = gtk_entry_new();
	gtk_size_group_add_widget(size_group, message_entry);
	caption = hildon_caption_new(caption_size_group, "Message", message_entry, 
			NULL, HILDON_CAPTION_MANDATORY);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), caption, FALSE, FALSE, 2);

	if (old_cookie > 0) {
		int idx;
		alarm_event_t *old_event;

		old_event = alarm_event_get(old_cookie);
		if (old_event == NULL) {
			malarm_print("error: unable to get alarm event of cookie %ld\n", 
					old_cookie);
			return -1;
		}

		if (old_event->alarm_time == ALARM_DISABLED) {
			old_event->alarm_time = get_actual_alarm_time(app, old_cookie);
			if (old_event->alarm_time < 0) {
				alarm_event_free(old_event);
				return -1;
			}
			is_old_event_disabled = 1;
		}

		/* localtime_r(&old_event->alarm_time, &tnow); */
		get_next_alarm_time(old_event, &tnow);

		hildon_time_editor_set_time(HILDON_TIME_EDITOR(time_editor), 
				tnow.tm_hour, tnow.tm_min, tnow.tm_sec);
		hildon_date_editor_set_date(HILDON_DATE_EDITOR(date_editor), 
				tnow.tm_year+1900, tnow.tm_mon+1, tnow.tm_mday);

		for (idx=0; idx<ARRAY_SIZE(repeat_list); idx++) {
			if (old_event->recurrence == repeat_list[idx].val) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(repeat_combo_box), idx);
				break;
			}
		}

		for (idx=0; idx<ARRAY_SIZE(sounds_list); idx++) {
			if (strcmp(old_event->sound, sounds_list[idx])==0) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(sound_combo_box), idx);
				break;
			}
		}

		if (old_event->message) {
			gtk_entry_set_text(GTK_ENTRY(message_entry), old_event->message);
		}

		alarm_event_free(old_event);
	}


	app->widget_running = 1;
	gtk_widget_show_all(GTK_WIDGET(GTK_DIALOG(dialog)->vbox));

wait_again:
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	app->widget_running = 0;
	stop_preview_sound(app);
	if (result != GTK_RESPONSE_OK) {
		ret = -1;
		goto alarm_dialog_out;
	}

	{
	guint year, month, day;
	guint hours, minutes, seconds;
	gint repeat_idx;
	/* gint sound_idx; */
	const gchar *message;
	time_t itm;
	struct tm stm;
	time_t orig_time;

	// get alarm info from widgets
	hildon_date_editor_get_date(HILDON_DATE_EDITOR(date_editor), 
			&year, &month, &day);
	hildon_time_editor_get_time(HILDON_TIME_EDITOR(time_editor), 
			&hours, &minutes, &seconds);
	repeat_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(repeat_combo_box));
	app->sound_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(sound_combo_box));
	g_assert((app->sound_idx >= 0) && (app->sound_idx < ARRAY_SIZE(sounds_list)));
	message = gtk_entry_get_text(GTK_ENTRY(message_entry));

	// initialize the alarm event
	memset(event, 0, sizeof(alarm_event_t));
	time(&itm);
	itm = (itm/60)*60; // ignore seconds
	localtime_r(&itm, &stm); // just to properly init stm fields
	stm.tm_year = year - 1900;
	stm.tm_mon = month - 1;
	stm.tm_mday = day;
	stm.tm_hour = hours;
	stm.tm_min = minutes;
	/* stm.tm_sec = seconds; */
	stm.tm_sec = 0;
	event->alarm_time = mktime(&stm);
	/* event->alarm_time = 0; // jan 1, 1970 07:30am */
	/* print_stm(&stm); */

	if (event->alarm_time < itm) {
		show_banner(app, "Cannot set alarm for time in the past");
		goto wait_again;
	} else if (event->alarm_time == ALARM_DISABLED) {
		show_banner(app, "Cannot set alarm at the end of time!");
		goto wait_again;
	}

	g_assert((repeat_idx >= 0) && (repeat_idx < ARRAY_SIZE(repeat_list)));
	event->recurrence = repeat_list[repeat_idx].val;
	event->recurrence_count = (repeat_idx == REPEAT_ONCE) ? 0 : -1;

	event->title = MALARM_NAME;
	event->message = alarm_escape_string(message);
	event->sound = sounds_list[app->sound_idx];
	event->icon = "qgn_list_hclk_alarm";
	event->flags = ALARM_EVENT_FLAGS;
	event->dbus_interface = MALARM_DBUS_NAME;
	event->dbus_service = MALARM_DBUS_NAME;
	event->dbus_path = MALARM_DBUS_PATH;
	event->dbus_name = "alarm_triggered";
	event->exec_name = NULL;

	orig_time = event->alarm_time;
	if (is_old_event_disabled) {
		event->alarm_time = ALARM_DISABLED;
		event->flags = 0;
	}

	/* malarm_debug("adding alarm event\n"); */
	*new_cookie = alarm_event_add(event);
	if (*new_cookie <= 0) {
		malarm_debug("Error setting alarm event. Error code: '%d'\n", 
				alarmd_get_error());
		ret = -1;
		goto alarm_dialog_out;
	}

	if (old_cookie > 0) {
		char key[50];

		alarm_event_del(old_cookie);

		// update gconf key
		cookie_to_gconf_key(old_cookie, key);
		gconf_client_unset(app->gconf, key, NULL);
		cookie_to_gconf_key(*new_cookie, key);
		if (!gconf_client_set_int(app->gconf, key, orig_time, NULL)) {
			malarm_print("error: failed to set gconf key %s to %ld\n", 
					key, orig_time);
			ret = -1;
			goto alarm_dialog_out;
		}
	}

	print_alarm_event(*new_cookie, event);
	}

alarm_dialog_out:
	// todo: do I need to free something???
	gtk_widget_destroy(dialog);
	return ret;
}

static int add_alarm_to_tree(app_data *app, cookie_t cookie, alarm_event_t *event)
{
	char *newline;
	struct tm stm;
	int enabled = TRUE;
	char *repeat = "Other";
	GtkTreeIter *piter, iter, new_iter;
	cookie_t tcookie;
	GtkTreePath *path;
	int i;
	char buf[100];


	if (event->alarm_time == ALARM_DISABLED) {
		event->alarm_time = get_actual_alarm_time(app, cookie);
		if (event->alarm_time < 0) {
			return -1;
		}
		enabled = FALSE;
	}

	get_next_alarm_time(event, &stm);
	date_to_string(&stm, buf, DATE_TO_STRING_WDAY);

	for (i=0; i<ARRAY_SIZE(repeat_list); i++) {
		if (event->recurrence == repeat_list[i].val) {
			repeat = repeat_list[i].text;
			break;
		}
	}

	// insert at correct position based on sorted cookies
	piter = NULL;
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(app->store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(app->store), &iter, 
					COOKIE_COLUMN, &tcookie,
					-1);
			if (cookie < tcookie) {
				piter = &iter;
				break;
			}
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(app->store), &iter));
	}

	gtk_tree_store_insert_before(app->store, &new_iter, NULL, piter);
	gtk_tree_store_set(app->store, &new_iter,
			SNOOZE_COLUMN, SNOOZE_STRING(event->snoozed),
			ENABLED_COLUMN, enabled,
			TIME_STRING_COLUMN, buf,
			REPEAT_COLUMN, repeat,
			MESSAGE_COLUMN, alarm_unescape_string_noalloc(event->message),
			COOKIE_COLUMN, cookie,
			ALARM_TIME_COLUMN, event->alarm_time,
			-1);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(app->store), &new_iter);
	g_assert(path);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(app->view), path, NULL, FALSE);

	return 0;
}

void populate_tree(app_data *app)
{
	cookie_t *cookie;
	alarm_event_t *event = NULL;
	/* time_t itm; */

	malarm_debug("start\n");

	gtk_tree_store_clear(GTK_TREE_STORE(app->store));

	/* time(&itm); */

	// also need to show snoozed alarms, which have alarm_time in the past
	/* cookie = alarm_event_query(itm, TIME_T_MAX, 0, 0); */
	cookie = alarm_event_query(0, TIME_T_MAX, 0, 0);
	while (cookie && *cookie) {
		if (event = alarm_event_get(*cookie)) {
			if (strcmp(event->title, MALARM_NAME) == 0) {
				print_alarm_event(*cookie, event);
				if (add_alarm_to_tree(app, *cookie, event) != 0) {
					// cannot find actual time of disabled alarm
					alarm_event_del(*cookie);
					malarm_debug("removed alarm cookie %d\n", *cookie);
				}
			}
			alarm_event_free(event);
		}
		cookie++;
	}
}

static void create_tree(app_data *app)
{
	GtkTreeStore *store;
	GtkWidget *view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *swindow;

	store = gtk_tree_store_new(N_COLUMNS, 
			G_TYPE_STRING, 
			G_TYPE_BOOLEAN, 
			G_TYPE_STRING, 
			G_TYPE_STRING, 
			G_TYPE_STRING,
			G_TYPE_LONG,
			G_TYPE_LONG);
	app->store = store;

	view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);
	app->view = view;

	/* snooze */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			" ", renderer, "text", SNOOZE_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	/* enable checkbox */
	renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(
			" ", renderer, "active", ENABLED_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	app->cb_toggled_handler_id = 
		g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(cb_toggled), app);
	app->toggled_renderer = renderer;

	/* time string */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"Time", renderer, "text", TIME_STRING_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	/* recurrence */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"Repeat", renderer, "text", REPEAT_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	/* alarm message */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
			"Message", renderer, "text", MESSAGE_COLUMN, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

	g_signal_connect(G_OBJECT(view), "row-activated", G_CALLBACK(cb_row_activated), app);
 
	g_signal_connect(G_OBJECT(app->view), "visibility-notify-event",
					 G_CALLBACK(cb_visibility), app);

	// scrolled window
	swindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(swindow), GTK_WIDGET(view));

	gtk_container_add(GTK_CONTAINER(app->window), GTK_WIDGET(swindow));

	populate_tree(app);
}

static void create_toolbar(app_data *app) 
{
	GtkToolbar* toolbar;
	GtkToolItem* tb_add;
	GtkToolItem* tb_remove;
	GtkToolItem* tb_edit;

	g_assert(app != NULL);

	tb_add = gtk_tool_button_new_from_stock(GTK_STOCK_ADD);
	tb_remove = gtk_tool_button_new_from_stock(GTK_STOCK_REMOVE);
	tb_edit = gtk_tool_button_new_from_stock(GTK_STOCK_EDIT);

	// required for BOTH_HORIZ style to work!
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(tb_add), TRUE);
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(tb_remove), TRUE);
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(tb_edit), TRUE);

	toolbar = GTK_TOOLBAR(gtk_toolbar_new());
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	gtk_toolbar_insert(toolbar, tb_add, -1);
	gtk_toolbar_insert(toolbar, tb_remove, -1);
	gtk_toolbar_insert(toolbar, tb_edit, -1);

	gtk_widget_show_all(GTK_WIDGET(toolbar));
	g_signal_connect(G_OBJECT(tb_add), "clicked", G_CALLBACK(cb_action_add), app);
	g_signal_connect(G_OBJECT(tb_remove), "clicked", G_CALLBACK(cb_action_remove), app);
	g_signal_connect(G_OBJECT(tb_edit), "clicked", G_CALLBACK(cb_action_edit), app);

	hildon_window_add_toolbar(HILDON_WINDOW(app->window), GTK_TOOLBAR(toolbar));
}

static void create_menu(app_data *app) 
{
	GtkWidget *main_menu;
	GtkWidget *add_item;
	GtkWidget *remove_item;
	GtkWidget *edit_item;
	GtkWidget *about_item;

	main_menu = gtk_menu_new();

	add_item = gtk_image_menu_item_new_with_label("Add alarm");
	remove_item = gtk_image_menu_item_new_with_label("Remove alarm");
	edit_item = gtk_image_menu_item_new_with_label("Edit alarm");
	about_item = gtk_image_menu_item_new_with_label("About");

	gtk_menu_append(main_menu, add_item);
	gtk_menu_append(main_menu, remove_item);
	gtk_menu_append(main_menu, edit_item);
	gtk_menu_append(main_menu, about_item);

	g_signal_connect(G_OBJECT(add_item), "activate",
			G_CALLBACK(cb_action_add), app);
	g_signal_connect(G_OBJECT(remove_item), "activate",
			G_CALLBACK(cb_action_remove), app);
	g_signal_connect(G_OBJECT(edit_item), "activate",
			G_CALLBACK(cb_action_edit), app);
	g_signal_connect(G_OBJECT(about_item), "activate",
			G_CALLBACK(cb_action_about), app);

	hildon_window_set_menu(app->window, GTK_MENU(main_menu));

	gtk_widget_show_all(GTK_WIDGET(main_menu));
}

void create_ui(app_data *app)
{
	create_toolbar(app);
	create_menu(app);
	create_tree(app);
}

