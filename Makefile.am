SUBDIRS =			\
	gnome-panel		\
	libpanel-applet		\
	applets			\
	doc			\
	help			\
	data			\
	man			\
	po

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-introspection \
	--with-girdir='$${datadir}/gir-distcheck' \
	--with-typelibdir='$${libdir}/girepository-distcheck'

EXTRA_DIST = \
	COPYING-DOCS		\
	HACKING

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	$(srcdir)/configure \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4/gtk-doc.m4 \
	$(srcdir)/m4/intltool.m4 \
	$(srcdir)/m4/libtool.m4 \
	$(srcdir)/m4/ltoptions.m4 \
	$(srcdir)/m4/ltsugar.m4 \
	$(srcdir)/m4/ltversion.m4 \
	$(srcdir)/m4/lt~obsolete.m4

CHANGELOG_GIT_RANGE = GNOME_PANEL_2_26_1..
dist-hook:
	$(AM_V_GEN)if test -d "$(srcdir)/.git"; then \
	  ( echo '# Generated by Makefile. Do not edit.'; echo; \
	    GIT_DIR="$(top_srcdir)/.git" ./missing --run \
	    git log $(CHANGELOG_GIT_RANGE) --no-color -M -C --name-status ) \
	  > ChangeLog.tmp \
	  && mv -f ChangeLog.tmp $(distdir)/ChangeLog \
	  || ( rm -f ChangeLog.tmp ; echo Failed to generate ChangeLog >&2 ); \
	else \
	  echo A git checkout is required to generate ChangeLog >&2; \
	fi

GITIGNOREFILES = $(PACKAGE)-\*.tar.{gz,bz2,xz}
-include $(top_srcdir)/git.mk
