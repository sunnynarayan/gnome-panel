/*
 * panel-applets-manager-dbus.c
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2010 Vincent Untz <vuntz@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <gio/gio.h>
#include <gmodule.h>
#include <string.h>

#include <panel-applets-manager.h>

#include "panel-applet-frame-dbus.h"

#include "panel-applets-manager-dbus.h"

G_DEFINE_TYPE_WITH_CODE (PanelAppletsManagerDBus,
			 panel_applets_manager_dbus,
			 PANEL_TYPE_APPLETS_MANAGER,
			 g_io_extension_point_implement (PANEL_APPLETS_MANAGER_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "dbus",
							 10))

struct _PanelAppletsManagerDBusPrivate
{
	GHashTable *applet_factories;
	GList      *monitors;
};

typedef gint (* ActivateAppletFunc) (void);
typedef GtkWidget * (* GetAppletWidgetFunc) (const gchar *factory_id,
                                             guint        uid);

typedef struct _PanelAppletFactoryInfo {
	gchar              *id;
	gchar              *location;
	gboolean            in_process;
	GModule            *module;
	ActivateAppletFunc  activate_applet;
	GetAppletWidgetFunc get_applet_widget;
	guint               n_applets;

	gchar              *srcdir;

	GList              *applet_list;
	gboolean            has_old_ids;
} PanelAppletFactoryInfo;

#define PANEL_APPLET_FACTORY_GROUP "Applet Factory"
#define PANEL_APPLETS_EXTENSION    ".panel-applet"

static void
panel_applet_factory_info_free (PanelAppletFactoryInfo *info)
{
	if (!info)
		return;

	g_free (info->id);
	g_free (info->location);
	g_list_foreach (info->applet_list,
			(GFunc) panel_applet_info_free,
			NULL);
	g_list_free (info->applet_list);
	info->applet_list = NULL;
	g_free (info->srcdir);

	g_slice_free (PanelAppletFactoryInfo, info);
}

static PanelAppletInfo *
_panel_applets_manager_get_applet_info (GKeyFile    *applet_file,
					const gchar *group,
					const gchar *factory_id)
{
	PanelAppletInfo  *info;
	char             *iid;
	char             *name;
	char             *comment;
	char             *icon;
	char            **old_ids;

	iid = g_strdup_printf ("%s::%s", factory_id, group);
	name = g_key_file_get_locale_string (applet_file, group,
					     "Name", NULL, NULL);
	comment = g_key_file_get_locale_string (applet_file, group,
						"Description", NULL, NULL);
	icon = g_key_file_get_string (applet_file, group, "Icon", NULL);
	/* Bonobo compatibility */
	old_ids = g_key_file_get_string_list (applet_file, group,
					      "BonoboId", NULL, NULL);

	info = panel_applet_info_new (iid, name, comment, icon, (const char **) old_ids);

	g_free (iid);
	g_free (name);
	g_free (comment);
	g_free (icon);
	g_strfreev (old_ids);

	return info;
}

static PanelAppletFactoryInfo *
panel_applets_manager_get_applet_factory_info_from_file (const gchar *filename)
{
	PanelAppletFactoryInfo *info;
	GKeyFile               *applet_file;
	gchar                 **groups;
	gsize                   n_groups;
	gint                    i;
	GError                 *error = NULL;

	applet_file = g_key_file_new ();
	if (!g_key_file_load_from_file (applet_file, filename, G_KEY_FILE_NONE, &error)) {
		g_warning ("Error opening panel applet file %s: %s",
			   filename, error->message);
		g_error_free (error);
		g_key_file_free (applet_file);

		return NULL;
	}

	info = g_slice_new0 (PanelAppletFactoryInfo);
	info->id = g_key_file_get_string (applet_file, PANEL_APPLET_FACTORY_GROUP, "Id", NULL);
	if (!info->id) {
		g_warning ("Bad panel applet file %s: Could not find 'Id' in group '%s'",
			   filename, PANEL_APPLET_FACTORY_GROUP);
		panel_applet_factory_info_free (info);
		g_key_file_free (applet_file);

		return NULL;
	}

	info->in_process = g_key_file_get_boolean (applet_file, PANEL_APPLET_FACTORY_GROUP,
						   "InProcess", NULL);
	if (info->in_process) {
		info->location = g_key_file_get_string (applet_file, PANEL_APPLET_FACTORY_GROUP,
							"Location", NULL);
		if (!info->location) {
			g_warning ("Bad panel applet file %s: In-process applet without 'Location'",
				   filename);
			panel_applet_factory_info_free (info);
			g_key_file_free (applet_file);

			return NULL;
		}
	}

	info->has_old_ids = FALSE;

	groups = g_key_file_get_groups (applet_file, &n_groups);
	for (i = 0; i < n_groups; i++) {
		PanelAppletInfo *ainfo;

		if (g_strcmp0 (groups[i], PANEL_APPLET_FACTORY_GROUP) == 0)
			continue;

		ainfo = _panel_applets_manager_get_applet_info (applet_file,
								groups[i], info->id);
		if (panel_applet_info_get_old_ids (ainfo) != NULL)
			info->has_old_ids = TRUE;

		info->applet_list = g_list_prepend (info->applet_list, ainfo);
	}
	g_strfreev (groups);

	g_key_file_free (applet_file);

	if (!info->applet_list) {
		panel_applet_factory_info_free (info);
		return NULL;
	}

	info->srcdir = g_path_get_dirname (filename);

	return info;
}

static GSList *
panel_applets_manager_get_applets_dirs (void)
{
	const gchar *dir = NULL;
	gchar      **paths;
	guint        i;
	GSList      *retval = NULL;

	dir = g_getenv ("GNOME_PANEL_APPLETS_DIR");
	if (!dir || g_strcmp0 (dir, "") == 0) {
		return g_slist_prepend (NULL, g_strdup (PANEL_APPLETS_DIR));
	}

	paths = g_strsplit (dir, ":", 0);
	for (i = 0; paths[i]; i++) {
		if (g_slist_find_custom (retval, paths[i], (GCompareFunc) g_strcmp0))
			continue;
		retval = g_slist_prepend (retval, g_strdup (paths[i]));
	}
	g_strfreev (paths);

	return g_slist_reverse (retval);
}

static void
applets_directory_changed (GFileMonitor     *monitor,
			   GFile            *file,
			   GFile            *other_file,
			   GFileMonitorEvent event_type,
			   gpointer          user_data)
{
	PanelAppletsManagerDBus *manager = PANEL_APPLETS_MANAGER_DBUS (user_data);

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CHANGED:
	case G_FILE_MONITOR_EVENT_CREATED: {
		PanelAppletFactoryInfo *info;
		PanelAppletFactoryInfo *old_info;
		gchar                  *filename;
		GSList                 *dirs, *d;

		filename = g_file_get_path (file);
		if (!g_str_has_suffix (filename, PANEL_APPLETS_EXTENSION)) {
			g_free (filename);
			return;
		}

		info = panel_applets_manager_get_applet_factory_info_from_file (filename);
		g_free (filename);

		if (!info)
			return;

		old_info = g_hash_table_lookup (manager->priv->applet_factories, info->id);
		if (!old_info) {
			/* New applet, just insert it */
			g_hash_table_insert (manager->priv->applet_factories, g_strdup (info->id), info);
			return;
		}

		/* Make sure we don't update an applet that has changed in
		 * another source dir unless it takes precedence over the
		 * current one */
		if (g_strcmp0 (info->srcdir, old_info->srcdir) == 0) {
			g_hash_table_replace (manager->priv->applet_factories, g_strdup (info->id), info);
			return;
		}

		dirs = panel_applets_manager_get_applets_dirs ();

		for (d = dirs; d; d = g_slist_next (d)) {
			gchar *path = (gchar *) d->data;

			if (g_strcmp0 (path, old_info->srcdir) == 0) {
				panel_applet_factory_info_free (info);
				break;
			} else if (g_strcmp0 (path, info->srcdir) == 0) {
				g_hash_table_replace (manager->priv->applet_factories, g_strdup (info->id), info);
				break;
			}
		}

		g_slist_foreach (dirs, (GFunc) g_free, NULL);
		g_slist_free (dirs);
	}
		break;
	default:
		/* Ignore any other change */
		break;
	}
}

static void
panel_applets_manager_dbus_load_applet_infos (PanelAppletsManagerDBus *manager)
{
	GSList      *dirs, *d;
	GDir        *dir;
	const gchar *dirent;
	GError      *error = NULL;

	dirs = panel_applets_manager_get_applets_dirs ();
	for (d = dirs; d; d = g_slist_next (d)) {
		GFileMonitor *monitor;
		GFile        *dir_file;
		gchar        *path = (gchar *) d->data;

		dir = g_dir_open (path, 0, &error);
		if (!dir) {
			g_warning ("%s", error->message);
			g_error_free (error);
			g_free (path);

			continue;
		}

		/* Monitor dir */
		dir_file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (dir_file,
						    G_FILE_MONITOR_NONE,
						    NULL, NULL);
		if (monitor) {
			g_signal_connect (monitor, "changed",
					  G_CALLBACK (applets_directory_changed),
					  manager);
			manager->priv->monitors = g_list_prepend (manager->priv->monitors, monitor);
		}
		g_object_unref (dir_file);

		while ((dirent = g_dir_read_name (dir))) {
			PanelAppletFactoryInfo *info;
			gchar                  *file;

			if (!g_str_has_suffix (dirent, PANEL_APPLETS_EXTENSION))
				continue;

			file = g_build_filename (path, dirent, NULL);
			info = panel_applets_manager_get_applet_factory_info_from_file (file);
			g_free (file);

			if (!info)
				continue;

			if (g_hash_table_lookup (manager->priv->applet_factories, info->id)) {
				panel_applet_factory_info_free (info);
				continue;
			}

			g_hash_table_insert (manager->priv->applet_factories, g_strdup (info->id), info);
		}

		g_dir_close (dir);
		g_free (path);
	}

	g_slist_free (dirs);
}

static GList *
panel_applets_manager_dbus_get_applets (PanelAppletsManager *manager)
{
	PanelAppletsManagerDBus *dbus_manager = PANEL_APPLETS_MANAGER_DBUS (manager);

	GHashTableIter iter;
	gpointer       key, value;
	GList         *retval = NULL;

	g_hash_table_iter_init (&iter, dbus_manager->priv->applet_factories);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PanelAppletFactoryInfo *info;

		info = (PanelAppletFactoryInfo *) value;
		retval = g_list_concat (retval, g_list_copy (info->applet_list));
	}

	return retval;
}

static PanelAppletFactoryInfo *
get_applet_factory_info (PanelAppletsManager *manager,
			 const gchar         *iid)
{
	PanelAppletsManagerDBus *dbus_manager = PANEL_APPLETS_MANAGER_DBUS (manager);

	PanelAppletFactoryInfo *info;
	const gchar            *sp;
	gchar                  *factory_id;

	sp = g_strrstr (iid, "::");
	if (!sp)
		return NULL;

	factory_id = g_strndup (iid, strlen (iid) - strlen (sp));
	info = g_hash_table_lookup (dbus_manager->priv->applet_factories, factory_id);
	g_free (factory_id);

	return info;
}

static gboolean
panel_applets_manager_dbus_factory_activate (PanelAppletsManager *manager,
					     const gchar         *iid)
{
	PanelAppletFactoryInfo *info;
	ActivateAppletFunc      activate_applet;
	GetAppletWidgetFunc     get_applet_widget;

	info = get_applet_factory_info (manager, iid);
	if (!info)
		return FALSE;

	/* Out-of-process applets are activated by the session bus */
	if (!info->in_process)
		return TRUE;

	if (info->module) {
		if (info->n_applets == 0) {
			if (info->activate_applet () != 0) {
				g_warning ("Failed to reactivate factory %s\n", iid);
				return FALSE;
			}
		}
		info->n_applets++;

		return TRUE;
	}

	info->module = g_module_open (info->location, G_MODULE_BIND_LAZY);
	if (!info->module) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s: %s\n",
			   iid, g_module_error ());
		return FALSE;
	}

	if (!g_module_symbol (info->module, "_panel_applet_shlib_factory", (gpointer *) &activate_applet)) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s: %s\n",
			   iid, g_module_error ());
		g_module_close (info->module);
		info->module = NULL;

		return FALSE;
	}

	if (!g_module_symbol (info->module, "panel_applet_get_applet_widget", (gpointer *) &get_applet_widget)) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s: %s", iid, g_module_error ());
		g_module_close (info->module);
		info->module = NULL;

		return FALSE;
	}

	/* Activate the applet */
	if (activate_applet () != 0) {
		/* FIXME: use a GError? */
		g_warning ("Failed to load applet %s\n", iid);
		g_module_close (info->module);
		info->module = NULL;

		return FALSE;
	}
	info->activate_applet = activate_applet;
	info->get_applet_widget = get_applet_widget;

	info->n_applets = 1;

	return TRUE;
}

static gboolean
panel_applets_manager_dbus_factory_deactivate (PanelAppletsManager *manager,
					       const gchar         *iid)
{
	PanelAppletFactoryInfo *info;

	info = get_applet_factory_info (manager, iid);
	if (!info)
		return FALSE;

	/* Out-of-process applets are deactivated by the session bus */
	if (!info->in_process)
		return TRUE;

	if (!info->module)
		return TRUE;

	info->n_applets--;
	if (info->n_applets == 0) {
		/* FIXME: we should close the module here, however applet types
		 * are registered static */
#if 0
		g_module_close (info->module);
		info->module = NULL;
#endif
	}

	return TRUE;
}

static PanelAppletInfo *
panel_applets_manager_dbus_get_applet_info (PanelAppletsManager *manager,
					    const gchar         *iid)
{
	PanelAppletFactoryInfo *info;
	GList                  *l;

	info = get_applet_factory_info (manager, iid);
	if (!info)
		return NULL;

	for (l = info->applet_list; l; l = g_list_next (l)) {
		PanelAppletInfo *ainfo = (PanelAppletInfo *) l->data;

		if (g_strcmp0 (panel_applet_info_get_iid (ainfo), iid) == 0)
			return ainfo;
	}

	return NULL;
}

static PanelAppletInfo *
panel_applets_manager_dbus_get_applet_info_from_old_id (PanelAppletsManager *manager,
							const gchar         *iid)
{
	PanelAppletsManagerDBus *dbus_manager = PANEL_APPLETS_MANAGER_DBUS (manager);

	GHashTableIter iter;
	gpointer       key, value;

	g_hash_table_iter_init (&iter, dbus_manager->priv->applet_factories);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		PanelAppletFactoryInfo *info;
		GList                  *l;

		info = (PanelAppletFactoryInfo *) value;
		if (!info->has_old_ids)
			continue;

		for (l = info->applet_list; l; l = g_list_next (l)) {
			PanelAppletInfo *ainfo;
			gint             i = 0;
			const gchar * const *old_ids;

			ainfo = (PanelAppletInfo *) l->data;

			old_ids = panel_applet_info_get_old_ids (ainfo);

			if (old_ids == NULL)
				continue;

			while (old_ids[i]) {
				if (g_strcmp0 (old_ids[i], iid) == 0)
					return ainfo;
				i++;
			}
		}
	}

	return NULL;
}

static gboolean
panel_applets_manager_dbus_load_applet (PanelAppletsManager         *manager,
					const gchar                 *iid,
					PanelAppletFrameActivating  *frame_act)
{
	return panel_applet_frame_dbus_load (iid, frame_act);
}

static GtkWidget *
panel_applets_manager_dbus_get_applet_widget (PanelAppletsManager *manager,
                                              const gchar         *iid,
                                              guint                uid)
{
	PanelAppletFactoryInfo *info;

	info = get_applet_factory_info (manager, iid);
	if (!info)
		return NULL;

	return info->get_applet_widget (info->id, uid);
}

static void
panel_applets_manager_dbus_finalize (GObject *object)
{
	PanelAppletsManagerDBus *manager = PANEL_APPLETS_MANAGER_DBUS (object);

	if (manager->priv->monitors) {
		g_list_foreach (manager->priv->monitors, (GFunc) g_object_unref, NULL);
		g_list_free (manager->priv->monitors);
		manager->priv->monitors = NULL;
	}

	if (manager->priv->applet_factories) {
		g_hash_table_destroy (manager->priv->applet_factories);
		manager->priv->applet_factories = NULL;
	}

	G_OBJECT_CLASS (panel_applets_manager_dbus_parent_class)->finalize (object);
}

static void
panel_applets_manager_dbus_init (PanelAppletsManagerDBus *manager)
{
	manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager,
						     PANEL_TYPE_APPLETS_MANAGER_DBUS,
						     PanelAppletsManagerDBusPrivate);

	manager->priv->applet_factories = g_hash_table_new_full (g_str_hash,
								 g_str_equal,
								 (GDestroyNotify) g_free,
								 (GDestroyNotify) panel_applet_factory_info_free);

	panel_applets_manager_dbus_load_applet_infos (manager);
}

static void
panel_applets_manager_dbus_class_init (PanelAppletsManagerDBusClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	PanelAppletsManagerClass *manager_class = PANEL_APPLETS_MANAGER_CLASS (class);

	gobject_class->finalize = panel_applets_manager_dbus_finalize;

	manager_class->get_applets = panel_applets_manager_dbus_get_applets;
	manager_class->factory_activate = panel_applets_manager_dbus_factory_activate;
	manager_class->factory_deactivate = panel_applets_manager_dbus_factory_deactivate;
	manager_class->get_applet_info = panel_applets_manager_dbus_get_applet_info;
	manager_class->get_applet_info_from_old_id = panel_applets_manager_dbus_get_applet_info_from_old_id;
	manager_class->load_applet = panel_applets_manager_dbus_load_applet;
	manager_class->get_applet_widget = panel_applets_manager_dbus_get_applet_widget;

	g_type_class_add_private (class, sizeof (PanelAppletsManagerDBusPrivate));
}
