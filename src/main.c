/*

  Copyright (c) 2009, Nokia Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  
    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.  
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.  
    * Neither the name of Nokia nor the names of its contributors 
    may be used to endorse or promote products derived from this 
    software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */
/*
 * Whiteboard daemon
 *
 * main.c
 *
 * Copyright 2007 Nokia Corporation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <glib.h>

#include <whiteboard_log.h>

#include "dbushandler.h"
#include "whiteboard_control.h"
#include "whiteboard_sib_handler.h"

WhiteBoardControl *whiteboard_control = NULL;
GMainLoop *whiteboard_mainloop = NULL;

void main_signal_handler(int sig)
{
	static volatile sig_atomic_t signalled = 0;

	whiteboard_log_debug_fb();

	if ( 1 == signalled )
	{
		signal(sig, SIG_DFL);
		raise(sig);
	}
	else
	{
		signalled = 1;
		whiteboard_control_stop_all(whiteboard_control);
		g_main_loop_quit(whiteboard_mainloop);
	}

	whiteboard_log_debug_fe();
}

int main(int argc, char **argv)
{
	DBusHandler *dbushandler = NULL;
	WhiteBoardSIBHandler *whiteboard_sib_handler = NULL;
	
	whiteboard_log_debug_fb();

	g_type_init();

	g_thread_init(NULL);

	dbus_g_thread_init();
	/* Set signal handlers */
	signal(SIGINT, main_signal_handler);
	signal(SIGTERM, main_signal_handler);

	/* Create new main loop */
	whiteboard_mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_ref(whiteboard_mainloop);

	/* TODO: remove hardcoded values */
	/* Create a new DBus connection handler */
	whiteboard_log_debug("Creating dbus handler.\n");
	dbushandler = dbushandler_new("/tmp/dbus-test",
				      whiteboard_mainloop);
	whiteboard_log_debug("Done\n");

	/* Create the node access component */
	whiteboard_log_debug("Creating sib access handler.\n");
	whiteboard_sib_handler = whiteboard_sib_handler_new(dbushandler);
	whiteboard_log_debug("Done\n");

	/* Create new control object and start all sinks/sources */
	whiteboard_log_debug("Creating control object and starting sibaccess/sib modules.\n");
	whiteboard_control = whiteboard_control_new();
        
#if ENABLE_SIB_ACCESS_STARTUP == 1
	//start processes due to: ./configure --libexecdir=/usr/local/lib/whiteboard/libexec
	whiteboard_control_start_all_from(whiteboard_control, WHITEBOARD_LIBEXECDIR);
#endif
	whiteboard_log_debug("Done\n");
	/* Enter main loop and block */
	
	g_main_loop_run(whiteboard_mainloop);

	whiteboard_log_debug("Finished, cleaning up.\n");
	whiteboard_control_stop_all(whiteboard_control);

	whiteboard_control_destroy(whiteboard_control);
	whiteboard_sib_handler_destroy(whiteboard_sib_handler);
	dbushandler_destroy(dbushandler);

	whiteboard_log_debug("Normal exit.\n");

	whiteboard_log_debug_fe();

	return 0;
}
