#############################################################################
#
#	WhiteBoard Daemon
#
#	Copyright (c) 2007 Nokia Corporation
#
#############################################################################

AC_INIT()
AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE(whiteboardd, 2.0-beta1)
AM_CONFIG_HEADER(config.h)

#############################################################################
# Constants
#############################################################################

#############################################################################
# Localisation
#############################################################################
#GETTEXT_PACKAGE=$PACKAGE
#AC_SUBST(GETTEXT_PACKAGE)
#AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, 
#		   "$GETTEXT_PACKAGE", [Define gettext package])
#ALL_LINGUAS="en_GB"
#AM_GLIB_GNU_GETTEXT

#localedir=`$PKG_CONFIG osso-af-settings --variable=localedir`
#AC_SUBST(localedir)
#AC_DEFINE_UNQUOTED([PACKAGE_LOCALE_DIR], 
#		   ["${prefix}/${DATADIRNAME}/locale"], [Locale directory])

#############################################################################
# Checks for some common programs needed for configure/compile/install
#############################################################################

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_LIBTOOL
AC_PROG_RANLIB

#############################################################################
# Check whether debug prints should be used
#############################################################################
AC_ARG_WITH(debug, 
	AS_HELP_STRING([--with-debug],
		       [Print additional debug messages (default = no)]),
	[AC_DEFINE([WHITEBOARD_DEBUG],[1],[Print debug messages])],
	[with_debug=no])


#############################################################################
# Check whether timestamp prints should be used
#############################################################################
AC_ARG_WITH(timestamps, 
	AS_HELP_STRING([--with-timestamps],
		       [Print timestamps for benchmarking (default = no)]),
	[AC_DEFINE([WHITEBOARD_TIMESTAMP_ENABLED],[1],[Print timestamp messages])],
	[with_timestamps=no])

#############################################################################
# Check whether unit tests should be built
#############################################################################
AC_ARG_WITH(unit-tests,
	[AS_HELP_STRING([--with-unit-tests], [Build unit tests (default = no)])],
	[AC_DEFINE([WHITEBOARD_UNIT_TESTS_ENABLED], [1], [Build unit tests])
	 PKG_CHECK_MODULES(CHECK, check >= 0.9.4)
	 with_unit_tests=yes],
	[with_unit_tests=no]
)
AM_CONDITIONAL(UNIT_TESTS, test $with_unit_tests = yes)

#############################################################################
# Check whether to start SIBAccess processes
#############################################################################
AC_ARG_ENABLE(sib-access-startup, 
	AS_HELP_STRING([--enable-sib-access-startup],
		       [Start all sib access processes (default=yes)]),
	[
		if test $enableval = yes; then
			AC_DEFINE([ENABLE_SIB_ACCESS_STARTUP],[1], [Enable SIB access startup])
			ENABLE_SIB_ACCESS_STARTUP=yes	
		else
			ENABLE_SIB_ACCESS_STARTUP=no
		fi
	],[
		ENABLE_SIB_ACCESS_STARTUP=yes	
		AC_DEFINE([ENABLE_SIB_ACCESS_STARTUP],[1], [Enable SIB access startup])
	]
)
AM_CONDITIONAL(ENABLE_SIB_ACCESS_STARTUP, test $ENABLE_SIB_ACCESS_STARTUP = yes)

#############################################################################
# Check for some header files (TODO: Add more?)
#############################################################################
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h stdlib.h string.h unistd.h])

##############################################################################
# Check for iconv
##############################################################################
#AC_CHECK_FUNCS([iconv])
#AC_CHECK_HEADERS([iconv.h])
#AC_CHECK_LIB([iconv],[main])

##############################################################################
# Check for pthread presence
##############################################################################
AC_CHECK_HEADERS([pthread.h],
		 [],
		 [AC_MSG_ERROR(pthread library required)])
AC_CHECK_LIB([pthread],[main])

##############################################################################
# Check for GNOME environment
##############################################################################
PKG_CHECK_MODULES(GNOME,
[
	glib-2.0 >= 2.8.6
	dbus-1 >= 0.61
	dbus-glib-1 >= 0.61
])

AC_SUBST(GNOME_CFLAGS)
AC_SUBST(GNOME_LIBS)


#PKG_CHECK_MODULES(CONIC,
#[
#	conic >= 0.13
#])

#AC_SUBST(CONIC_FLAGS)
#AC_SUBST(CONIC_LIBS)

##############################################################################
# Check for the WhiteBoard lib
##############################################################################
PKG_CHECK_MODULES(LIBWHITEBOARD,
[
	libwhiteboard-util >= 2.0-beta1
])

AC_SUBST(LIBWHITEBOARD_CFLAGS)
AC_SUBST(LIBWHITEBOARD_LIBS)

##############################################################################
# Checks for typedefs, structures, and compiler characteristics.
##############################################################################

AC_C_CONST
AC_C_INLINE
AC_STRUCT_TM

##############################################################################
# Locale stuff
##############################################################################

AC_DEFINE_UNQUOTED([LOCALEDIR], 
		   "${runtime_localedir}", [Runtime locale catalog files path])

##############################################################################
# Check compile target to be able to disable local player in emulator
##############################################################################

case $host in
        i386-pc-linux* | i686-pc-linux*)
               CFLAGS="$CFLAGS -Di686"
	       USE_i686=yes
	       USE_ARM=no
	       USE_SOLARIS=no
               ;;
	i386-pc-solaris* | i686-pc-solaris*)
               CFLAGS="$CFLAGS -Di686"
	       LDFLAGS="$LDFLAGS -lsocket -lnsl"
	       USE_i686=yes
	       USE_ARM=no
	       USE_SOLARIS=yes
	       ;;
        arm-*)
               CFLAGS="$CFLAGS -DARM"
	       USE_ARM=yes
	       USE_i686=no
	       USE_SOLARIS=no
               ;;
esac

AM_CONDITIONAL(ARM, test $USE_ARM=yes)
AM_CONDITIONAL(i686, test $USE_i686=yes)
AM_CONDITIONAL(SOLARIS, test $USE_SOLARIS=yes)

##############################################################################
# Files to create
##############################################################################

AC_OUTPUT(
	Makefile
	include/Makefile
	src/Makefile 
	etc/Makefile
	etc/com.nokia.whiteboard.service
)
echo "Debug messages: " ${with_debug}
echo "Starting SIB-access processes: " ${ENABLE_SIB_ACCESS_STARTUP}
