AC_INIT([gnome-panel], [3.15.2],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-panel])
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar -Wno-portability])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LIBPANEL_APPLET_API_VERSION=5.0
AC_SUBST([LIBPANEL_APPLET_API_VERSION])

LIBPANEL_APPLET_GIR_VERSION=5_0
AC_SUBST([LIBPANEL_APPLET_GIR_VERSION])

LIB_PANEL_APPLET_LT_VERSION=0:1:0
AC_SUBST(LIB_PANEL_APPLET_LT_VERSION)

GNOME_MAINTAINER_MODE_DEFINES

AC_ARG_ENABLE([documentation],
	AS_HELP_STRING([--enable-documentation], [enable man pages and HTML]),
	[], [enable_documentation=yes])
AM_CONDITIONAL(ENABLE_DOCUMENTATION, test x$enable_documentation = xyes)

LT_PREREQ([2.2.6])
LT_INIT([dlopen disable-static])

IT_PROG_INTLTOOL([0.40.6])

AC_PROG_CC

AM_PATH_PYTHON
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

GNOME_COMPILE_WARNINGS([maximum])

# Check for introspection
GOBJECT_INTROSPECTION_CHECK([0.9.5])

AC_ARG_WITH([girdir],
	AS_HELP_STRING([--with-girdir=DIR], [Installation path for .gir files @<:@auto@:>@]),
	[ac_with_girdir=$withval], [ac_with_girdir=""])
if test "$ac_with_girdir" != ""; then
	INTROSPECTION_GIRDIR=${ac_with_girdir}
fi

AC_ARG_WITH([typelibdir],
	AS_HELP_STRING([--with-typelibdir=DIR],[Installation path for .typelib files @<:@auto@:>@]),
	[ac_with_typelibdir=$withval], [ac_with_typelibdir=""])
if test "$ac_with_typelibdir" != ""; then
	INTROSPECTION_TYPELIBDIR=${ac_with_typelibdir}
fi

AC_MSG_NOTICE([installing .gir files in $INTROSPECTION_GIRDIR])
AC_MSG_NOTICE([installing .typelib files in $INTROSPECTION_TYPELIBDIR])

AC_SUBST(INTROSPECTION_GIRDIR)
AC_SUBST(INTROSPECTION_TYPELIBDIR)

GLIB_GSETTINGS

m4_pattern_allow([AM_V_GEN])dnl Make autoconf not complain about the rule below
PANEL_INTLTOOL_PANEL_APPLET_RULE='%.panel-applet: %.panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; $(AM_V_GEN) LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([PANEL_INTLTOOL_PANEL_APPLET_RULE])

AC_ARG_ENABLE(deprecation_flags,
              [AC_HELP_STRING([--enable-deprecation-flags],
                              [use *_DISABLE_DEPRECATED flags @<:@default=no@:>@])],,
              [enable_deprecation_flags=no])

if test "x$enable_deprecation_flags" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST(DISABLE_DEPRECATED_CFLAGS)
fi

AC_CHECK_HEADERS(crt_externs.h)
AC_CHECK_FUNCS(_NSGetEnviron)

LIBGNOME_DESKTOP_REQUIRED=2.91.0
GDK_PIXBUF_REQUIRED=2.25.2
PANGO_REQUIRED=1.15.4
GLIB_REQUIRED=2.35.0
GTK_REQUIRED=3.15.2
LIBWNCK_REQUIRED=3.4.6
LIBGNOME_MENU_REQUIRED=3.7.90
TELEPATHY_GLIB_REQUIRED=0.14.0
LIBECAL_REQUIRED=3.5.3
LIBEDATASERVER_REQUIRED=3.5.3
CAIRO_REQUIRED=1.0.0
GWEATHER_REQUIRED=3.10.1
DCONF_REQUIRED=0.13.4
LIBRSVG_REQUIRED=2.36.2

dnl pkg-config dependency checks

PKG_CHECK_MODULES(PANEL, gmodule-2.0 >= $GLIB_REQUIRED
			 gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQUIRED
			 pango >= $PANGO_REQUIRED
			 gtk+-3.0 >= $GTK_REQUIRED
			 cairo-xlib glib-2.0 >= $GLIB_REQUIRED
			 gmodule-2.0 >= $GLIB_REQUIRED
			 gio-2.0 >= $GLIB_REQUIRED
			 gio-unix-2.0 >= $GLIB_REQUIRED
			 gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED
			 libgnome-menu-3.0 >= $LIBGNOME_MENU_REQUIRED
			 dconf >= $DCONF_REQUIRED)
AC_SUBST(PANEL_CFLAGS)
AC_SUBST(PANEL_LIBS)

AC_ARG_ENABLE(telepathy_glib, AS_HELP_STRING([--enable-telepathy-glib],[Enable telepathy-glib support (auto)]),enable_telepathy_glib=$enableval,enable_telepathy_glib=auto)
if test "x$enable_telepathy_glib" = "xno" ; then
  HAVE_TELEPATHY_GLIB=no
else
  PKG_CHECK_MODULES(TELEPATHY_GLIB, telepathy-glib >= $TELEPATHY_GLIB_REQUIRED, HAVE_TELEPATHY_GLIB=yes, HAVE_TELEPATHY_GLIB=no)
fi
if test "x$HAVE_TELEPATHY_GLIB" = "xyes" ; then
    AC_DEFINE(HAVE_TELEPATHY_GLIB, 1, [Defined if telepathy-glib support is enabled])
fi

PKG_CHECK_MODULES(LIBPANEL_APPLET, gtk+-3.0 >= $GTK_REQUIRED cairo-xlib cairo-gobject gio-2.0 >= $GLIB_REQUIRED gio-unix-2.0 >= $GLIB_REQUIRED)
AC_SUBST(LIBPANEL_APPLET_CFLAGS)
AC_SUBST(LIBPANEL_APPLET_LIBS)

PKG_CHECK_MODULES(FISH, gtk+-3.0 >= $GTK_REQUIRED cairo >= $CAIRO_REQUIRED)
AC_SUBST(FISH_CFLAGS)
AC_SUBST(FISH_LIBS)

PKG_CHECK_MODULES(NOTIFICATION_AREA, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(NOTIFICATION_AREA_CFLAGS)
AC_SUBST(NOTIFICATION_AREA_LIBS)

PKG_CHECK_MODULES(WNCKLET, gtk+-3.0 >= $GTK_REQUIRED libwnck-3.0 >= $LIBWNCK_REQUIRED)
AC_SUBST(WNCKLET_CFLAGS)
AC_SUBST(WNCKLET_LIBS)

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_FUNCS(nl_langinfo)

PKG_CHECK_MODULES(TZ, gio-2.0 >= $GLIB_REQUIRED)
AC_SUBST(TZ_CFLAGS)
AC_SUBST(TZ_LIBS)

PKG_CHECK_MODULES(CLOCK, pango >= $PANGO_REQUIRED gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gio-2.0 >= $GLIB_REQUIRED librsvg-2.0 >= $LIBRSVG_REQUIRED gweather-3.0 >= $GWEATHER_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED polkit-gobject-1 dconf >= $DCONF_REQUIRED)
AC_SUBST(CLOCK_CFLAGS)
AC_SUBST(CLOCK_LIBS)

# Check whether to enable e-d-s support for clock applet
AC_MSG_CHECKING([whether to enable evolution-data-server support])
AC_ARG_ENABLE([eds],
    AS_HELP_STRING([--enable-eds], [Whether to enable evolution-data-server support @<:@default=auto@:>@]),
    [], [enable_eds=auto])
AC_MSG_RESULT([$enable_eds])
CLOCK_EDS_ICONDIR=
if test "$enable_eds" != "no"; then
    PKG_CHECK_MODULES(CLOCK_EDS,[
        libecal-1.2 >= $LIBECAL_REQUIRED
        libedataserver-1.2 >= $LIBEDATASERVER_REQUIRED
     ], [have_eds=yes], [have_eds=no])

    if test "$enable_eds" = "yes" -a "$have_eds" = "no"; then
        AC_MSG_ERROR([evolution-data-server support was requested but required dependencies were not found: $CLOCK_EDS_PKG_ERRORS])
    elif test "$have_eds" = "yes"; then
        enable_eds=yes
        AC_DEFINE([HAVE_EDS], 1, [Define to enable evolution-data-server support])
        CLOCK_EDS_ICONDIR=`$PKG_CONFIG --variable=datadir evolution-data-server-1.2`/pixmaps/evolution-data-server
    else
        enable_eds=no
    fi
fi

AM_CONDITIONAL(ENABLE_EDS, test "$enable_eds" = "yes")
AC_SUBST(CLOCK_EDS_ICONDIR)

# For the run dialog
gl_CHECK_TYPE_STRUCT_DIRENT_D_TYPE

dnl X development libraries check

PKG_CHECK_MODULES(X, x11 xau, :, [
  # pkg-config modules not found (only present since X11R7 aka Xorg); use
  # old-style detection
  AC_PATH_XTRA()
  # X not found
  if test x$no_x = xyes ; then
    AC_MSG_ERROR([X development libraries not found])
  fi

  gp_save_cflags="$CFLAGS"
  gp_save_libs="$LIBS"
  CFLAGS="$X_CFLAGS"
  LIBS="$X_PRE_LIBS $X_LIBS $X_EXTRA_LIBS"
  AC_CHECK_LIB(X11, XFree,, AC_MSG_ERROR([libX11 not found]))
  AC_CHECK_LIB(Xau, XauFileName,, AC_MSG_ERROR([libXau not found]))
  CFLAGS="$gp_save_cflags"
  LIBS="$gp_save_libs"

  X_LIBS="$X_PRE_LIBS $X_LIBS -lX11 -lXau $X_EXTRA_LIBS"
])

AC_SUBST(X_LIBS)

dnl X RANDR extension

PKG_CHECK_MODULES(XRANDR, xrandr >= 1.2.0, have_randr=yes, have_randr=no)
if test "x$have_randr" = "xyes"; then
  AC_DEFINE(HAVE_RANDR, 1, [Have the Xrandr extension library])
fi

dnl Modules dir
AC_SUBST([modulesdir],"\$(libdir)/gnome-panel/modules")

dnl Applets dir
AC_SUBST([appletsdir],"\$(datadir)/gnome-panel/\$(LIBPANEL_APPLET_API_VERSION)/applets")
AC_SUBST([appletsdir_pkgconfig],"\${datadir}/gnome-panel/\${libpanel_applet_api_version}/applets")

dnl  Language Support

GETTEXT_PACKAGE=gnome-panel-3.0
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_GNU_GETTEXT

dnl gtk-doc stuff

GTK_DOC_CHECK([1.0])

dnl yelp-tools stuff

YELP_HELP_INIT

dnl platform test
case $host in
*-*-solaris*)
	ostype=solaris
	AC_DEFINE(HAVE_SOLARIS, 1, [Define to 1])
	;;
esac

AC_CONFIG_FILES([
Makefile
data/Makefile
data/16x16/Makefile
data/22x22/Makefile
data/24x24/Makefile
data/32x32/Makefile
data/48x48/Makefile
data/scalable/Makefile
gnome-panel/Makefile
gnome-panel/libpanel-applet-private/Makefile
gnome-panel/libpanel-util/Makefile
gnome-panel/gnome-panel.desktop.in
libpanel-applet/Makefile
libpanel-applet/libpanel-applet.pc
libpanel-applet/libpanel-applet-uninstalled.pc
po/Makefile.in
applets/Makefile
applets/clock/Makefile
applets/clock/pixmaps/Makefile
applets/fish/Makefile
applets/notification_area/Makefile
applets/wncklet/Makefile
doc/Makefile
doc/reference/Makefile
doc/reference/panel-applet/Makefile
help/Makefile
help/clock/Makefile
help/fish/Makefile
man/Makefile
])

AC_OUTPUT

if echo foo | xgettext --from-code=UTF-8 -LC -o - - 2>/dev/null ; then
:;
else
echo "Warning: Your version of gettext does not support --from-code."
echo "         This will cause translation of some strings in non US-English"
echo "         locales to fail. For full support please upgrade to GNU"
echo "         gettext 0.12 or later."
echo
fi

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              gnome-panel $VERSION
              `echo gnome-panel $VERSION | sed "s/./=/g"`

        prefix:                        ${prefix}
        exec_prefix:                   ${exec_prefix}
        libdir:                        ${libdir}
        bindir:                        ${bindir}
        sbindir:                       ${sbindir}
        sysconfdir:                    ${sysconfdir}
        localstatedir:                 ${localstatedir}
        datadir:                       ${datadir}
        source code location:          ${srcdir}
        compiler:                      ${CC}
        cflags:                        ${CFLAGS}
        Maintainer mode:               ${USE_MAINTAINER_MODE}
        Use *_DISABLE_DEPRECATED:      ${enable_deprecation_flags}

        Evolution-Data-Server support: ${enable_eds}
        Telepathy-Glib support:        ${HAVE_TELEPATHY_GLIB}
        XRandr support:                ${have_randr}
        Build introspection support:   ${found_introspection}
        Build gtk-doc documentation:   ${enable_gtk_doc}

"
