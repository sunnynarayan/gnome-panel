/* -*- c-basic-offset: 8; indent-tabs-mode: t -*-
 * Copyright (C) 1997 - 2000 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2004 Red Hat Inc.
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

#include "menu.h"

#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#include <libpanel-util/panel-glib.h>
#include <libpanel-util/panel-keyfile.h>
#include <libpanel-util/panel-xdg.h>

#include "launcher.h"
#include "panel-util.h"
#include "panel.h"
#include "panel-stock-icons.h"
#include "panel-action-button.h"
#include "panel-menu-button.h"
#include "panel-menu-items.h"
#include "panel-globals.h"
#include "panel-run-dialog.h"
#include "panel-lockdown.h"
#include "panel-icon-names.h"

typedef struct {
	GtkWidget    *pixmap;
	GIcon        *gicon;
	char         *image;
	char         *fallback_image;
	GtkIconTheme *icon_theme;
	GtkIconSize   icon_size;
} IconToLoad;

typedef struct {
	GtkWidget   *image;
	GIcon       *gicon;
	GdkPixbuf   *pixbuf;
	GtkIconSize  icon_size;
} IconToAdd;

static guint load_icons_id = 0;
static GHashTable *loaded_icons = NULL;
static GList *icons_to_load = NULL;
static GList *icons_to_add = NULL;

static GSList *image_menu_items = NULL;

static GtkWidget *populate_menu_from_directory (GtkWidget          *menu,
						GMenuTreeDirectory *directory);

static void panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
					    GtkIconSize  icon_size,
					    GIcon       *gicon,
					    const char  *image_filename,
					    const char  *fallback_image_filename);

GtkWidget *
add_menu_separator (GtkWidget *menu)
{
	GtkWidget *menuitem;
	
	menuitem = gtk_separator_menu_item_new ();
	gtk_widget_set_sensitive (menuitem, FALSE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	return menuitem;
}

static void
activate_app_def (GtkWidget      *menuitem,
		  GMenuTreeEntry *entry)
{
	const char       *path;

	path = gmenu_tree_entry_get_desktop_file_path (entry);
	panel_menu_item_activate_desktop_file (menuitem, path);
}

PanelWidget *
menu_get_panel (GtkWidget *menu)
{
	PanelWidget *retval = NULL;

	g_return_val_if_fail (menu != NULL, NULL);

	if (GTK_IS_MENU_ITEM (menu))
		menu = gtk_widget_get_parent (menu);

	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	while (menu) {
		retval = g_object_get_data (G_OBJECT (menu), "menu_panel");
		if (retval)
			break;

		menu = gtk_widget_get_parent (gtk_menu_get_attach_widget (GTK_MENU (menu)));
		if (!GTK_IS_MENU (menu))
			break;
	}

	if (retval && !PANEL_IS_WIDGET (retval)) {
		g_warning ("Invalid PanelWidget associated with menu");
		retval = NULL;
	}

	if (!retval) {
		g_warning ("Cannot find the PanelWidget associated with menu");
		retval = panels->data;
	}

	return retval;
}

static void
setup_menu_panel (GtkWidget *menu)
{
	PanelWidget *panel;

	panel = g_object_get_data (G_OBJECT (menu), "menu_panel");
	if (panel)
		return;

	panel = menu_get_panel (menu);
	g_object_set_data (G_OBJECT (menu), "menu_panel", panel);

	if (panel)
		gtk_menu_set_screen (GTK_MENU (menu),
				     gtk_widget_get_screen (GTK_WIDGET (panel)));
}

GdkScreen *
menuitem_to_screen (GtkWidget *menuitem)
{
	PanelWidget *panel_widget;

	panel_widget = menu_get_panel (menuitem);

	return gtk_window_get_screen (GTK_WINDOW (panel_widget->toplevel));
}

static void
reload_image_menu_items (void)
{
	GSList *l;

	for (l = image_menu_items; l; l = l->next) {
		GtkWidget *image = l->data;
		gboolean   is_mapped;
      
		is_mapped = gtk_widget_get_mapped (image);

		if (is_mapped)
			gtk_widget_unmap (image);

		gtk_image_set_from_pixbuf (GTK_IMAGE (image), NULL);
    
		if (is_mapped)
			gtk_widget_map (image);

	}
}

static void
icon_theme_changed (GtkIconTheme *icon_theme,
		    gpointer      data)
{
	reload_image_menu_items ();
}

GtkWidget *
panel_create_menu (void)
{
	GtkWidget       *retval;
	GtkStyleContext *context;
	static gboolean  registered_icon_theme_changer = FALSE;

	if (!registered_icon_theme_changer) {
		registered_icon_theme_changer = TRUE;

		g_signal_connect (gtk_icon_theme_get_default (), "changed",
				  G_CALLBACK (icon_theme_changed), NULL);
	}
	
	retval = gtk_menu_new ();
	gtk_widget_set_name (retval, "gnome-panel-main-menu");

	context = gtk_widget_get_style_context (retval);
	gtk_style_context_add_class (context, "gnome-panel-main-menu");

	return retval;
}

GtkWidget *
create_empty_menu (void)
{
	GtkWidget *retval;

	retval = panel_create_menu ();

	g_signal_connect (retval, "show", G_CALLBACK (setup_menu_panel), NULL);

	/* intercept all right button clicks makes sure they don't
	   go to the object itself */
	g_signal_connect (retval, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return retval;
}

static void
icon_to_load_free (IconToLoad *icon)
{
	if (!icon)
		return;

	if (icon->pixmap)
		g_object_unref (icon->pixmap);
	icon->pixmap = NULL;

	if (icon->gicon)
		g_object_unref (icon->gicon);
	icon->gicon = NULL;

	g_free (icon->image);          icon->image = NULL;
	g_free (icon->fallback_image); icon->fallback_image = NULL;
	g_free (icon);
}

static IconToLoad *
icon_to_load_copy (IconToLoad *icon)
{
	IconToLoad *retval;

	if (!icon)
		return NULL;

	retval = g_new0 (IconToLoad, 1);

	retval->pixmap         = g_object_ref (icon->pixmap);
	if (icon->gicon)
		retval->gicon  = g_object_ref (icon->gicon);
	else
		retval->gicon  = NULL;
	retval->image          = g_strdup (icon->image);
	retval->fallback_image = g_strdup (icon->fallback_image);
	retval->icon_size      = icon->icon_size;

	return retval;
}

static void
remove_pixmap_from_loaded (gpointer data, GObject *where_the_object_was)
{
	char *key = data;

	if (loaded_icons != NULL)
		g_hash_table_remove (loaded_icons, key);

	g_free (key);
}

GdkPixbuf *
panel_make_menu_icon (GtkIconTheme *icon_theme,
		      const char   *icon,
		      const char   *fallback,
		      int           size,
		      gboolean     *long_operation)
{
	GdkPixbuf *pb;
	char *file, *key;
	gboolean loaded;

	g_return_val_if_fail (size > 0, NULL);

	file = NULL;
	if (icon != NULL)
		file = panel_find_icon (icon_theme, icon, size);
	if (file == NULL && fallback != NULL)
		file = panel_find_icon (icon_theme, fallback, size);

	if (file == NULL)
		return NULL;

	if (long_operation != NULL)
		*long_operation = TRUE;

	pb = NULL;

	loaded = FALSE;

	key = g_strdup_printf ("%d:%s", size, file);

	if (loaded_icons != NULL &&
	    (pb = g_hash_table_lookup (loaded_icons, key)) != NULL) {
		if (pb != NULL)
			g_object_ref (G_OBJECT (pb));
	}

	if (pb == NULL) {
		pb = gdk_pixbuf_new_from_file (file, NULL);
		if (pb) {
			gint width, height;

			width = gdk_pixbuf_get_width (pb);
			height = gdk_pixbuf_get_height (pb);
			
			/* if we want 24 and we get 22, do nothing;
			 * else scale */
			if (!(size - 2 <= width && width <= size &&
                              size - 2 <= height && height <= size)) {
				GdkPixbuf *tmp;

				tmp = gdk_pixbuf_scale_simple (pb, size, size,
							       GDK_INTERP_BILINEAR);

				g_object_unref (pb);
				pb = tmp;
			}
		}
				
		/* add icon to the hash table so we don't load it again */
		loaded = TRUE;
	}

	if (pb == NULL) {
		g_free (file);
		g_free (key);
		return NULL;
	}

	if (loaded &&
	    (gdk_pixbuf_get_width (pb) != size &&
	     gdk_pixbuf_get_height (pb) != size)) {
		GdkPixbuf *pb2;
		int        dest_width;
		int        dest_height;
		int        width;
		int        height;

		width  = gdk_pixbuf_get_width (pb);
		height = gdk_pixbuf_get_height (pb);

		if (height > width) {
			dest_width  = (size * width) / height;
			dest_height = size;
		} else {
			dest_width  = size;
			dest_height = (size * height) / width;
		}

		pb2 = gdk_pixbuf_scale_simple (pb, dest_width, dest_height,
					       GDK_INTERP_BILINEAR);
		g_object_unref (G_OBJECT (pb));
		pb = pb2;
	}

	if (loaded) {
		if (loaded_icons == NULL)
			loaded_icons = g_hash_table_new_full
				(g_str_hash, g_str_equal,
				 (GDestroyNotify) g_free,
				 (GDestroyNotify) g_object_unref);
		g_hash_table_replace (loaded_icons,
				      g_strdup (key),
				      g_object_ref (G_OBJECT (pb)));
		g_object_weak_ref (G_OBJECT (pb),
				   (GWeakNotify) remove_pixmap_from_loaded,
				   g_strdup (key));
	} else {
		/* we didn't load from disk */
		if (long_operation != NULL)
			*long_operation = FALSE;
	}

	g_free (file);
	g_free (key);

	return pb;
}

static void
menu_item_style_updated (GtkImage *image,
                         gpointer  data)
{
	GtkWidget   *widget;
	GdkPixbuf   *pixbuf;
	GtkIconSize  icon_size = (GtkIconSize) GPOINTER_TO_INT (data);
	int          icon_height;
	gboolean     is_mapped;

	if (!gtk_icon_size_lookup (icon_size, NULL, &icon_height))
		return;

	pixbuf = gtk_image_get_pixbuf (image);
	if (!pixbuf)
		return;

	if (gdk_pixbuf_get_height (pixbuf) == icon_height)
		return;

	widget = GTK_WIDGET (image);

	is_mapped = gtk_widget_get_mapped (widget);
	if (is_mapped)
		gtk_widget_unmap (widget);

	gtk_image_set_from_pixbuf (image, NULL);
    
	if (is_mapped)
		gtk_widget_map (widget);
}

static void
do_icons_to_add (void)
{
	while (icons_to_add) {
		IconToAdd *icon_to_add = icons_to_add->data;
		int icon_height;

		icons_to_add = g_list_delete_link (icons_to_add, icons_to_add);

		if (icon_to_add->gicon) {
			gtk_image_set_from_gicon (
				GTK_IMAGE (icon_to_add->image),
				icon_to_add->gicon,
				icon_to_add->icon_size);
		} else {
			g_assert (icon_to_add->pixbuf);

			gtk_image_set_from_pixbuf (
				GTK_IMAGE (icon_to_add->image),
				icon_to_add->pixbuf);

			g_signal_connect (icon_to_add->image, "style-updated",
					  G_CALLBACK (menu_item_style_updated),
					  GINT_TO_POINTER (icon_to_add->icon_size));

			g_object_unref (icon_to_add->pixbuf);
		}

		if (gtk_icon_size_lookup (icon_to_add->icon_size, NULL, &icon_height))
			gtk_image_set_pixel_size (GTK_IMAGE (icon_to_add->image), icon_height);

		if (icon_to_add->gicon)
			g_object_unref (icon_to_add->gicon);
		g_object_unref (icon_to_add->image);
		g_free (icon_to_add);
	}
}

static gboolean
load_icons_handler (gpointer data)
{
	IconToLoad *icon;
	gboolean    long_operation = FALSE;

load_icons_handler_again:

	if (!icons_to_load) {
		load_icons_id = 0;
		do_icons_to_add ();

		return FALSE;
	}

	icon = icons_to_load->data;
	icons_to_load->data = NULL;
	/* pop */
	icons_to_load = g_list_delete_link (icons_to_load, icons_to_load);

	/* if not visible anymore, just ignore */
	if ( ! gtk_widget_get_visible (icon->pixmap)) {
		icon_to_load_free (icon);
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;
	}

	if (icon->gicon) {
		IconToAdd *icon_to_add;

		icon_to_add            = g_new (IconToAdd, 1);
		icon_to_add->image     = g_object_ref (icon->pixmap);
		icon_to_add->pixbuf    = NULL;
		icon_to_add->icon_size = icon->icon_size;
		if (icon->gicon)
			icon_to_add->gicon = g_object_ref (icon->gicon);
		else
			icon_to_add->gicon = NULL;

		icons_to_add = g_list_prepend (icons_to_add, icon_to_add);
	} else {
		IconToAdd *icon_to_add;
		GdkPixbuf *pb;
		int        icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

		gtk_icon_size_lookup (icon->icon_size, NULL, &icon_height);

		pb = panel_make_menu_icon (icon->icon_theme,
					   icon->image,
					   icon->fallback_image,
					   icon_height,
					   &long_operation);
		if (!pb) {
			icon_to_load_free (icon);
			if (long_operation)
				/* this may have been a long operation so jump back to
				 * the main loop for a while */
				return TRUE;
			else
				/* we didn't do anything long/hard, so just do this again,
				 * this is fun, don't go back to main loop */
				goto load_icons_handler_again;
		}

		icon_to_add            = g_new (IconToAdd, 1);
		icon_to_add->image     = g_object_ref (icon->pixmap);
		icon_to_add->gicon     = NULL;
		icon_to_add->pixbuf    = pb;
		icon_to_add->icon_size = icon->icon_size;

		icons_to_add = g_list_prepend (icons_to_add, icon_to_add);
	}

	icon_to_load_free (icon);

	if (!long_operation)
		/* we didn't do anything long/hard, so just do this again,
		 * this is fun, don't go back to main loop */
		goto load_icons_handler_again;

	/* if still more we'll come back */
	return TRUE;
}

gboolean
menu_dummy_button_press_event (GtkWidget      *menuitem,
			       GdkEventButton *event)
{
	if (event->button == 3)
		return TRUE;

	return FALSE;
}

static void  
drag_begin_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
	/* FIXME: workaround for a possible gtk+ bug
	 *    See bugs #92085(gtk+) and #91184(panel) for details.
	 *    Maybe it's not needed with GtkTooltip?
	 */
	g_object_set (widget, "has-tooltip", FALSE, NULL);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * GTK+ menuing code in some manner.
 */
static void  
drag_end_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
  GtkWidget *xgrab_shell;
  GtkWidget *parent;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = gtk_widget_get_parent (widget);
  xgrab_shell = NULL;

  /* FIXME: workaround for a possible gtk+ bug
   *    See bugs #92085(gtk+) and #91184(panel) for details.
   */
  g_object_set (widget, "has-tooltip", TRUE, NULL);

  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!gtk_widget_get_mapped (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = gtk_widget_get_parent (tmp);
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = gtk_menu_shell_get_parent_shell (GTK_MENU_SHELL (parent));
    }

  if (xgrab_shell)
    {
      gboolean      status;
      GdkDisplay    *display;
      GdkDevice     *pointer;
      GdkDevice     *keyboard;
      GdkDeviceManager *device_manager;
      GdkWindow *window = gtk_widget_get_window (xgrab_shell);
      GdkCursor *cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                                      GDK_ARROW);

      display = gdk_window_get_display (window);
      device_manager = gdk_display_get_device_manager (display);
      pointer = gdk_device_manager_get_client_pointer (device_manager);
      keyboard = gdk_device_get_associated_device (pointer);

      /* FIXMEgpoo: Not sure if report to GDK_OWNERSHIP_WINDOW
	            or GDK_OWNERSHIP_APPLICATION. Idem for the
                    keyboard below */
      status = gdk_device_grab (pointer, window,
                                GDK_OWNERSHIP_WINDOW, TRUE,
                                GDK_BUTTON_PRESS_MASK
                                | GDK_BUTTON_RELEASE_MASK
                                | GDK_ENTER_NOTIFY_MASK
                                | GDK_LEAVE_NOTIFY_MASK
                                | GDK_POINTER_MOTION_MASK,
                                cursor, GDK_CURRENT_TIME);

      if (!status)
	{
	  if (gdk_device_grab (keyboard, window,
			       GDK_OWNERSHIP_WINDOW, TRUE,
			       GDK_KEY_PRESS | GDK_KEY_RELEASE,
			       NULL, GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS)
	    {
	    /* FIXMEgpoo: We need either accessors or a workaround to grab
	       the focus */
#if 0
	     GTK_MENU_SHELL (xgrab_shell)->GSEAL(have_xgrab) = TRUE;
#endif
	    }
	  else
	    {
	      gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
	    }
	}

      g_object_unref (cursor);
    }
}

static void  
drag_data_get_menu_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             time,
		       GMenuTreeEntry   *entry)
{
	const char *path;
	char       *uri;
	char       *uri_list;

	path = gmenu_tree_entry_get_desktop_file_path (entry);
	uri = g_filename_to_uri (path, NULL, NULL);
	uri_list = g_strconcat (uri, "\r\n", NULL);
	g_free (uri);

	gtk_selection_data_set (selection_data,
				gtk_selection_data_get_target (selection_data), 8, (guchar *)uri_list,
				strlen (uri_list));
	g_free (uri_list);
}

static void
image_menuitem_set_size_request (GtkWidget  *menuitem,
                                 GtkIconSize icon_size)
{
        GtkStyleContext *context;
        GtkStateFlags state;
        GtkBorder padding, border;
        int border_width;
	int icon_height;
	int req_height;

	if (!gtk_icon_size_lookup (icon_size, NULL, &icon_height))
		return;

	/* If we don't have a pixmap for this menuitem
	 * at least make sure its the same height as
	 * the rest.
	 * This is a bit ugly, since we should keep this in sync with what's in
	 * gtk_menu_item_size_request()
	 */
        context = gtk_widget_get_style_context (menuitem);
        state = gtk_widget_get_state_flags (menuitem);
        gtk_style_context_get_padding (context, state, &padding);
        gtk_style_context_get_border (context, state, &border);

        border_width = gtk_container_get_border_width (GTK_CONTAINER (menuitem));
	req_height = icon_height;
	req_height += (border_width * 2) + padding.top + padding.bottom + border.top + border.bottom;
        gtk_widget_set_size_request (menuitem, -1, req_height);
}

static char *
menu_escape_underscores_and_prepend (const char *text)
{
	GString    *escaped_text;
	const char *src;
	int         inserted;
	
	if (!text)
		return g_strdup (text);

	escaped_text = g_string_sized_new (strlen (text) + 1);
	g_string_printf (escaped_text, "_%s", text);

	src = text;
	inserted = 1;

	while (*src) {
		gunichar c;

		c = g_utf8_get_char (src);

		if (c == (gunichar)-1) {
			g_warning ("Invalid input string for underscore escaping");
			return g_strdup (text);
		} else if (c == '_') {
			g_string_insert_c (escaped_text,
					   src - text + inserted, '_');
			inserted++;
		}

		src = g_utf8_next_char (src);
	}

	return g_string_free (escaped_text, FALSE);
}

void
setup_menuitem (GtkWidget   *menuitem,
		GtkIconSize  icon_size,
		GtkWidget   *image,
		const char  *title)
			       
{
	GtkWidget *label;
	char      *_title;

	/* this creates a label with an invisible mnemonic */
	label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
	_title = menu_escape_underscores_and_prepend (title);
	gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _title);
	g_free (_title);

	gtk_label_set_pattern (GTK_LABEL (label), "");

	gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (label), menuitem);

	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_widget_show (label);
       
	gtk_container_add (GTK_CONTAINER (menuitem), label);

	if (image) {
		g_object_set_data_full (G_OBJECT (menuitem),
					"Panel:Image",
					g_object_ref (image),
					(GDestroyNotify) g_object_unref);
		gtk_widget_show (image);
		panel_image_menu_item_set_image (PANEL_IMAGE_MENU_ITEM (menuitem), image);
	} else if (icon_size != GTK_ICON_SIZE_INVALID)
                image_menuitem_set_size_request (menuitem, icon_size);

	gtk_widget_show (menuitem);
}

static void
drag_data_get_string_cb (GtkWidget *widget, GdkDragContext     *context,
			 GtkSelectionData   *selection_data, guint info,
			 guint time, const char *string)
{
	gtk_selection_data_set (selection_data,
				gtk_selection_data_get_target (selection_data), 8, (guchar *)string,
				strlen(string));
}

void
setup_uri_drag (GtkWidget  *menuitem,
		const char *uri,
		const char *icon)
{
	static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	if (panel_lockdown_get_panels_locked_down_s ())
		return;

	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);

	if (icon != NULL)
		gtk_drag_source_set_icon_name (menuitem, icon);
	
	g_signal_connect (G_OBJECT (menuitem), "drag_begin",
			  G_CALLBACK (drag_begin_menu_cb), NULL);
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (uri),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);
}

void
setup_internal_applet_drag (GtkWidget             *menuitem,
			    PanelActionButtonType  type)
{
	static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};

	if (panel_lockdown_get_panels_locked_down_s ())
		return;

	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);

	if (panel_action_get_icon_name (type)  != NULL)
		gtk_drag_source_set_icon_name (menuitem,
					       panel_action_get_icon_name (type));
	
	g_signal_connect (G_OBJECT (menuitem), "drag_begin",
			  G_CALLBACK (drag_begin_menu_cb), NULL);
	g_signal_connect_data (G_OBJECT (menuitem), "drag_data_get",
			       G_CALLBACK (drag_data_get_string_cb),
			       g_strdup (panel_action_get_drag_id (type)),
			       (GClosureNotify)g_free,
			       0 /* connect_flags */);
	g_signal_connect (G_OBJECT (menuitem), "drag_end",
			  G_CALLBACK (drag_end_menu_cb), NULL);
}

static void
submenu_to_display (GtkWidget *menu)
{
	GMenuTree           *tree;
	GMenuTreeDirectory  *directory;
	const char          *menu_path;
	void               (*append_callback) (GtkWidget *, gpointer);
	gpointer             append_data;

	if (!g_object_get_data (G_OBJECT (menu), "panel-menu-needs-loading"))
		return;

	g_object_set_data (G_OBJECT (menu), "panel-menu-needs-loading", NULL);

	directory = g_object_get_data (G_OBJECT (menu),
				       "panel-menu-tree-directory");
	if (!directory) {
		menu_path = g_object_get_data (G_OBJECT (menu),
					       "panel-menu-tree-path");
		if (!menu_path)
			return;

		tree = g_object_get_data (G_OBJECT (menu), "panel-menu-tree");
		if (!tree)
			return;

		directory = gmenu_tree_get_directory_from_path (tree,
								menu_path);

		g_object_set_data_full (G_OBJECT (menu),
					"panel-menu-tree-directory",
					directory,
					(GDestroyNotify) gmenu_tree_item_unref);
	}

	if (directory)
		populate_menu_from_directory (menu, directory);

	append_callback = g_object_get_data (G_OBJECT (menu),
					     "panel-menu-append-callback");
	append_data     = g_object_get_data (G_OBJECT (menu),
					     "panel-menu-append-callback-data");
	if (append_callback)
		append_callback (menu, append_data);
}

static gboolean
submenu_to_display_in_idle (gpointer data)
{
	GtkWidget *menu = GTK_WIDGET (data);

	g_object_set_data (G_OBJECT (menu), "panel-menu-idle-id", NULL);

	submenu_to_display (menu);

	return FALSE;
}

static void
remove_submenu_to_display_idle (gpointer data)
{
	guint idle_id = GPOINTER_TO_UINT (data);

	g_source_remove (idle_id);
}

static GtkWidget *
create_fake_menu (GMenuTreeDirectory *directory)
{	
	GtkWidget *menu;
	guint      idle_id;
	
	menu = create_empty_menu ();

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				gmenu_tree_item_ref (directory),
				(GDestroyNotify) gmenu_tree_item_unref);
	
	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);

	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	return menu;
}

GtkWidget *
panel_image_menu_item_new2 (void)
{
	GtkWidget *menuitem;
	GtkStyleContext *context;

	menuitem = panel_image_menu_item_new ();
	panel_image_menu_item_set_always_show_image (PANEL_IMAGE_MENU_ITEM (menuitem), TRUE);
	context = gtk_widget_get_style_context (menuitem);
	gtk_style_context_add_class (context, "gnome-panel-menu-item");

	return menuitem;
}

static GtkWidget *
create_submenu_entry (GtkWidget          *menu,
		      GMenuTreeDirectory *directory)
{
	GtkWidget *menuitem;
	gboolean   force_categories_icon;

	force_categories_icon = g_object_get_data (G_OBJECT (menu),
						   "panel-menu-force-icon-for-categories") != NULL;

	if (force_categories_icon)
		menuitem = panel_image_menu_item_new2 ();
	else
		menuitem = panel_image_menu_item_new ();

	panel_load_menu_image_deferred (menuitem,
					panel_menu_icon_get_size (),
					gmenu_tree_directory_get_icon (directory),
					NULL,
					PANEL_ICON_FOLDER);

	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			gmenu_tree_directory_get_name (directory));

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	gtk_widget_show (menuitem);

	return menuitem;
}

static void
create_submenu (GtkWidget          *menu,
		GMenuTreeDirectory *directory,
		GMenuTreeDirectory *alias_directory)
{
	GtkWidget *menuitem;
	GtkWidget *submenu;
	gboolean   force_categories_icon;

	if (alias_directory)
		menuitem = create_submenu_entry (menu, alias_directory);
	else
		menuitem = create_submenu_entry (menu, directory);
	
	submenu = create_fake_menu (directory);

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	/* Keep the infor that we force (or not) the icons to be visible */
	force_categories_icon = g_object_get_data (G_OBJECT (menu),
						   "panel-menu-force-icon-for-categories") != NULL;
	g_object_set_data (G_OBJECT (submenu),
			   "panel-menu-force-icon-for-categories",
			   GINT_TO_POINTER (force_categories_icon));
}

static void 
create_header (GtkWidget       *menu,
	       GMenuTreeHeader *header)
{
	GMenuTreeDirectory *directory;
	GtkWidget          *menuitem;

	directory = gmenu_tree_header_get_directory (header);
	menuitem = create_submenu_entry (menu, directory);
	gmenu_tree_item_unref (directory);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (gtk_false), NULL);
}

static void
create_menuitem (GtkWidget          *menu,
		 GMenuTreeEntry     *entry,
		 GMenuTreeDirectory *alias_directory)
{
	GtkWidget  *menuitem;
	
	menuitem = panel_image_menu_item_new2 ();

	if (alias_directory)
		panel_load_menu_image_deferred (menuitem,
						panel_menu_icon_get_size (),
						gmenu_tree_directory_get_icon (alias_directory),
						NULL,
						NULL);
	else
		panel_load_menu_image_deferred (menuitem,
						panel_menu_icon_get_size (),
						g_app_info_get_icon (G_APP_INFO (gmenu_tree_entry_get_app_info (entry))),
						NULL, NULL);

	setup_menuitem (menuitem,
			panel_menu_icon_get_size (),
			NULL,
			alias_directory ? gmenu_tree_directory_get_name (alias_directory) :
			                  g_app_info_get_display_name (G_APP_INFO (gmenu_tree_entry_get_app_info (entry))));

	if (alias_directory &&
	    gmenu_tree_directory_get_comment (alias_directory))
		panel_util_set_tooltip_text (menuitem,
					     gmenu_tree_directory_get_comment (alias_directory));
	else if	(!alias_directory) {
		const char *description = g_app_info_get_description (G_APP_INFO (gmenu_tree_entry_get_app_info (entry)));
		if (!description)
			description = g_desktop_app_info_get_generic_name (gmenu_tree_entry_get_app_info (entry));
		if (description)
			panel_util_set_tooltip_text (menuitem,
						     description);
	}

	g_signal_connect_after (menuitem, "button_press_event",
				G_CALLBACK (menu_dummy_button_press_event), NULL);

	if (!panel_lockdown_get_panels_locked_down_s ()) {
		GIcon *icon;

		static GtkTargetEntry menu_item_targets[] = {
			{ "text/uri-list", 0, 0 }
		};

		gtk_drag_source_set (menuitem,
				     GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
				     menu_item_targets, 1,
				     GDK_ACTION_COPY);

		icon = g_app_info_get_icon (G_APP_INFO (gmenu_tree_entry_get_app_info (entry)));
		if (icon != NULL)
			gtk_drag_source_set_icon_gicon (menuitem, icon);

		g_signal_connect (G_OBJECT (menuitem), "drag_begin",
				  G_CALLBACK (drag_begin_menu_cb), NULL);
		g_signal_connect (menuitem, "drag_data_get",
				  G_CALLBACK (drag_data_get_menu_cb), entry);
		g_signal_connect (menuitem, "drag_end",
				  G_CALLBACK (drag_end_menu_cb), NULL);
	}

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (activate_app_def), entry);

	gtk_widget_show (menuitem);
}

static void
create_menuitem_from_alias (GtkWidget      *menu,
			    GMenuTreeAlias *alias)
{
	GMenuTreeDirectory *src = gmenu_tree_alias_get_directory (alias);

	switch (gmenu_tree_alias_get_aliased_item_type (alias)) {
	case GMENU_TREE_ITEM_DIRECTORY: {
		GMenuTreeDirectory *directory = gmenu_tree_alias_get_aliased_directory (alias);
		create_submenu (menu,
				directory,
				src);
		gmenu_tree_item_unref (directory);
		break;
	}

	case GMENU_TREE_ITEM_ENTRY: {
		GMenuTreeEntry *entry = gmenu_tree_alias_get_aliased_entry (alias);
		create_menuitem (menu,
				 entry,
				 src);
		gmenu_tree_item_unref (entry);
		break;
	}

	default:
		break;
	}

	gmenu_tree_item_unref (src);
}

static void
handle_gmenu_tree_changed (GMenuTree *tree,
			   GtkWidget *menu)
{
	guint idle_id;
	GList *list, *l;
	GError *error = NULL;

	/* Remove existing items */
	list = gtk_container_get_children (GTK_CONTAINER (menu));
	for (l = list; l; l = l->next)
		gtk_widget_destroy (l->data);
	g_list_free (list);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-directory",
				NULL, NULL);

	if (!gmenu_tree_load_sync (tree, &error)) {
		g_warning ("Failed to load applications: %s", error->message);
		g_clear_error (&error);
	}

	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);
}

static void
remove_gmenu_tree_monitor (GtkWidget *menu,
			  GMenuTree  *tree)
{
	g_signal_handlers_disconnect_by_func (tree,
					      G_CALLBACK (handle_gmenu_tree_changed),
					      menu);
}

GtkWidget *
create_applications_menu (const char *menu_file,
			  const char *menu_path,
			  gboolean    always_show_image)
{
	GMenuTree *tree;
	GtkWidget *menu;
	guint      idle_id;
	GError *error = NULL;

	menu = create_empty_menu ();

	if (always_show_image)
		g_object_set_data (G_OBJECT (menu),
				   "panel-menu-force-icon-for-categories",
				   GINT_TO_POINTER (TRUE));

	tree = gmenu_tree_new (menu_file, GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);

	if (!gmenu_tree_load_sync (tree, &error)) {
		g_warning ("Failed to load applications: %s", error->message);
		g_clear_error (&error);
		return menu;
	}

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree",
				g_object_ref (tree),
				(GDestroyNotify) g_object_unref);

	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-tree-path",
				g_strdup (menu_path ? menu_path : "/"),
				(GDestroyNotify) g_free);
	
	g_object_set_data (G_OBJECT (menu),
			   "panel-menu-needs-loading",
			   GUINT_TO_POINTER (TRUE));

	g_signal_connect (menu, "show",
			  G_CALLBACK (submenu_to_display), NULL);

	idle_id = g_idle_add_full (G_PRIORITY_LOW,
				   submenu_to_display_in_idle,
				   menu,
				   NULL);
	g_object_set_data_full (G_OBJECT (menu),
				"panel-menu-idle-id",
				GUINT_TO_POINTER (idle_id),
				remove_submenu_to_display_idle);

	g_signal_connect (menu, "button_press_event",
			  G_CALLBACK (menu_dummy_button_press_event), NULL);

	g_signal_connect (tree, "changed", G_CALLBACK (handle_gmenu_tree_changed), menu);
	g_signal_connect (menu, "destroy", G_CALLBACK (remove_gmenu_tree_monitor), tree);

	g_object_unref (tree);

	return menu;
}

static GtkWidget *
populate_menu_from_directory (GtkWidget          *menu,
			      GMenuTreeDirectory *directory)
{	
	GList    *children;
	gboolean  add_separator;
	GMenuTreeIter *iter;
	GMenuTreeItemType next_type;

	children = gtk_container_get_children (GTK_CONTAINER (menu));
	add_separator = (children != NULL);
	g_list_free (children);

	iter = gmenu_tree_directory_iter (directory);

	while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID) {
		gpointer item = NULL;

		if (add_separator ||
		    next_type == GMENU_TREE_ITEM_SEPARATOR) {
			add_menu_separator (menu);
			add_separator = FALSE;
		}

		switch (next_type) {
		case GMENU_TREE_ITEM_DIRECTORY:
			item = gmenu_tree_iter_get_directory (iter);
			create_submenu (menu, item, NULL);
			break;

		case GMENU_TREE_ITEM_ENTRY:
			item = gmenu_tree_iter_get_entry (iter);
			create_menuitem (menu, item, NULL);
			break;

		case GMENU_TREE_ITEM_SEPARATOR :
			/* already added */
			break;

		case GMENU_TREE_ITEM_ALIAS:
			item = gmenu_tree_iter_get_alias (iter);
			create_menuitem_from_alias (menu, item);
			break;

		case GMENU_TREE_ITEM_HEADER:
			item = gmenu_tree_iter_get_header (iter);
			create_header (menu, item);
			break;

		default:
			break;
		}

		if (item)
			gmenu_tree_item_unref (item);
	}

	gmenu_tree_iter_unref (iter);

	return menu;
}

void
setup_menu_item_with_icon (GtkWidget   *item,
			   GtkIconSize  icon_size,
			   const char  *icon_name,
			   GIcon       *gicon,
			   const char  *title)
{
	if (icon_name || gicon)
		panel_load_menu_image_deferred (item, icon_size,
						gicon, icon_name, NULL);

	setup_menuitem (item, icon_size, NULL, title);
}

static void
main_menu_append (GtkWidget *main_menu,
		  gpointer   data)
{
	PanelWidget *panel;
	GtkWidget   *item;
	gboolean     add_separator;
	GList       *children;
	GList       *last;

	panel = PANEL_WIDGET (data);

	add_separator = FALSE;
	children = gtk_container_get_children (GTK_CONTAINER (main_menu));
	last = g_list_last (children);
	if (last != NULL) {
		add_separator = !GTK_IS_SEPARATOR (GTK_WIDGET (last->data));
	}
	g_list_free (children);

	if (add_separator)
		add_menu_separator (main_menu);

	item = panel_place_menu_item_new (TRUE, FALSE);
	panel_place_menu_item_set_panel (item, panel);
	gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), item);
	gtk_widget_show (item);

	item = panel_desktop_menu_item_new (TRUE, FALSE, FALSE);
	panel_desktop_menu_item_set_panel (item, panel);
	gtk_menu_shell_append (GTK_MENU_SHELL (main_menu), item);
	gtk_widget_show (item);

	panel_menu_items_append_lock_logout (main_menu);
}

GtkWidget *
create_main_menu (PanelWidget *panel)
{
	GtkWidget *main_menu;
	gchar *applications_menu;

	applications_menu = get_applications_menu ();
	main_menu = create_applications_menu (applications_menu, NULL, TRUE);
	g_free (applications_menu);

	g_object_set_data (G_OBJECT (main_menu), "menu_panel", panel);
	/* FIXME need to update the panel on parent_set */

	g_object_set_data (G_OBJECT (main_menu),
			   "panel-menu-append-callback",
			   main_menu_append);
	g_object_set_data (G_OBJECT (main_menu),
			   "panel-menu-append-callback-data",
			   panel);

	return main_menu;
}

static GList *
find_in_load_list (GtkWidget *image)
{
	GList *li;
	for (li = icons_to_load; li != NULL; li = li->next) {
		IconToLoad *icon = li->data;
		if (icon->pixmap == image)
			return li;
	}
	return NULL;
}

static void
image_menu_shown (GtkWidget *image, gpointer data)
{
	IconToLoad *new_icon;
	IconToLoad *icon;
	
	icon = (IconToLoad *) data;

	/* if we've already handled this */
	if (gtk_image_get_storage_type (GTK_IMAGE (image)) != GTK_IMAGE_EMPTY)
		return;

	if (find_in_load_list (image) == NULL) {
		new_icon = icon_to_load_copy (icon);
		new_icon->icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (image));
		icons_to_load = g_list_append (icons_to_load, new_icon);
	}
	if (load_icons_id == 0)
		load_icons_id = g_idle_add (load_icons_handler, NULL);
}

static void
image_menu_destroy (GtkWidget *image, gpointer data)
{
	image_menu_items = g_slist_remove (image_menu_items, image);
}

static void
panel_load_menu_image_deferred (GtkWidget   *image_menu_item,
				GtkIconSize  icon_size,
				GIcon       *gicon,
				const char  *image_filename,
				const char  *fallback_image_filename)
{
	IconToLoad *icon;
	GtkWidget *image;
	int        icon_height = PANEL_DEFAULT_MENU_ICON_SIZE;

	icon = g_new (IconToLoad, 1);

	gtk_icon_size_lookup (icon_size, NULL, &icon_height);

	image = gtk_image_new ();
	gtk_widget_set_size_request (image, icon_height, icon_height);

	/* this takes over the floating ref */
	icon->pixmap = g_object_ref_sink (G_OBJECT (image));

	if (gicon)
		icon->gicon  = g_object_ref (gicon);
	else
		icon->gicon  = NULL;
	icon->image          = g_strdup (image_filename);
	icon->fallback_image = g_strdup (fallback_image_filename);
	icon->icon_size      = icon_size;

	gtk_widget_show (image);

	g_object_set_data_full (G_OBJECT (image_menu_item),
				"Panel:Image",
				g_object_ref (image),
				(GDestroyNotify) g_object_unref);

	panel_image_menu_item_set_image (PANEL_IMAGE_MENU_ITEM (image_menu_item), image);

	g_signal_connect_data (image, "map",
			       G_CALLBACK (image_menu_shown), icon,
			       (GClosureNotify) icon_to_load_free, 0);

	g_signal_connect (image, "destroy",
			  G_CALLBACK (image_menu_destroy), NULL);

	image_menu_items = g_slist_prepend (image_menu_items, image);
}

gchar *
get_applications_menu (void)
{
	const gchar *xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
	return g_strdup_printf ("%sapplications.menu",
	                        !PANEL_GLIB_STR_EMPTY (xdg_menu_prefx) ? xdg_menu_prefx : "gnome-");
}
