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
 * Whiteboard Daemon
 *
 * whiteboard_sib_handler.h
 *
 * Copyright 2007 Nokia Corporation
 */

#ifndef WHITEBOARD_SIB_HANDLER_H
#define WHITEBOARD_SIB_HANDLER_H

#include "dbushandler.h"

struct _WhiteBoardSIBHandler;
typedef struct _WhiteBoardSIBHandler WhiteBoardSIBHandler;

/**
 * Create a new sib handler instance
 * @param dbus_handler Pointer to dbushandler instance.
 * @return A pointer to a WhiteBoardSibHandler instance
 */
WhiteBoardSIBHandler *whiteboard_sib_handler_new(DBusHandler* dbus_handler);

/**
 * Destroy a sib handler instance
 *
 * @param self A pointer to WhiteBoardSibHandler instance
 */
void whiteboard_sib_handler_destroy(WhiteBoardSIBHandler* self);

/**
 * Get a new sib_handler transaction id (never returns the same id twice)
 *
 * @return A new sib_handler transaction id
 */
gint whiteboard_sib_handler_get_access_id();

/**
 * Remove node from internal databases
 *
 **/
void whiteboard_sib_handler_remove_nodedata( WhiteBoardSIBHandler *self,
					     gchar *nodeid);

#endif
