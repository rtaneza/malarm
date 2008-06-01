# List of the filenames of binaries that this project will produce.
bin_PROGRAMS = malarm

# For each binary file name, list the source files required for it to
# build
malarm_SOURCES = malarm_main.c malarm_main.h \
				 malarm_ui.c malarm_ui.h \
				 malarm_util.c malarm_util.h

# In order for the desktop and service to be copied into the correct
# places (and to support prefix-redirection), use the following
# configuration:
dbusdir=$(datadir)/dbus-1/services
dbus_DATA=org.maemo.malarm.service
desktopdir=$(datadir)/applications/hildon
desktop_DATA=malarm.desktop
# We described two directories and gave automake a list of files
# which are to be copied into these directories on install. Without
# these directives, the desktop and service files would never be
# installed even if they would be distributed (using EXTRA_DIST).

soundsdir=$(datadir)/sounds
sounds_DATA = malarm_silent.mp3
EXTRA_DIST = $(sounds_DATA)

