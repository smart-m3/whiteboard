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
 * WhiteBoard daemon.
 *
 * access_sib.h
 *
 * Copyright 2007 Nokia Corporation
 */

#ifndef ACCESS_SIB_H
#define ACCESS_SIB_H

#include <glib.h>
#include <whiteboard_util.h>
#include <whiteboard_log.h>

struct _AccessSIB;

typedef struct _AccessSIB AccessSIB;

/*****************************************************************************
 * Creation/destruction
 *****************************************************************************/

/**
 * Create a new AccessSIB instance
 *
 * @param uuid The source's UUID
 * @param name Friendly name
 */
AccessSIB* access_sib_new(gchar* uuid, gchar* name);

/**
 * Increase the reference count of a AccessSIB instance
 *
 * @param source A AccessSIB instance
 * @return refcount after the operation
 */
gint access_sib_ref(AccessSIB* source);

/**
 * Decrease the reference count of a AccessSIB instance
 *
 * @param source A AccessSIB instance
 * @return refcount after the operation
 */
gint access_sib_unref(AccessSIB* source);

/**
 * Destroy a AccessSIB instance
 *
 * @param source A AccessSIB instance
 */
void access_sib_destroy(AccessSIB* source);

/*****************************************************************************
 * Set/get functions
 *****************************************************************************/

/**
 * Get uuid of source. Note, this takes a copy. Remember to free it
 *
 * @param sink A AccessSIB instance
 * @param uuid Pointer where uuid of the source is copied
 * @return TRUE if successful, FALSE if uuid was NULL
 */
gboolean access_sib_get_uuid(AccessSIB* source, gchar** uuid);

/**
 * Get name of source. Note, this takes a copy. Remember to free it
 *
 * @param sink A AccessSIB instance
 * @param name Pointer where name of the source is copied
 * @return TRUE if successful, FALSE if name was NULL
 */
gboolean access_sib_get_name(AccessSIB* source, gchar** name);

/**
 * Compare a AccessSIB with a access_sib id
 *
 * @param a A AccessSIB instance
 * @param b A UUID to compare with the AccessSIB instance
 */
gint access_sib_compare_id(gconstpointer a, gconstpointer b);

gboolean access_sib_is_node_joined( AccessSIB *node, const gchar *nodeid);

void access_sib_add_to_joined_nodes(  AccessSIB *node, gchar *nodeid);

void access_sib_remove_from_joined_nodes(  AccessSIB *node, gchar *nodeid);

GList *access_sib_get_joined_nodes(AccessSIB *self);

#endif
