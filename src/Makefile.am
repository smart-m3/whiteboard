bin_PROGRAMS = whiteboardd

# Compiler flags
whiteboardd_CFLAGS  = -Wall -I$(top_srcdir)/include -I/usr/include -I.
whiteboardd_CFLAGS += @GNOME_CFLAGS@ @LIBWHITEBOARD_CFLAGS@

# TODO: Define this in a more global and flexible way

whiteboardd_CFLAGS += -DWHITEBOARD_LIBEXECDIR=\"$(libdir)/whiteboard/libexec\"

# Linker flags
whiteboardd_LDFLAGS = @GNOME_LIBS@ @LIBWHITEBOARD_LIBS@ -lgthread-2.0


AM_CFLAGS = $(whiteboardd_CFLAGS) -g

# Put these in alphabetical order so they are easy to find.
# Leave main.c out of this list because it must not be present
# in the unit testing library build.
sources = \
	access_sib.c \
	dbushandler.c \
	whiteboard_control.c \
	whiteboard_sib_handler.c

whiteboardd_SOURCES = \
	main.c \
	$(sources)


##############################################################################
# Library building for unit tests
##############################################################################
if UNIT_TESTS

noinst_LTLIBRARIES = libwhiteboarddtest.la

# Compiler flags
libwhiteboarddtest_la_CFLAGS  = -Wall -I$(top_srcdir)/include -I/usr/include -I.
libwhiteboarddtest_la_CFLAGS += @GNOME_CFLAGS@ @LIBWHITEBOARD_CFLAGS@

# TODO: Define this in a more global and flexible way
libwhiteboarddtest_la_CFLAGS += -DMEDIAHUB_LIBEXECDIR=\"$(libdir)/mediahub/libexec\"

# Linker flags
libwhiteboarddtest_la_LIBADD = @GNOME_LIBS@ @LIBWHITEBOARD_LIBS@

# List of source files, except main.c, which must not be present in unit tests
libwhiteboarddtest_la_SOURCES = $(sources)

endif
