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
 * WhiteBoard Daemon
 *
 * whiteboard_sib_handler.c
 *
 * Copyright 2007 Nokia Corporation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <glib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <whiteboard_util.h>
#include <whiteboard_dbus_ifaces.h>
#include <whiteboard_node.h>
#include <whiteboard_log.h>
#include <sibmsg.h>

#include "dbushandler.h"
#include "access_sib.h"
#include "whiteboard_sib_handler.h"


typedef struct _JoinData
{
  gchar *sib;
  gchar *node;
} JoinData;

struct _WhiteBoardSIBHandler
{
  DBusHandler *dbus_handler;
  GList* sib_list; // all registered SIBs 

  // nodeid -> server URI for joined nodes.
  GHashTable *joined_nodes_map;

  // accessid -> JoinData 
  GHashTable *joindata_map;
};

/* Keep this preprocessor instruction always AFTER struct definitions
   and BEFORE any function declaration/prototype */
#ifndef UNIT_TEST_INCLUDE_IMPLEMENTATION

/*****************************************************************************
 * Private function prototypes
 *****************************************************************************/

static void whiteboard_sib_handler_dbus_cb(DBusHandler *context, WhiteBoardPacket *packet,
					   gpointer user_data);

static void whiteboard_sib_handler_sib_registered_cb(DBusHandler* context, 
						      gchar* uuid, gchar* name,
						      gpointer user_data);

static void whiteboard_sib_handler_node_disconnected_cb(DBusHandler* context, 
							gchar* uuid,
							gpointer user_data);

static gint whiteboard_sib_handler_handle_method_get_description(DBusHandler *context,
								 WhiteBoardPacket *packet,
								 gpointer user_data);

static gint whiteboard_sib_handler_handle_method_refresh_node(DBusHandler *context,
							      WhiteBoardPacket *packet,
							      gpointer user_data);

static gint whiteboard_sib_handler_handle_join(DBusHandler *context,
					       WhiteBoardPacket *packet,
					       gpointer user_data);

static gint whiteboard_sib_handler_handle_insert(DBusHandler *context,
						  WhiteBoardPacket *packet,
						  gpointer user_data);

static gint whiteboard_sib_handler_handle_update(DBusHandler *context,
						  WhiteBoardPacket *packet,
						  gpointer user_data);

static gint whiteboard_sib_handler_handle_remove(DBusHandler *context,
						 WhiteBoardPacket *packet,
						 gpointer user_data);

static gint whiteboard_sib_handler_handle_subscribe_query(DBusHandler *context,
						    WhiteBoardPacket *packet,
							  gpointer user_data);

static gint whiteboard_sib_handler_handle_unsubscribe(DBusHandler *context,
						      WhiteBoardPacket *packet,
						      gpointer user_data);
static gint whiteboard_sib_handler_handle_unsubscribe_complete(DBusHandler *context,
							       WhiteBoardPacket *packet,
							       gpointer user_data);

static gint whiteboard_sib_handler_handle_leave(DBusHandler *context,
						WhiteBoardPacket *packet,
						gpointer user_data);

static gint whiteboard_sib_handler_handle_signal_join_complete(DBusHandler *context,
							       WhiteBoardPacket *packet,
							       gpointer user_data);

static gint whiteboard_sib_handler_handle_signal_subscription_ind(DBusHandler *context,
								  WhiteBoardPacket *packet,
								  gpointer user_data);

static gint whiteboard_sib_handler_handle_subscribe_return(DBusHandler *context,
							   WhiteBoardPacket *packet,
							   gpointer user_data);

static const gchar *whiteboard_sib_handler_get_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
								    const gchar *nodeid);

static void  whiteboard_sib_handler_add_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
							     gchar *node,
							     gchar *sib);

static gboolean whiteboard_sib_handler_remove_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
								   const gchar *nodeid);



static void whiteboard_sib_handler_add_joindata_by_accessid(WhiteBoardSIBHandler *self,
							    gint accessid,
							    JoinData *jd);
static JoinData *whiteboard_sib_handler_get_joindata_by_accessid(WhiteBoardSIBHandler *self,
								 gint accessid);

static gboolean whiteboard_sib_handler_remove_joindata_by_accessid(WhiteBoardSIBHandler* self, gint accessid);



/*****************************************************************************
 * Creation/destruction
 *****************************************************************************/

WhiteBoardSIBHandler *whiteboard_sib_handler_new(DBusHandler *dbus_handler)
{
  static gboolean instantiated = FALSE;
  WhiteBoardSIBHandler *self = NULL;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != dbus_handler, NULL);

  if (instantiated == TRUE)
    {
      whiteboard_log_error("WhiteBoardSIBHandler already instantiated. " \
			   "Won't create another instance.\n");
      return NULL;
    }

  self = g_new0(struct _WhiteBoardSIBHandler, 1);

  self->dbus_handler = dbus_handler;
  dbushandler_set_callback_sib_handler(dbus_handler,
				       whiteboard_sib_handler_dbus_cb,
				       self);
  dbushandler_set_callback_sib_registered(dbus_handler,
					  whiteboard_sib_handler_sib_registered_cb,
					  self);

  dbushandler_set_callback_node_disconnected(dbus_handler,
					whiteboard_sib_handler_node_disconnected_cb,
					self);

  self->sib_list = NULL;

  self->joined_nodes_map = g_hash_table_new_full(g_str_hash, g_str_equal,
						 g_free, g_free);
  self->joindata_map = g_hash_table_new(g_direct_hash, g_direct_equal);
  if (NULL != self)
    instantiated = TRUE;

  whiteboard_log_debug_fe();

  return self;
}

void whiteboard_sib_handler_destroy(WhiteBoardSIBHandler *self)
{
  GList* link = NULL;

  whiteboard_log_debug_fb();

  g_return_if_fail( NULL != self);

  for (link = g_list_first(self->sib_list);  link != NULL; link = link->next)
    access_sib_unref((AccessSIB*)link->data);
		
  g_list_free(self->sib_list);

  g_hash_table_destroy(self->joined_nodes_map);
  
  g_hash_table_destroy(self->joindata_map);
  
  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			"Destroying sib_handler object.\n");

  g_free(self);

  whiteboard_log_debug_fe();
}

/*****************************************************************************
 * Private utilities
 *****************************************************************************/

gint whiteboard_sib_handler_get_access_id()
{
  static gint whiteboard_sib_handler_id = 0;
  
  return ++whiteboard_sib_handler_id;
}

static void whiteboard_sib_handler_add_sib(WhiteBoardSIBHandler* sib_handler, gchar* uuid,
					    gchar* name)
{
  GList* list = NULL;
  AccessSIB* node = NULL;
  
  whiteboard_log_debug_fb();
  
  g_return_if_fail(NULL != sib_handler);
  g_return_if_fail(NULL != uuid);
  g_return_if_fail(NULL != name);

  /* Check if we already have a sink with the given UUID */
  list = g_list_find_custom(sib_handler->sib_list, uuid, 
			    access_sib_compare_id);
  if (list == NULL)
    {
      /* Create a new node instance and add it to the list */
      node = access_sib_new(uuid, name);
      sib_handler->sib_list = g_list_append(sib_handler->sib_list, node);

      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			    "Added node: %s, %s\n", uuid, name);

      /* Inform all nodes about a new node */
      list = dbushandler_get_discovery_connections(sib_handler->dbus_handler);
      whiteboard_util_send_signal_to_list(WHITEBOARD_DBUS_OBJECT,
					  WHITEBOARD_DBUS_DISCOVERY_INTERFACE,
					  WHITEBOARD_DBUS_DISCOVERY_SIGNAL_SIB_INSERTED,
					  list,
					  DBUS_TYPE_STRING, &uuid,
					  DBUS_TYPE_STRING, &name,
					  WHITEBOARD_UTIL_LIST_END);

    }
  else
    {
      whiteboard_log_warning("Node %s with UUID[%s] already present.\n", 
			     name, uuid);
    }
  
  whiteboard_log_debug_fe();
}

/*****************************************************************************
 * DBus message handlers
 *****************************************************************************/

/* TODO: Change return values to gboolean */

static gint whiteboard_sib_handler_handle_method_get_description(DBusHandler *context,
								 WhiteBoardPacket *packet,
								 gpointer user_data)
{
  DBusConnection* source_connection = NULL;
  gchar* sourceid = NULL;
  
  whiteboard_log_debug_fb();
  
  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_STRING, &sourceid,
				WHITEBOARD_UTIL_LIST_END);
  
  g_return_val_if_fail( sourceid != NULL, -1);
  
  source_connection = dbushandler_get_connection_by_uuid(context, sourceid);
  
  if (NULL == source_connection)
    {
      whiteboard_log_error("Couldn't get connection for service: %s\n",
			   sourceid);
      whiteboard_log_debug_fe();
      return -1;
    }
  
  dbus_message_set_interface(packet->message, WHITEBOARD_DBUS_NODE_INTERFACE);
  
  dbus_connection_send(source_connection, packet->message, NULL);
  dbus_connection_flush(source_connection);
  
  whiteboard_log_debug_fe();
  
  return 1;
}

static gint whiteboard_sib_handler_handle_method_refresh_node(DBusHandler *context,
							      WhiteBoardPacket *packet,
							      gpointer user_data)
{	
 GList* connections = NULL;
  
  whiteboard_log_debug_fb();
  
  dbus_message_set_interface(packet->message, WHITEBOARD_DBUS_CONTROL_INTERFACE);
  
  connections = dbushandler_get_control_connections(context);
  
  whiteboard_util_send_message_to_list(connections, packet->message);
  
  whiteboard_log_debug_fe();
  
  return 1;
}

static gint whiteboard_sib_handler_handle_method_get_sibs(DBusHandler *context,
							   WhiteBoardPacket *packet,
							   gpointer user_data)
{	
  WhiteBoardSIBHandler *sib_handler = NULL;
  gchar* uuid = NULL;
  gchar* name = NULL;
  GList* link = NULL;
  AccessSIB* source = NULL;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( context != NULL, -1);
  g_return_val_if_fail( user_data != NULL, -1);
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;
  
  for (link = g_list_first(sib_handler->sib_list); 
       link != NULL; link = link->next)
    {
      source = (AccessSIB*) link->data;
      access_sib_ref(source);
		
      if (access_sib_get_name(source, &name) == FALSE)
	{
	  whiteboard_log_error("Source name is NULL\n");
	  return -1;
	}
      if (access_sib_get_uuid(source, &uuid) == FALSE)
	{
	  whiteboard_log_error("Source uuid is NULL\n");
	  g_free(name);
	  return -1;
	}
      access_sib_unref(source);

      whiteboard_log_debugc(WHITEBOARD_DEBUG_NODE,
			    "Signaling sib %s\n", name);
	  
      /* Send SIB as a signal to the requesting NODE */
      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
				  WHITEBOARD_DBUS_DISCOVERY_INTERFACE,
				  WHITEBOARD_DBUS_DISCOVERY_SIGNAL_SIB,
				  packet->connection,
				  DBUS_TYPE_STRING, &uuid,
				  DBUS_TYPE_STRING, &name,
				  WHITEBOARD_UTIL_LIST_END);
      g_free(name);
      g_free(uuid);
	  
    } 
  /* Send SIB as a signal to the requesting NODE */
  whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
			      WHITEBOARD_DBUS_DISCOVERY_INTERFACE,
			      WHITEBOARD_DBUS_DISCOVERY_SIGNAL_ALL_FOR_NOW,
			      packet->connection,
			      WHITEBOARD_UTIL_LIST_END);
  whiteboard_log_debug_fe();
  
  return 1;
}

static gint whiteboard_sib_handler_handle_signal_sib_removed(DBusHandler *context,
							      WhiteBoardPacket *packet,
							      gpointer user_data)
{
  gchar* uuid = NULL;
  GList* list = NULL;
  WhiteBoardSIBHandler* sib_handler = NULL;
  GList* connections = NULL;
  GList* joined_nodes = NULL;
  GList* link = NULL;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;

  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_STRING, &uuid,
				WHITEBOARD_UTIL_LIST_END);
  
  /* Remove the source from internal data structures */
  list = g_list_find_custom(sib_handler->sib_list, uuid,
			    access_sib_compare_id);
  
  dbushandler_remove_connection_by_uuid(context, uuid);
  
  if (list == NULL)
    {
      whiteboard_log_warning("Node %s not found. Cannot remove, probably it's a control connection\n",
			     uuid);
    }
  else
    {
      // Remove associations between removed SIB and joined nodes.
      joined_nodes = access_sib_get_joined_nodes((AccessSIB*)list->data);
      
      for( link=g_list_first(joined_nodes); link != NULL; link = link->next)
	{
	  whiteboard_sib_handler_remove_sib_by_joined_nodeid(sib_handler, (gchar *)link->data);
	}
      
      sib_handler->sib_list = g_list_remove_link(sib_handler->sib_list,
						  list);
      
      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
			    "Removing node %s\n", uuid);
      
      
      access_sib_unref((AccessSIB*)list->data);
      
      g_list_free(list);
    }
  
  /* Then send signal to all NODEs */
  connections = dbushandler_get_discovery_connections(context);
  whiteboard_util_send_message_to_list(connections, packet->message);

  return 1;
}

static gint whiteboard_sib_handler_handle_join(DBusHandler *context,
					       WhiteBoardPacket *packet,
					       gpointer user_data)
{
  gboolean retval = FALSE;
  gchar* udn = NULL;
  gchar* nodeid = NULL;
  //apr09obsolete gchar *username = NULL;
  gchar *uuid=NULL;
  GList* list = NULL;
  gint join_id = -1;
  gint msgnum=0;
  AccessSIB *source = NULL;
  WhiteBoardSIBHandler* sib_handler = NULL;
  DBusConnection *conn = NULL;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;
  
  whiteboard_util_parse_message(packet->message,
				//apr09obsolete DBUS_TYPE_STRING, &username,
				DBUS_TYPE_STRING, &nodeid,
				DBUS_TYPE_STRING, &udn,
				DBUS_TYPE_INT32, &msgnum,
				WHITEBOARD_UTIL_LIST_END);

  //apr09obsolete whiteboard_log_debug("UserName: %s\n", username);
  whiteboard_log_debug("Node: %s\n", nodeid);
  whiteboard_log_debug("sib: %s\n", udn);
  whiteboard_log_debug("msgnum: %d\n", msgnum);
  
  /* find the source from internal data structures */
  list = g_list_find_custom(sib_handler->sib_list, udn,
			    access_sib_compare_id);
  
  
  if (list == NULL)
    {
      whiteboard_log_warning("SIB %s not found. Cannot join.\n",
			     udn);
      retval=FALSE;
    }
  else
    {
      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
			    "Node %s joining SIB %s\n", nodeid,udn);
      source = (AccessSIB*) list->data;
      access_sib_ref(source);

      if (!access_sib_get_uuid(source, &uuid) )
	{
	  whiteboard_log_error("Could not get uuid\n");
	  access_sib_unref(source);
	  retval=FALSE;
	  //return -1;
	}
      else
	{
	  conn = dbushandler_get_connection_by_uuid(context,
						    uuid);
	  if(uuid)
	    {
	      g_free(uuid);
	      uuid=NULL;
	    }
	  
	  if( NULL != conn)
	    {
	      // check that not already joined
	      if( ( FALSE == access_sib_is_node_joined(source, nodeid) ) &&
		  ( NULL == whiteboard_sib_handler_get_sib_by_joined_nodeid(sib_handler, nodeid) ) )
		{
		  join_id = whiteboard_sib_handler_get_access_id();
		  
		  dbushandler_set_node_connection_with_access_id( context,
								join_id,
								packet->connection);
		  dbushandler_set_sib_connection_with_access_id( context,
								  join_id,
								  conn);
		  
		  
		  whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
					      WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
					      WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_JOIN,
					      conn,
					      DBUS_TYPE_INT32, &join_id,
					      //apr09obsolete DBUS_TYPE_STRING, &username,
					      DBUS_TYPE_STRING, &nodeid,
					      DBUS_TYPE_STRING, &udn,
					      DBUS_TYPE_INT32, &msgnum,
					      WHITEBOARD_UTIL_LIST_END);
		  access_sib_add_to_joined_nodes(source, nodeid);
		  access_sib_unref(source);
		  
		  whiteboard_sib_handler_add_sib_by_joined_nodeid(sib_handler, nodeid, udn);

		  JoinData *jd = g_new0(JoinData,1);
		  jd->sib = g_strdup(udn);
		  jd->node = g_strdup(nodeid);
		  whiteboard_sib_handler_add_joindata_by_accessid(sib_handler, join_id, jd);
		  retval = TRUE;
		}
	      else
		{
		  whiteboard_log_warning("Node (%s) already joined\n", nodeid);
		  access_sib_unref(source);	  
		  retval = FALSE;
		}
	    }
	  else
	    {
	      whiteboard_log_error("Could not get dbus connection\n");
	      access_sib_unref(source);
	      //return -1;
	      retval = FALSE;
	    }
	}
    }
  whiteboard_log_debug("Sending join method return with value: %d\n", retval);
  
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &join_id,
				     WHITEBOARD_UTIL_LIST_END);
  whiteboard_log_debug_fb();
  return retval;
}

static gint whiteboard_sib_handler_handle_leave(DBusHandler *context,
						WhiteBoardPacket *packet,
						gpointer user_data)
{
  gint retval = -1;
  const gchar* udn = NULL;
  gchar* nodeid = NULL;
  gchar *uuid=NULL;
  gint msgnum=0;
  GList* list = NULL;
  AccessSIB *source = NULL;
  WhiteBoardSIBHandler* sib_handler = NULL;
  DBusConnection *conn = NULL;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;

  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_STRING, &nodeid,
				DBUS_TYPE_INT32, &msgnum,
				WHITEBOARD_UTIL_LIST_END);

  udn = whiteboard_sib_handler_get_sib_by_joined_nodeid(sib_handler, (const char*) nodeid);
  if(NULL == udn)
    {
      whiteboard_log_warning("Found no joined SIBs for node %s. Cannot leave.\n",
			     nodeid);
      retval = -1;
    }
  else
    {
      /* find the source from internal data structures */
      list = g_list_find_custom(sib_handler->sib_list, udn,
				access_sib_compare_id);
      
      
      if (list == NULL)
	{
	  whiteboard_log_warning("Node %s not found. Cannot leave.\n",
				 udn);
	  retval=-1;
	}
      else
	{
	  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				"Node (%s) leaving sib (%s) \n", nodeid, udn);
	  source = (AccessSIB*) list->data;
	  access_sib_ref(source);
	  
	  if (!access_sib_get_uuid(source, &uuid) )
	    {
	      whiteboard_log_error("Could not get uuid\n");
	      access_sib_unref(source);
	      retval=-1;
	      //return -1;
	    }
	  else
	    {
	      conn = dbushandler_get_connection_by_uuid(context,
							uuid);
	      if(uuid)
		{
		  g_free(uuid);
		  uuid=NULL;
		}
	      
	      if( NULL != conn)
		{
		  // check that not already joined
		  if( TRUE == access_sib_is_node_joined(source, nodeid) )
		    {
		      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
						  WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
						  WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_LEAVE,
						  conn,
						  DBUS_TYPE_STRING, &nodeid,
						  DBUS_TYPE_STRING, &udn,
						  DBUS_TYPE_INT32, &msgnum,
						  WHITEBOARD_UTIL_LIST_END);
		      access_sib_remove_from_joined_nodes(source, nodeid);
		      access_sib_unref(source);
		      
		      // remove association between nodeid and sib
		      whiteboard_sib_handler_remove_sib_by_joined_nodeid(sib_handler, nodeid);
		      
		      retval = 0;
		    }
		  else
		    {
		      whiteboard_log_warning("Node (%s) not joined\n", nodeid);
		      access_sib_unref(source);	  
		      retval = -1;
		    }
		}
	      else
		{
		  whiteboard_log_error("Could not get dbus connection\n");
		  access_sib_unref(source);
		  //return -1;
		  retval = -1;
		}
	    }
	}
    }
  whiteboard_log_debug("Sending leave method return with value: %d\n", retval);
  
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &retval,
				     WHITEBOARD_UTIL_LIST_END);
  whiteboard_log_debug_fb();
  return (retval == 0);
  
}

static gint whiteboard_sib_handler_handle_insert(DBusHandler *context,
						  WhiteBoardPacket *packet,
						  gpointer user_data)
{
  gint retval = -1;
  gchar *uuid = NULL;
  gchar* nodeid = NULL;
  gchar* sibid=NULL;
  gchar* insert_request = NULL;
  gchar *insert_response = NULL;
  gint response_success = -1;
  gboolean free_response = FALSE;
  DBusConnection* conn = NULL;
  GList *list = NULL;
  WhiteBoardSIBHandler* sib_handler=NULL;
  AccessSIB *source = NULL;
  DBusMessage *reply = NULL;
  gint msgnum=0;
  gint encoding =0;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );

  sib_handler = (WhiteBoardSIBHandler*) user_data;

  if( whiteboard_util_parse_message(packet->message,
				    DBUS_TYPE_STRING, &nodeid,
				    DBUS_TYPE_STRING, &sibid,
				    DBUS_TYPE_INT32, &msgnum,
				    DBUS_TYPE_INT32, &encoding,
				    DBUS_TYPE_STRING, &insert_request,
				    WHITEBOARD_UTIL_LIST_END) )
    {
      if(NULL == sibid)
	{
	  whiteboard_log_warning("Found no joined SIBs for node %s. Cannot insert.\n",
				 nodeid);
	  retval = FALSE;
	  insert_response = g_strdup("Fail");
	  response_success = -1;
	  free_response = TRUE;
	}
      else
	{
	  /* find the source from internal data structures */
	  list = g_list_find_custom(sib_handler->sib_list, sibid,
				    access_sib_compare_id);
	  
	  
	  if (list == NULL)
	    {
	      whiteboard_log_warning("SIB (%s) not found. Cannot insert triplets.\n",
				     sibid);
	      retval=FALSE;
	      insert_response = g_strdup("Fail");
	      response_success = -1;
	    }
	  else
	    {
	      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				    "Insert request from Node (%s), SIB (%s) \n", nodeid, sibid);
	      source = (AccessSIB*) list->data;
	      access_sib_ref(source);
	      
	      if (!access_sib_get_uuid(source, &uuid) )
		{
		  whiteboard_log_error("Could not get uuid\n");
		  retval=FALSE;
		  insert_response = g_strdup("Fail");
		  response_success = -1;
		  free_response = TRUE;
		  //return -1;
		}
	      else
		{
		  conn = dbushandler_get_connection_by_uuid(context,
							    uuid);
		  if(uuid)
		    {
		      g_free(uuid);
		      uuid=NULL;
		    }
		  
		  if( NULL != conn)
		    {
		      // check that not already joined
		      if( TRUE == access_sib_is_node_joined(source, nodeid) )
			{
			  whiteboard_util_send_method_with_reply(WHITEBOARD_DBUS_SERVICE,
								 WHITEBOARD_DBUS_OBJECT,
								 WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
								 WHITEBOARD_DBUS_SIB_ACCESS_METHOD_INSERT,
								 conn,
								 &reply,
								 DBUS_TYPE_STRING, &nodeid,
								 DBUS_TYPE_STRING, &sibid,
								 DBUS_TYPE_INT32, &msgnum,
								 DBUS_TYPE_INT32, &encoding,
								 DBUS_TYPE_STRING, &insert_request,
								 WHITEBOARD_UTIL_LIST_END);
			  
			  if(reply)
			    {
			      whiteboard_util_parse_message(reply,
							    DBUS_TYPE_INT32, &response_success,
							    DBUS_TYPE_STRING, &insert_response,
							    WHITEBOARD_UTIL_LIST_END);
			      
			      retval = TRUE;
			    }
			  else
			    {
			      whiteboard_log_warning("No insert reply, node %s\n", nodeid);
			      response_success = -1;
			      insert_response = g_strdup("Fail");
			      free_response = TRUE;
			      retval = FALSE;
			    }
			}
		      else
			{
			  whiteboard_log_warning("Node (%s) not joined\n", nodeid);
			  response_success = -1;
			  insert_response = g_strdup("Fail");
			  free_response = TRUE;
			  retval = FALSE;
			}
		    }
		  else
		    {
		      whiteboard_log_error("Could not get dbus connection\n");
		      //return -1;
		      response_success = -1;		  
		      insert_response = g_strdup("Fail");
		      free_response = TRUE;	      
		      retval = FALSE;
		    }
		}
	      
	      access_sib_unref(source);
	    }
	}
    }
  else
    {
      response_success = -1;		  
      insert_response = g_strdup("Fail");
      free_response = TRUE;	      
      retval = FALSE;  
    }
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &response_success,
				     DBUS_TYPE_STRING, &insert_response,
				     WHITEBOARD_UTIL_LIST_END);
  if(reply)
    dbus_message_unref(reply);
  
  if( free_response)
    {
      g_free(insert_response);
    }
  whiteboard_log_debug_fe();
  return retval;
}

static gint whiteboard_sib_handler_handle_update(DBusHandler *context,
						  WhiteBoardPacket *packet,
						  gpointer user_data)
{
  gint retval = -1;
  gchar *uuid = NULL;
  gchar* nodeid = NULL;
  gchar* sibid=NULL;
  gchar* insert_request = NULL;
  gchar* remove_request = NULL;
  gchar *update_response = NULL;
  gint response_success = -1;
  EncodingType encoding;
  gboolean free_response = FALSE;
  DBusConnection* conn = NULL;
  GList *list = NULL;
  WhiteBoardSIBHandler* sib_handler=NULL;
  AccessSIB *source = NULL;
  DBusMessage *reply = NULL;
  gint msgnum=0;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );

  sib_handler = (WhiteBoardSIBHandler*) user_data;
  
  if(whiteboard_util_parse_message(packet->message,
				   DBUS_TYPE_STRING, &nodeid,
				   DBUS_TYPE_STRING, &sibid,
				   DBUS_TYPE_INT32, &msgnum,
				   DBUS_TYPE_INT32, &encoding,
				   DBUS_TYPE_STRING, &insert_request,
				   DBUS_TYPE_STRING, &remove_request,				
				   WHITEBOARD_UTIL_LIST_END))
    {
      
      if(NULL == sibid)
	{
	  whiteboard_log_warning("Found no joined SIBs for node %s. Cannot update.\n",
				 nodeid);
	  update_response = g_strdup("Fail");
	  response_success = -1;
	  free_response = TRUE;
	}
      else
	{
	  /* find the source from internal data structures */
	  list = g_list_find_custom(sib_handler->sib_list, sibid,
				    access_sib_compare_id);
	  
	  
	  if (list == NULL)
	    {
	      whiteboard_log_warning("SIB (%s) not found. Cannot update triplets.\n",
				     sibid);
	      update_response = g_strdup("Fail");
	      response_success = -1;
	      free_response = TRUE;
	    }
	  else
	    {
	      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				    "Update request from Node (%s), SIB (%s) \n", nodeid, sibid);
	      source = (AccessSIB*) list->data;
	      access_sib_ref(source);
	      
	      if (!access_sib_get_uuid(source, &uuid) )
		{
		  whiteboard_log_error("Could not get uuid\n");
		  update_response = g_strdup("Fail");
		  response_success = -1;
		  free_response = TRUE;
		  //return -1;
		}
	      else
		{
		  conn = dbushandler_get_connection_by_uuid(context,
							    uuid);
		  if(uuid)
		    {
		      g_free(uuid);
		      uuid=NULL;
		    }
		  
		  if( NULL != conn)
		    {
		      // check that not already joined
		      if( TRUE == access_sib_is_node_joined(source, nodeid) )
			{
			  whiteboard_util_send_method_with_reply(WHITEBOARD_DBUS_SERVICE,
								 WHITEBOARD_DBUS_OBJECT,
								 WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
								 WHITEBOARD_DBUS_SIB_ACCESS_METHOD_UPDATE,
								 conn,
								 &reply,
								 DBUS_TYPE_STRING, &nodeid,
								 DBUS_TYPE_STRING, &sibid,
								 DBUS_TYPE_INT32, &msgnum,
								 DBUS_TYPE_INT32, &encoding,
								 DBUS_TYPE_STRING, &insert_request,
								 DBUS_TYPE_STRING, &remove_request,
								 WHITEBOARD_UTIL_LIST_END);
			  
			  if(reply)
			    {
			      whiteboard_util_parse_message(reply,
							    DBUS_TYPE_INT32, &response_success,
							    DBUS_TYPE_STRING, &update_response,
							    WHITEBOARD_UTIL_LIST_END);
			      
			      retval = 1;
			    }
			  else
			    {
			      whiteboard_log_warning("No reply update request\n");
			      response_success = -1;
			      update_response = g_strdup("Fail");
			      free_response = TRUE;
			      retval = FALSE;
			    }
			}
		      else
			{
			  whiteboard_log_warning("Node (%s) not joined\n", nodeid);
			  response_success = -1;
			  update_response = g_strdup("Fail");
			  free_response = TRUE;
			}
		    }
		  else
		    {
		      whiteboard_log_error("Could not get dbus connection\n");
		      //return -1;
		      response_success = -1;		  
		      update_response = g_strdup("Fail");
		      free_response = TRUE;	      
		    }
		}
	      access_sib_unref(source);
	    }
	}
    }
  else
    {
      whiteboard_log_error("Could not parse DBUS msg parameters\n");
      //return -1;
      response_success = -1;		  
      update_response = g_strdup("Fail");
      free_response = TRUE;	      
    }
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &response_success,
				     DBUS_TYPE_STRING, &update_response,
				     WHITEBOARD_UTIL_LIST_END);
  if(reply)
     dbus_message_unref(reply);
  
  if( free_response)
    {
      g_free(update_response);
    }
  whiteboard_log_debug_fe();
  return retval;
}

static gint whiteboard_sib_handler_handle_remove(DBusHandler *context,
						 WhiteBoardPacket *packet,
						 gpointer user_data)
{
  const gchar* member = NULL;

  gint retval = -1;
  gchar *uuid = NULL;
  gchar* nodeid = NULL;
  gchar* sibid=NULL;
  gchar* insert_request = NULL;
  gchar*  response = NULL;
  gboolean free_response = FALSE;
  gint response_success = -1;

  EncodingType encoding;
  DBusConnection* conn = NULL;
  GList *list = NULL;
  WhiteBoardSIBHandler* sib_handler=NULL;
  AccessSIB *source = NULL;
  DBusMessage *reply = NULL;
  gint msgnum=0;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );

  member = dbus_message_get_member(packet->message);
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;
  
  if(whiteboard_util_parse_message(packet->message,
				   DBUS_TYPE_STRING, &nodeid,
				   DBUS_TYPE_STRING, &sibid,
				   DBUS_TYPE_INT32, &msgnum,
				   DBUS_TYPE_INT32, &encoding,
				   DBUS_TYPE_STRING, &insert_request,
				   WHITEBOARD_UTIL_LIST_END) )
    {
      whiteboard_log_debug("Remove: nodeid:%s, sibid :%s, msgnum: %d, encoding: %d, request :%s\n", nodeid, sibid, msgnum, encoding, insert_request);
      if(NULL == sibid)
	{
	  whiteboard_log_warning("Found no joined SIBs for node %s. Cannot remove.\n",
				 nodeid);
	  response = g_strdup("Fail");
	  response_success = -1;
	  free_response = TRUE;

	}
      else
	{
	  /* find the source from internal data structures */
	  list = g_list_find_custom(sib_handler->sib_list, sibid,
				    access_sib_compare_id);
	  
	  
	  if (list == NULL)
	    {
	      whiteboard_log_warning("SIB (%s) not found. Cannot %s triplets.\n",member,
				     sibid);
	      response = g_strdup("Fail");
	      response_success = -1;
	      free_response = TRUE;
	      
	    }
	  else
	    {
	      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				    "%s request from Node (%s), SIB (%s) \n",member, nodeid, sibid);
	      source = (AccessSIB*) list->data;
	      access_sib_ref(source);
	      
	      if (!access_sib_get_uuid(source, &uuid) )
		{
		  whiteboard_log_error("Could not get uuid\n");
		  response = g_strdup("Fail");
		  response_success = -1;
		  free_response = TRUE;
		  //return -1;
		}
	      else
		{
		  conn = dbushandler_get_connection_by_uuid(context,
							    uuid);
		  if(uuid)
		    {
		      g_free(uuid);
		      uuid=NULL;
		    }
		  
		  if( NULL != conn)
		    {
		      // check that not already joined
		      if( TRUE == access_sib_is_node_joined(source, nodeid) )
			{
			  whiteboard_util_send_method_with_reply(WHITEBOARD_DBUS_SERVICE,
								 WHITEBOARD_DBUS_OBJECT,
								 WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
								 member,
								 conn,
								 &reply,
								 DBUS_TYPE_STRING, &nodeid,
								 DBUS_TYPE_STRING, &sibid,
								 DBUS_TYPE_INT32, &msgnum,
								 DBUS_TYPE_INT32, &encoding,
								 DBUS_TYPE_STRING, &insert_request,
								 WHITEBOARD_UTIL_LIST_END);
			  
			  if(reply &&  whiteboard_util_parse_message(reply,
								     DBUS_TYPE_INT32, &response_success,
								     DBUS_TYPE_STRING, &response,
								     WHITEBOARD_UTIL_LIST_END) )
			    {
			      retval = 1;
			    }
			  else
			    {
			      whiteboard_log_warning("No reply of could not parse message\n");
			      response = g_strdup("Fail");
			      response_success = -1;
			      free_response = TRUE;
			    }
			}
		      else
			{
			  whiteboard_log_warning("Node (%s) not joined\n", nodeid);
			  response = g_strdup("Fail");
			  response_success = -1;
			  free_response = TRUE;
			}
		    }
		  else
		    {
		      whiteboard_log_error("Could not get dbus connection\n");
		      response = g_strdup("Fail");
		      response_success = -1;
		      free_response = TRUE;
		      //return -1;
		    }
		}
	      access_sib_unref(source);
	    }
	}
    }
  else
    {
      response = g_strdup("Fail");
      free_response = TRUE;
    }
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &response_success,
				     DBUS_TYPE_STRING, &response,
				     WHITEBOARD_UTIL_LIST_END);
  
  if(reply)
    dbus_message_unref(reply);
  
  if(free_response)
    g_free(response);
  
  whiteboard_log_debug_fe();
  return retval;
}

static gint whiteboard_sib_handler_handle_subscribe_query(DBusHandler *context,
							  WhiteBoardPacket *packet,
							  gpointer user_data)
{
  gint retval = -1;
  gint access_id = -1;
  gchar *uuid = NULL;
  gchar* nodeid = NULL;
  gchar* sibid=NULL;
  gint type = -1;
  gchar *request = NULL;
  DBusConnection* conn = NULL;
  GList *list = NULL;
  WhiteBoardSIBHandler* sib_handler=NULL;
  AccessSIB *source = NULL;
  const gchar *member = NULL;
  gint msgnum=0;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );

  sib_handler = (WhiteBoardSIBHandler*) user_data;
  member = dbus_message_get_member(packet->message);
  if( whiteboard_util_parse_message(packet->message,
				    DBUS_TYPE_STRING, &nodeid,
				    DBUS_TYPE_STRING, &sibid,
				    DBUS_TYPE_INT32, &msgnum,
				    DBUS_TYPE_INT32, &type,
				    DBUS_TYPE_STRING, &request,
				    WHITEBOARD_UTIL_LIST_END) )
    {
  
      if(NULL == sibid)
	{
	  whiteboard_log_warning("Found no joined SIBs for node %s. Cannot %s.\n",
				 nodeid, member);
	  retval = FALSE;
	}
      else
	{
	  /* find the source from internal data structures */
	  list = g_list_find_custom(sib_handler->sib_list, sibid,
				    access_sib_compare_id);
	  
	  
	  if (list == NULL)
	    {
	      whiteboard_log_warning("SIB (%s) not found. Cannot subscribe.\n",
				     sibid);
	      retval=FALSE;
	    }
	  else
	    {
	      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				    "%s request from Node (%s), SIB (%s) \n", member,nodeid, sibid);
	      source = (AccessSIB*) list->data;
	      access_sib_ref(source);
	      
	      if (!access_sib_get_uuid(source, &uuid) )
		{
		  whiteboard_log_error("Could not get uuid\n");
		  retval=FALSE;
		  //return -1;
		}
	      else
		{
		  conn = dbushandler_get_connection_by_uuid(context,
							    uuid);
		  if(uuid)
		    {
		      g_free(uuid);
		      uuid=NULL;
		    }
	      
		  if( NULL != conn)
		    {
		      // check that joined
		      if( TRUE == access_sib_is_node_joined(source, nodeid) )
			{
			  access_id = whiteboard_sib_handler_get_access_id();
			  dbushandler_set_node_connection_with_access_id( context,
									  access_id,
									  packet->connection);
			  dbushandler_set_sib_connection_with_access_id( context,
									 access_id,
									 conn);
		      
			  whiteboard_util_send_method(WHITEBOARD_DBUS_SERVICE,
						      WHITEBOARD_DBUS_OBJECT,
						      WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
						      member,
						      conn,
						      DBUS_TYPE_INT32, &access_id,
						      DBUS_TYPE_STRING, &nodeid,
						      DBUS_TYPE_STRING, &sibid,
						      DBUS_TYPE_INT32, &msgnum,
						      DBUS_TYPE_INT32, &type,
						      DBUS_TYPE_STRING, &request,
						      WHITEBOARD_UTIL_LIST_END);
			}
		      else
			{
			  whiteboard_log_warning("Node (%s) not joined\n", nodeid);
			  retval = FALSE;
			}
		    }
		  else
		    {
		      whiteboard_log_error("Could not get dbus connection\n");
		      //return -1;
		      retval = FALSE;
		    }
		}
	      access_sib_unref(source);
	    }
	}
    }
  whiteboard_util_send_method_return(packet->connection, packet->message,
				     DBUS_TYPE_INT32, &access_id,
				     WHITEBOARD_UTIL_LIST_END);
  whiteboard_log_debug_fe();
  return retval;
}


static gint whiteboard_sib_handler_handle_unsubscribe(DBusHandler *context,
						      WhiteBoardPacket *packet,
						      gpointer user_data)
{
  
  gint retval = -1;
  gchar *uuid = NULL;
  gchar* nodeid = NULL;
  gchar* sibid=NULL;
  gchar *subscription_id = NULL;
  gint access_id = -1;
  DBusConnection* conn = NULL;
  GList *list = NULL;
  WhiteBoardSIBHandler* sib_handler=NULL;
  AccessSIB *source = NULL;
  gint msgnum=0;
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != user_data, -1 );

  sib_handler = (WhiteBoardSIBHandler*) user_data;
  
  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_INT32, &access_id,
				DBUS_TYPE_STRING, &nodeid,
				DBUS_TYPE_STRING, &sibid,
				DBUS_TYPE_INT32, &msgnum,
				DBUS_TYPE_STRING, &subscription_id,
				WHITEBOARD_UTIL_LIST_END);
  
  if(NULL == sibid)
    {
      whiteboard_log_warning("Found no joined SIBs for node %s. Cannot unsubscribe.\n",
			     nodeid);
      retval = -1;
    }
  else
    {
      /* find the source from internal data structures */
      list = g_list_find_custom(sib_handler->sib_list, sibid,
				access_sib_compare_id);
      
      
      if (list == NULL)
	{
	  whiteboard_log_warning("SIB (%s) not found. Cannot unscubscribe.\n",
				 sibid);
	  retval=-1;
	}
      else
	{
	  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER,
				"Unsubsribe request from Node (%s), SIB (%s) \n", nodeid, sibid);
	  source = (AccessSIB*) list->data;
	  access_sib_ref(source);
	  
	  if (!access_sib_get_uuid(source, &uuid) )
	    {
	      whiteboard_log_error("Could not get uuid\n");
	      retval=-1;
	      //return -1;
	    }
	  else
	    {
	      conn = dbushandler_get_connection_by_uuid(context,
							uuid);
	      if(uuid)
		{
		  g_free(uuid);
		  uuid=NULL;
		}
	      
	      if( NULL != conn)
		{
		  // check that joined
		  if( TRUE == access_sib_is_node_joined(source, nodeid) )
		    {
		      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
						  WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE,
						  WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_UNSUBSCRIBE,
						  conn,
						  DBUS_TYPE_INT32, &access_id,
						  DBUS_TYPE_STRING, &nodeid,
						  DBUS_TYPE_STRING, &sibid,
						  DBUS_TYPE_INT32, &msgnum,
						  DBUS_TYPE_STRING, &subscription_id,
						  WHITEBOARD_UTIL_LIST_END);
		      retval = 1;
		    }
		  else
		    {
		      retval = -1;
		    }
		}
	      else
		{
		  whiteboard_log_error("Could not get dbus connection\n");
		  retval = -1;
		}
	    }
	  access_sib_unref(source);
	}
    }
  if(retval < 0)
    {
      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
				  WHITEBOARD_DBUS_NODE_INTERFACE,
				  WHITEBOARD_DBUS_NODE_SIGNAL_UNSUBSCRIBE_COMPLETE,
				  packet->connection,
				  DBUS_TYPE_INT32, &access_id,
				  DBUS_TYPE_STRING, &nodeid,
				  DBUS_TYPE_STRING, &sibid,
				  DBUS_TYPE_INT32, &retval,
				  DBUS_TYPE_STRING, &subscription_id,
				  WHITEBOARD_UTIL_LIST_END);
    }
  whiteboard_log_debug_fe();
  return retval;
}

static gint whiteboard_sib_handler_handle_signal_join_complete(DBusHandler *context,
							       WhiteBoardPacket *packet,
							       gpointer user_data)
{
  WhiteBoardSIBHandler *self = (WhiteBoardSIBHandler *)user_data;
  dbus_int32_t join_id;
  dbus_int32_t status;
  DBusConnection *node_connection;

  whiteboard_log_debug_fb();	
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );
  g_return_val_if_fail( NULL != self, -1);
  
  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_INT32, &join_id,
				DBUS_TYPE_INT32, &status,
				WHITEBOARD_UTIL_LIST_END);
	


	

  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			"Got signal (join complete) with access id: %d,status %d\n", 
			join_id, status);


	
  /* Find the connection associated to this access id */
  node_connection = dbushandler_get_node_connection_by_access_id(context, 
							     join_id);

  whiteboard_util_forward_packet(node_connection, packet->message,
				 NULL,
				 NULL,
				 WHITEBOARD_DBUS_NODE_INTERFACE,
				 NULL);
  /* Then remove the connection associated to this access id 
     from data structures */
  dbushandler_invalidate_access_id(context,join_id);

  JoinData *jd=whiteboard_sib_handler_get_joindata_by_accessid(self,join_id);
  if(jd)
    {
	    
      whiteboard_sib_handler_remove_joindata_by_accessid( self, join_id);
    }
  else
    {
      whiteboard_log_debug("Could not find JoinData w/ accessid:%d\n", join_id);
    }
	
  if(status && jd)
    {
      whiteboard_sib_handler_remove_sib_by_joined_nodeid( self, jd->node);

            /* find the source from internal data structures */
      GList *list = g_list_find_custom(self->sib_list, jd->sib,
				       access_sib_compare_id);
      
      
      if (list != NULL)
	{
	  AccessSIB *source = (AccessSIB*) list->data;
	  access_sib_ref(source);
	  
	  access_sib_remove_from_joined_nodes(source, jd->node);
	  access_sib_unref(source);
	}
      else
	{
	  whiteboard_log_debug("Can not find sib: %s, from sib_list\n", jd->sib); 
	}
    }
  whiteboard_sib_handler_remove_joindata_by_accessid(self,join_id);
  if(jd)
    {
      g_free(jd->node);
      g_free(jd->sib);
      g_free(jd);
    }
  whiteboard_log_debug_fe();
  return 0;

}

static gint whiteboard_sib_handler_handle_unsubscribe_complete(DBusHandler *context,
							       WhiteBoardPacket *packet,
							       gpointer user_data)
{

  dbus_int32_t access_id = -1;
  DBusConnection *node_connection;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );

  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_INT32, &access_id,
				WHITEBOARD_UTIL_LIST_END);
  
  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			"Got signal (unsubscribe complete) with access_id:%d. \n", access_id);
  
  /* Find the connection associated to this access id */
  node_connection = dbushandler_get_node_connection_by_access_id(context, 
								access_id);
  
  whiteboard_util_forward_packet(node_connection, packet->message,
				 NULL,
				 NULL,
				 WHITEBOARD_DBUS_NODE_INTERFACE,
				 NULL);
  
  dbushandler_invalidate_access_id(context, access_id);
  
  whiteboard_log_debug_fe();
  return 0;
}

static gint whiteboard_sib_handler_handle_signal_subscription_ind(DBusHandler *context,
								  WhiteBoardPacket *packet,
								  gpointer user_data)
{
  gint access_id;
  DBusConnection *node_connection;
	
  whiteboard_log_debug_fb();
	
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );

  whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_INT32, &access_id,
				WHITEBOARD_UTIL_LIST_END);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			"Got signal (subscription_ind) with access_id: %d\n", 
			access_id);

  /* Find the connection associated to this access id */
  node_connection = dbushandler_get_node_connection_by_access_id(context, access_id);

  whiteboard_util_forward_packet(node_connection, packet->message,
				 NULL,
				 NULL,
				 WHITEBOARD_DBUS_NODE_INTERFACE,
				 NULL);
  whiteboard_log_debug_fe();
  return 0;
}

static gint whiteboard_sib_handler_handle_subscribe_return(DBusHandler *context,
							   WhiteBoardPacket *packet,
							   gpointer user_data)
{
  gchar *subscription_id=NULL;;
  gchar *results = NULL;
  gint access_id = -1;
  gint status = -1;
  DBusConnection *node_connection;
	
  whiteboard_log_debug_fb();
	
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );

  if(whiteboard_util_parse_message(packet->message,
				   DBUS_TYPE_INT32, &access_id,
				   DBUS_TYPE_INT32, &status,
				   DBUS_TYPE_STRING, &subscription_id,
				   DBUS_TYPE_STRING, &results,
				   WHITEBOARD_UTIL_LIST_END))
    {

      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			    "Got subscribe return with access_id: %d, status:%d, subscription_id: %s, results: %s\n",
			    access_id,
			    status,
			    subscription_id,
			    results);
      
      /* Find the connection associated to this access id */
      node_connection = dbushandler_get_node_connection_by_access_id(context, access_id);
      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
				  WHITEBOARD_DBUS_NODE_INTERFACE,
				  WHITEBOARD_DBUS_NODE_METHOD_SUBSCRIBE,
				  node_connection,
				  DBUS_TYPE_INT32, &access_id,
				  DBUS_TYPE_INT32, &status,
				  DBUS_TYPE_STRING, &subscription_id,
				  DBUS_TYPE_STRING, &results,
				  WHITEBOARD_UTIL_LIST_END);
    }
  whiteboard_log_debug_fe();
  return 0;
}

static gint whiteboard_sib_handler_handle_query_return(DBusHandler *context,
							   WhiteBoardPacket *packet,
							   gpointer user_data)
{
  gchar *results = NULL;
  gint access_id = -1;
  gint status = -1;
  DBusConnection *node_connection;
	
  whiteboard_log_debug_fb();
	
  g_return_val_if_fail( NULL != context, -1 );
  g_return_val_if_fail( NULL != packet, -1 );

  if( whiteboard_util_parse_message(packet->message,
				DBUS_TYPE_INT32, &access_id,
				DBUS_TYPE_INT32, &status,
				DBUS_TYPE_STRING, &results,
				    WHITEBOARD_UTIL_LIST_END))
    {

      whiteboard_log_debugc(WHITEBOARD_DEBUG_SIB_HANDLER, 
			    "Got query return with access_id: %d, status:%d, results: %s\n",
			    access_id,
			    status,
			    results);
      
      /* Find the connection associated to this access id */
      node_connection = dbushandler_get_node_connection_by_access_id(context, access_id);
      
      whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
				  WHITEBOARD_DBUS_NODE_INTERFACE,
				  WHITEBOARD_DBUS_NODE_METHOD_QUERY,
				  node_connection,
				  DBUS_TYPE_INT32, &access_id,
				  DBUS_TYPE_INT32, &status,
				  DBUS_TYPE_STRING, &results,
				  WHITEBOARD_UTIL_LIST_END);

      dbushandler_invalidate_access_id(context, access_id);
    }
  whiteboard_log_debug_fe();
  
  return 0;
}

#if 0
static gint whiteboard_sib_handler_handle_insert_response(DBusHandler *context,
							  WhiteBoardPacket *packet,
							  gpointer user_data)
{
  DBusConnection* connection = NULL;
  gchar* destination = NULL;
  
  whiteboard_log_debug_fb();
  
  destination = (gchar*) dbus_message_get_destination(packet->message);
  
  whiteboard_log_debug("Getting message destination (%s)\n", destination);
  connection = dbushandler_get_connection_by_uuid(context, destination);
  
  if (connection == NULL)
    {
      whiteboard_log_debug("Could not get target NODE connection\n");
    }
  else
    {
      whiteboard_util_forward_packet(connection, packet->message,
				     NULL,
				     NULL,
				     WHITEBOARD_DBUS_NODE_INTERFACE,
				     NULL);
    }
  whiteboard_log_debug_fe();
  return 0;
}
#endif
static void whiteboard_sib_handler_dbus_cb(DBusHandler *context, WhiteBoardPacket *packet,
					   gpointer user_data)
{
  const gchar* interface = NULL;
  const gchar* member = NULL;
  gint type = 0;
  
  whiteboard_log_debug_fb();
  
  g_return_if_fail( NULL != context );
  g_return_if_fail( NULL != packet );
  
  interface = dbus_message_get_interface(packet->message);
  member = dbus_message_get_member(packet->message);
  type = dbus_message_get_type(packet->message);
  
  switch (type)
    {
    case DBUS_MESSAGE_TYPE_SIGNAL:
      
      if (!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_SIB_REMOVED))
	{
	  whiteboard_log_debug("Got node removed\n");
	  
	  whiteboard_sib_handler_handle_signal_sib_removed(context, 
							    packet,
							    user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_JOIN_COMPLETE))
	{
	  whiteboard_log_debug("Got join complete\n");
	  
	  whiteboard_sib_handler_handle_signal_join_complete(context, 
							     packet,
							     user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_SUBSCRIPTION_IND))
	{
	  whiteboard_log_debug("Got subscription ind\n");
	  
	  whiteboard_sib_handler_handle_signal_subscription_ind(context, 
								packet,
								user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_NODE_SIGNAL_UNSUBSCRIBE))
	{
	  whiteboard_log_debug("Got unsubscribe req\n");
	   
	  whiteboard_sib_handler_handle_unsubscribe(context, 
						    packet,
						    user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_SIGNAL_UNSUBSCRIBE_COMPLETE))
	{
	  whiteboard_log_debug("Got unsubscribe complete\n");
	   
	  whiteboard_sib_handler_handle_unsubscribe_complete(context, 
							     packet,
							     user_data);
	}
      else
	{
	  whiteboard_log_warning("Unknown sib_handler signal: %s %s\n",
				 interface, member);
	}
      
      break;
      
    case DBUS_MESSAGE_TYPE_METHOD_CALL:

      if(!strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_REFRESH_NODE))
	{
	  whiteboard_log_debug("Got refresh node call\n");
		    
	  whiteboard_sib_handler_handle_method_refresh_node(context,
							    packet,
							    user_data);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_JOIN))
	{
	  whiteboard_log_debug("Got join method\n");
	  
	  whiteboard_sib_handler_handle_join(context, 
					     packet,
					     user_data);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_LEAVE))
	{
	  whiteboard_log_debug("Got leave method\n");
	  
	  whiteboard_sib_handler_handle_leave(context, 
					      packet,
					      user_data);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_DISCOVERY_METHOD_GET_SIBS))
	{
	  whiteboard_log_debug("Got get sibs call\n");
		    
	  whiteboard_sib_handler_handle_method_get_sibs(context,
							packet,
							user_data);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_GET_DESCRIPTION))
	{	
	  whiteboard_log_debug("Got get description call\n");
		    
	  whiteboard_sib_handler_handle_method_get_description(context,
							       packet,
							       user_data);
	}
      else if( !strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_INSERT))
	{	
	  whiteboard_log_debug("Got INSERT method call\n");
	  whiteboard_sib_handler_handle_insert(context,
						packet,
						user_data);
	}
      else if( !strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_UPDATE))
	{	
	  whiteboard_log_debug("Got UPDATE method call\n");
	  whiteboard_sib_handler_handle_update(context,
						packet,
						user_data);
	}
      else if( !strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_SUBSCRIBE) ||
	       !strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_QUERY) )
	
	{	
	  whiteboard_log_debug("Got subscribe/query method call\n");
	  whiteboard_sib_handler_handle_subscribe_query(context,
						  packet,
						  user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_NODE_METHOD_REMOVE))
	{	
	  whiteboard_log_debug("Got remove method call\n");
	  whiteboard_sib_handler_handle_remove(context,
					       packet,
					       user_data);
	}
      else
	{
	  whiteboard_log_warning("Unknown method call: %s %s\n",
				 interface, member);
	}
		
      break;
		
    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      if(!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_METHOD_SUBSCRIBE))
	{	
	  whiteboard_log_debug("Got subscribe method return\n");
	  whiteboard_sib_handler_handle_subscribe_return(context,
							 packet,
							 user_data);
	}
      else if(!strcmp(member, WHITEBOARD_DBUS_SIB_ACCESS_METHOD_QUERY))
	{	
	  whiteboard_log_debug("Got query method return\n");
	  whiteboard_sib_handler_handle_query_return(context,
							 packet,
							 user_data);
	}
      else
	{
	  whiteboard_log_warning("Unknown method return: %s %s\n",
				 interface, member);
	}
      
      break;
      
    default:
      whiteboard_log_warning("Unknown message type: %d for %s %s\n",
			     type, interface, member);
      break;
      
    }
  
  whiteboard_log_debug_fe();
}

void whiteboard_sib_handler_sib_registered_cb(DBusHandler* context,
					      gchar* uuid,
					      gchar* name,
					      gpointer user_data)
{
  WhiteBoardSIBHandler* sib_handler = NULL;

  whiteboard_log_debug_fb();

  sib_handler = (WhiteBoardSIBHandler*) user_data;
  g_return_if_fail(sib_handler != NULL);

  g_return_if_fail(NULL != uuid);
  g_return_if_fail(NULL != name);

  whiteboard_sib_handler_add_sib(sib_handler, uuid, name);

  whiteboard_log_debug_fe();
}

static void whiteboard_sib_handler_node_disconnected_cb(DBusHandler* context, 
							gchar* uuid,
							gpointer user_data)
{
  WhiteBoardSIBHandler* sib_handler = NULL;
  GList *link = NULL;
  const gchar *sib = NULL;
  AccessSIB *sibdata;
  whiteboard_log_debug_fb();
  
  sib_handler = (WhiteBoardSIBHandler*) user_data;
  g_return_if_fail(sib_handler != NULL);
  
  g_return_if_fail(NULL != uuid);

  sib = whiteboard_sib_handler_get_sib_by_joined_nodeid( sib_handler, uuid);
  if(sib)
    {
      
      link = g_list_find_custom(sib_handler->sib_list, sib, 
				access_sib_compare_id);
      if(link)
	{
	  sibdata = (AccessSIB*) link->data;
	  access_sib_ref(sibdata);
	  access_sib_remove_from_joined_nodes(sibdata, uuid);
	  access_sib_unref(sibdata);
	}
    }
  
  if( whiteboard_sib_handler_remove_sib_by_joined_nodeid(sib_handler,
							uuid) )
    {
      whiteboard_log_debug("Removed %s from joined nodes\n", uuid);
    }
  whiteboard_log_debug_fe();
}

/*****************************************************************************
 * Connection lists by access ID
 *****************************************************************************/

static const gchar *whiteboard_sib_handler_get_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
								    const gchar *nodeid)
{
  const gchar *sib_uri = NULL;
  
  whiteboard_log_debug_fb();
  
  sib_uri = ( const gchar *) g_hash_table_lookup(self->joined_nodes_map,
						 nodeid);
  
  whiteboard_log_debug_fe();
  
  return sib_uri;
}

static void whiteboard_sib_handler_add_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
							    gchar *node,
							    gchar *sib)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug("Adding node (%s) as joined for SIB (%s)%d\n", node, sib);
  g_hash_table_insert(self->joined_nodes_map,
		      g_strdup(node), g_strdup(sib));
  whiteboard_log_debug_fe();
}

static gboolean whiteboard_sib_handler_remove_sib_by_joined_nodeid(WhiteBoardSIBHandler *self,
								   const gchar *nodeid)
{

  const gchar *sib_uri = NULL;
  gboolean retval = FALSE;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail(NULL != self, FALSE);
  g_return_val_if_fail(NULL != nodeid, FALSE);

  sib_uri = whiteboard_sib_handler_get_sib_by_joined_nodeid(self, nodeid);
  if (sib_uri != NULL)
    {
      /* Remove the sib from the hash map */
      whiteboard_log_debug("Removing:%s, sib:%s Map size (before remove):%d \n",
			   nodeid, sib_uri,   g_hash_table_size(self->joined_nodes_map));
	    
      retval = g_hash_table_remove(self->joined_nodes_map, nodeid);
      whiteboard_log_debug("Remove: %s, Map size (after remove): %d\n",
			   (retval == TRUE? "OK":"Fail"),
			   g_hash_table_size(self->joined_nodes_map)  );
      
      // dbus_connection_unref(conn);
    }

  whiteboard_log_debug_fe();
  return retval;
}



static void whiteboard_sib_handler_add_joindata_by_accessid(WhiteBoardSIBHandler *self,
						  gint accessid,
						  JoinData *jd)
{
  whiteboard_log_debug_fb();

  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != jd);

  g_hash_table_insert(self->joindata_map, GINT_TO_POINTER(accessid), (gpointer)jd);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Insert joindata: %d, data %p. Map size: %d\n",
			accessid, jd, g_hash_table_size(self->joindata_map));

  whiteboard_log_debug_fe();
}

static JoinData *whiteboard_sib_handler_get_joindata_by_accessid(WhiteBoardSIBHandler *self,
								 gint accessid)

{
  JoinData *jd=NULL;
  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, NULL);
  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Trying to get joindata w/ accessid: %d, Map size: %d\n", 
			accessid, g_hash_table_size(self->joindata_map));
	
  jd= (JoinData*) g_hash_table_lookup(self->joindata_map, GINT_TO_POINTER(accessid));
  whiteboard_log_debug_fe();
  return jd;
}

static gboolean whiteboard_sib_handler_remove_joindata_by_accessid(WhiteBoardSIBHandler* self, gint accessid)
{
  JoinData * jd = NULL;
  gboolean retval = FALSE;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, FALSE);

  jd = whiteboard_sib_handler_get_joindata_by_accessid(self, accessid);
  if (jd != NULL)
    {
      /* Remove the connection from the hash map */
      retval = g_hash_table_remove(self->joindata_map, GINT_TO_POINTER(accessid));

      whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			    "Removed:%d, conn:%p, ok:%s. Map size:%d\n",
			    accessid, jd, (retval) ? "TRUE" : "FALSE",
			    g_hash_table_size(self->joindata_map));
		
      // dbus_connection_unref(conn);
    }

  whiteboard_log_debug_fe();

  return retval;
}



/* Keep this preprocessor instruction always at the end of the file */
#endif /* UNIT_TEST_INCLUDE_IMPLEMENTATION */
