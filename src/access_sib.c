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
 * access_sib.c
 *
 * Copyright 2007 Nokia Corporation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <glib.h>
#include <whiteboard_util.h>
#include <whiteboard_log.h>

#include "access_sib.h"

struct _AccessSIB
{
  gchar* uuid;
  gchar* name;

  // list for joined nodes
  GList *joined_nodes; 
  
  gint refcount;
};

/* Keep this preprocessor instruction always AFTER struct definitions
   and BEFORE any function declaration/prototype */
#ifndef UNIT_TEST_INCLUDE_IMPLEMENTATION

/*****************************************************************************
 * Creation/destruction
 *****************************************************************************/

/**
 * Create a new AccessSIB instance
 *
 * @param uuid The source's UUID
 * @param name Friendly name
 */
AccessSIB* access_sib_new(gchar* uuid, gchar* name)
{
	AccessSIB* source = NULL;
	
	whiteboard_log_debug_fb();
	
	g_return_val_if_fail(uuid != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);
	
	source = g_new0(AccessSIB, 1);
	source->uuid = g_strdup(uuid);
	source->name = g_strdup(name);
	
	source->joined_nodes = NULL;
	
	source->refcount = 1;
 
	whiteboard_log_debug_fe();

	return source;	
}

/**
 * Increase the reference count of a AccessSIB instance
 *
 * @param source A AccessSIB instance
 * @return refcount after the operation
 */
gint access_sib_ref(AccessSIB* source)
{
	whiteboard_log_debug_fb();

	g_return_val_if_fail(source != NULL, 0);

	if (g_atomic_int_get(&source->refcount) > 0)
	{
		/* Refcount is something sensible */
		g_atomic_int_inc(&source->refcount);

		whiteboard_log_debugc(WHITEBOARD_DEBUG_NODE, 
				    "AccessSIB %s (%s) refcount: %d\n",
				    source->name, source->uuid,
				    source->refcount);
	}
	else
	{
		/* Refcount is already zero */
		whiteboard_log_debugc(WHITEBOARD_DEBUG_NODE,
				    "AccessSIB %s (%s) refcount already %d!\n",
				    source->name, source->uuid,
				    source->refcount);
	}

	whiteboard_log_debug_fe();
	return source->refcount;
}

/**
 * Decrease the reference count of a AccessSIB instance
 *
 * @param source A AccessSIB instance
 * @return refcount after the operation
 */
gint access_sib_unref(AccessSIB* source)
{
	whiteboard_log_debug_fb();

	g_return_val_if_fail(source != NULL, 0);

	if (g_atomic_int_dec_and_test(&source->refcount) == FALSE)
	{
		whiteboard_log_debugc(WHITEBOARD_DEBUG_NODE,
				    "AccessSIB %s (%s) refcount dec: %d\n",
				    source->name, source->uuid,
				    source->refcount);
	}
	else
	{
		whiteboard_log_debugc(WHITEBOARD_DEBUG_NODE,
				    "AccessSIB %s (%s) refcount dec: %d\n",
				    source->name, source->uuid,
				    source->refcount);
		access_sib_destroy(source);
	}

	whiteboard_log_debug_fe();
	return source->refcount;
}

/**
 * Destroy a AccessSIB instance
 *
 * @param source A AccessSIB instance
 */
void access_sib_destroy(AccessSIB* source)
{
	whiteboard_log_debug_fb();

	g_return_if_fail(source != NULL);

	g_free(source->uuid);
	source->uuid = NULL;

	g_free(source->name);
	source->name = NULL;
	
	g_list_free(source->joined_nodes);
	
	g_free(source);

	whiteboard_log_debug_fe();
}

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
gboolean access_sib_get_uuid(AccessSIB* source, gchar** uuid)
{
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(uuid != NULL, FALSE);

	if (source->uuid == NULL)
		return FALSE;

	*uuid = g_strdup(source->uuid);
	return TRUE;
}

/**
 * Get name of source. Note, this takes a copy. Remember to free it
 *
 * @param sink A AccessSIB instance
 * @param name Pointer where name of the source is copied
 * @return TRUE if successful, FALSE if name was NULL
 */
gboolean access_sib_get_name(AccessSIB* source, gchar** name)
{
	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(name != NULL, FALSE);

	if (source->name == NULL)
		return FALSE;

	*name = g_strdup(source->name);
	return TRUE;
}

/**
 * Compare a AccessSIB with a access_sib id
 *
 * @param a A AccessSIB instance
 * @param b A UUID to compare with the AccessSIB instance
 */
gint access_sib_compare_id(gconstpointer a, gconstpointer b)
{
	if (a == NULL)
		return -1;
	else if (b == NULL)
		return 1;
	else
		return g_strcasecmp(((AccessSIB*)a)->uuid, (gchar*) b);
}

gint access_sib_compare_nodeid(gconstpointer a, gconstpointer b)
{
	if (a == NULL)
		return -1;
	else if (b == NULL)
		return 1;
	else
	  return g_strcasecmp( (gchar *)a, (gchar*) b);
}

GList *access_sib_get_joined_nodes( AccessSIB *self)
{
  g_return_val_if_fail(self!=NULL,NULL);
  return self->joined_nodes;
}

gboolean access_sib_is_node_joined( AccessSIB *node, const gchar *nodeid)
{
  GList *result = NULL;
  whiteboard_log_debug_fb();
  result = g_list_find_custom(node->joined_nodes, nodeid, access_sib_compare_nodeid);
  
  whiteboard_log_debug_fe();
  return (result != NULL);
}

void access_sib_add_to_joined_nodes(  AccessSIB *node, gchar *nodeid)
{
  gchar *id=NULL;
  whiteboard_log_debug_fb();
  if( FALSE == access_sib_is_node_joined(node, nodeid) )
    {
      id=g_strdup(nodeid); 
      node->joined_nodes = g_list_prepend( node->joined_nodes, id);
    }
  whiteboard_log_debug_fe();
}

void access_sib_remove_from_joined_nodes( AccessSIB *node, gchar *nodeid)
{
  GList *result = NULL;
  whiteboard_log_debug_fb();
  result = g_list_find_custom(node->joined_nodes, nodeid, access_sib_compare_nodeid);
  if(result != NULL)
    {
      node->joined_nodes = g_list_remove_link(node->joined_nodes, result);
      g_free(result->data);
    }
  else
    {
      whiteboard_log_warning("Cannot find node to remove from joined_nodes list");
    }
  
  whiteboard_log_debug_fe();
}

/* Keep this preprocessor instruction always at the end of the file */
#endif /* UNIT_TEST_INCLUDE_IMPLEMENTATION */
