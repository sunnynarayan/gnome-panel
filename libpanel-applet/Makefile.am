lib_LTLIBRARIES = libpanel-applet.la
noinst_PROGRAMS = test-dbus-applet

AM_CPPFLAGS =							\
	$(LIBPANEL_APPLET_CFLAGS)				\
	-I$(top_builddir)/libpanel-applet			\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

libpanel_appletincludedir = $(includedir)/gnome-panel/libpanel-applet
libpanel_appletinclude_HEADERS =	\
	panel-applet.h			\
	panel-applet-enums.h

libpanel_applet_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	panel-applet.h			\
	panel-applet.c			\
	panel-applet-factory.h		\
	panel-applet-factory.c		\
	panel-applet-bindings.h		\
	panel-applet-bindings.c		\
	panel-applet-private.h

libpanel_applet_la_LIBADD  = \
	$(LIBPANEL_APPLET_LIBS) \
	$(X_LIBS)

libpanel_applet_la_LDFLAGS = \
	-version-info $(LIB_PANEL_APPLET_LT_VERSION)	\
	-no-undefined

test_dbus_applet_LDADD =	\
	$(LIBPANEL_APPLET_LIBS)	\
	libpanel-applet.la

$(libpanel_applet_la_OBJECTS) $(test_dbus_applet_OBJECTS): $(BUILT_SOURCES)

panel-applet-marshal.h: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=panel_applet_marshal > $@

panel-applet-marshal.c: panel-applet-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)echo "#include \"panel-applet-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=panel_applet_marshal >> $@

panel_applet_enum_headers = $(top_srcdir)/libpanel-applet/panel-applet.h

panel-applet-enums.c: $(panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"panel-applet-enums.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--fprod "\n#include \"@filename@\"\n" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(panel_applet_enum_headers) > $@

panel-applet-enums.h: $(panel_applet_enum_headers)
	$(AM_V_GEN)glib-mkenums \
			--fhead "#ifndef __PANEL_APPLET_ENUMS_H__\n" \
			--fhead "#define __PANEL_APPLET_ENUMS_H__\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __PANEL_APPLET_ENUMS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define PANEL_TYPE_@ENUMNAME@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(panel_applet_enum_headers) > $@

BUILT_SOURCES = \
	panel-applet-enums.c			\
	panel-applet-enums.h			\
	panel-applet-marshal.c			\
	panel-applet-marshal.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpanel-applet.pc

appletdir       = $(appletsdir)
applet_in_files = org.gnome.panel.TestApplet.panel-applet.in
noinst_DATA     = $(applet_in_files:.panel-applet.in=.panel-applet)

@PANEL_INTLTOOL_PANEL_APPLET_RULE@

EXTRA_DIST =						\
	org.gnome.panel.TestApplet.panel-applet.in	\
	panel-applet-marshal.list			\
	libpanel-applet.pc.in			\
	libpanel-applet-uninstalled.pc.in

CLEANFILES = $(BUILT_SOURCES) $(noinst_DATA)

###############################
# Introspection generation

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = 	\
	panel-applet.c		\
	panel-applet.h
built_introspection_sources =	\
	panel-applet-enums.c	\
	panel-applet-enums.h

PanelApplet-$(LIBPANEL_APPLET_API_VERSION).gir: libpanel-applet.la
PanelApplet_@LIBPANEL_APPLET_GIR_VERSION@_gir_INCLUDES = GObject-2.0 Gtk-3.0
PanelApplet_@LIBPANEL_APPLET_GIR_VERSION@_gir_CFLAGS = $(AM_CPPFLAGS)
PanelApplet_@LIBPANEL_APPLET_GIR_VERSION@_gir_LIBS = libpanel-applet.la
PanelApplet_@LIBPANEL_APPLET_GIR_VERSION@_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources)) $(addprefix $(builddir)/,$(built_introspection_sources))
PanelApplet_@LIBPANEL_APPLET_GIR_VERSION@_gir_SCANNERFLAGS = --identifier-prefix=Panel --symbol-prefix=panel --pkg-export=libpanel-applet
INTROSPECTION_GIRS += PanelApplet-$(LIBPANEL_APPLET_API_VERSION).gir

girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

################################

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
