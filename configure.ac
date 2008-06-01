# This will be lowercased when automake will create the distribution
# directories. The version number is currently set at 0.1, but new
# distributed versions should change this number (no other variables
# need be touched).
AC_INIT(malarm, 0.1)

# Tell automake to prepare for real work.
AM_INIT_AUTOMAKE
# Check for the C compiler.
AC_PROG_CC
# Check that 'install' program is available (used by the automake
# generated Makefiles).
AC_PROG_INSTALL

# Check whether the necessary pkg-config packages are present. The
# PKG_CHECK_MODULES macro is supplied by pkg-config
# (/usr/share/aclocal/).
#
# The first parameter will be the variable name prefix that will be
# used to create two variables: one to hold the CFLAGS required by
# the packages, and one to hold the LDFLAGS (LIBS) required by the
# packages. The variable name prefix (MALARM) can be chosen freely.
PKG_CHECK_MODULES(MALARM, gtk+-2.0 hildon-1 hildon-fm-2 gnome-vfs-2.0 \
                       gconf-2.0 libosso libalarm)
# At this point MALARM_CFLAGS will contain the necessary compiler flags
# and MALARM_LIBS will contain the linker options necessary for all the
# packages listed above.
#
# Add the pkg-config supplied values to the ones that are used by
# Makefile or supplied by the user running ./configure.
CFLAGS="$MALARM_CFLAGS $CFLAGS"
LIBS="$MALARM_LIBS $LIBS"

# Generate the Makefile from Makefile.in
AC_OUTPUT(Makefile)

# Generate the service and desktop files based on the templates.
AC_OUTPUT(malarm.desktop org.maemo.malarm.service)

