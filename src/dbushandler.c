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
 * WhiteBoard daemon
 *
 * dbushandler.c
 *
 * Copyright 2007 Nokia Corporation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib-bindings.h>

#include <whiteboard_dbus_ifaces.h>
#include <whiteboard_command.h>
#include <whiteboard_util.h>

#include "whiteboard_sib_handler.h"
#include "dbushandler.h"
//#include "dbushandler_marshal.h"
#include "whiteboard_log.h"

#if 0
static GType server_object_get_type();

#define OBJECT_TYPE_SERVER server_object_get_type()

#define SERVER_OBJECT(object) G_TYPE_CHECK_INSTANCE_CAST(object,	\
							 OBJECT_TYPE_SERVER, \
							 ServerObject)

#define SERVER_OBJECT_CLASS(object) G_TYPE_CHECK_CLASS_CAST(object,	\
							    OBJECT_TYPE_SERVER, \
							    ServerObjectClass)
#define SERVER_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), OBJECT_TYPE_SERVER, ServerObjectClass))

#define IS_SERVER_OBJECT(object) G_TYPE_CHECK_INSTANCE_TYPE(object,	\
							    OBJECT_TYPE_SERVER)

#define IS_SERVER_OBJECT_CLASS(object) G_TYPE_CHECK_CLASS_TYPE(object, \
							       OBJECT_TYPE_SERVER



/******************************************************************************
 * Type definitions
 ******************************************************************************/

struct _ServerObject;
typedef struct _ServerObject ServerObject;

struct _ServerObjectClass;
typedef struct _ServerObjectClass ServerObjectClass;

typedef void (*StartingCB) (ServerObject* source,
			 gpointer user_data);

/* Standard GObject class structures, etc */
struct _ServerObjectClass
{
  GObjectClass parent;
  
  StartingCB starting_cb;
  
  DBusGConnection *connection;
};

struct _ServerObject
{
  GObject parent;

  gchar *local_address;
};

enum
  {
    SIGNAL_STARTING,
	
    NUM_SIGNALS
  };

static guint server_signals[NUM_SIGNALS];


gboolean discover_discover (ServerObject *server, gchar *uuid, gchar **address, GError **error);
gboolean register_node (ServerObject *server, gchar *uuid, gint *success, GError **error);
gboolean register_sib (ServerObject *server, gchar *uuid, gint *success, GError **error);
gboolean register_control (ServerObject *server, gchar *uuid, gint *success, GError **error);

static void server_object_class_init(ServerObjectClass *klass);
//static void server_object_init(GTypeInstance* instance, gpointer g_class);
static ServerObject *server_object_new(const gchar *path);
static gint server_register(ServerObject *self);

#include "server-bindings.h"
#define INTROSPECT_RSP "<node> \n \
 <interface name=\"com.nokia.whiteboard\">\n	\
   <method name=\"discover\">\n \
      <arg name=\"uuid\" type=\"s\" direction=\"in\"/>\n \
      <arg name=\"path\" type=\"s\" direction=\"out\"/>\n \
      </method>\n \
  </interface>\n \
 <interface name=\"com.nokia.whiteboard.register\">\n \
   <method name=\"node\">\n \
      <arg name=\"uuid\" type=\"s\" direction=\"in\"/>\n \
      </method>\n \
   <method name=\"sib\">\n \
      <arg name=\"uuid\" type=\"s\" direction=\"in\"/>\n \
      </method>\n \
   <method name=\"control\">\n \
      <arg name=\"uuid\" type=\"s\" direction=\"in\"/>\n \
      </method>\n \
  </interface>\n \
</node> \n\
"
#endif
struct _DBusHandler
{
  GList *node_connections;
  GList *control_connections;
  GList *sib_connections;
  GList *discovery_connections;
  
  /* UUID -> dbus connection */
  GHashTable *connection_map;

  /* access id -> node connection */
  GHashTable *access_node_map;
  /* access id -> sib connection */
  GHashTable *access_sib_map;

  /* subscription id -> ui connection */
  GHashTable *subscription_map;
  
  GMainLoop *loop;
  DBusConnection *session_bus;
  gchar *local_address;

  WhiteBoardCommonPacketCB sib_handler_cb;
  WhiteBoardSIBRegisteredCB sib_registered_cb;
  WhiteBoardNodeDisconnectedCB node_disconnected_cb;
  gpointer user_data_sib_handler;
  gpointer user_data_sib_registered;
  gpointer user_data_node_disconnected;
};

typedef struct _rmData
{
  gpointer value;
  gpointer key;
} rmData;
  
/* Keep this preprocessor instruction always AFTER struct definitions
   and BEFORE any function declaration/prototype */
#ifndef UNIT_TEST_INCLUDE_IMPLEMENTATION

/* Private function prototypes */

static int dbushandler_initialize(DBusHandler* self);

static void dbushandler_handle_connection(DBusServer* server,
					  DBusConnection* conn,
					  gpointer data);

static DBusHandlerResult dbushandler_handle_message(DBusConnection* conn,
						    DBusMessage* msg,
						    gpointer data);
static void dbushandler_unregister_handler(DBusConnection* conn,gpointer data);

static void dbushandler_custom_command_request(DBusHandler* self,
					       DBusMessage* msg);

static void dbushandler_custom_command_response(DBusHandler* self,
						DBusMessage* msg);

static int dbushandler_register_control(DBusHandler *self, DBusConnection *conn,
					DBusMessage *msg);

static int dbushandler_register_node(DBusHandler *self, DBusConnection *conn,
				   DBusMessage *msg);

static int dbushandler_unregister_node(DBusHandler *self, DBusConnection *conn,
				       DBusMessage *msg);

static int dbushandler_register_sib(DBusHandler *self, DBusConnection *conn,
				     DBusMessage *msg);

static int dbushandler_register_discovery(DBusHandler *self,
					  DBusConnection *conn,
					  DBusMessage *msg);

static void dbushandler_handle_disconnect( DBusHandler *self, DBusConnection *conn);

static gboolean dbushandler_compare_hashtable_value(gpointer _key, gpointer _value, gpointer _data);

/* Public functions */

/**
 * Creates new dbushandler instance
 *
 * @return pointer to dbushandler instance
 */
DBusHandler* dbushandler_new(gchar* local_address, GMainLoop* loop)
{
  static gboolean instantiated = FALSE;
  DBusHandler *self = NULL;
  
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail(NULL != local_address, NULL);
  g_return_val_if_fail(NULL != loop, NULL);
  
  if (instantiated == TRUE)
    {
      /* Can be instantiated only once */
      whiteboard_log_error("DBusHandler already instantiated. "		\
			   "Won't create another instance.\n");
      whiteboard_log_error("DBusHandler already instantiated. "		\
			   "Won't create another instance.\n");
      return NULL;
    }
  
  self = g_new0(struct _DBusHandler, 1);
  
  self->loop = loop;
  g_main_loop_ref(loop);
  
  self->local_address = g_strdup(local_address);
  self->node_connections = NULL;
  self->control_connections = NULL;
  self->sib_connections = NULL;
  
  self->connection_map = g_hash_table_new_full(g_str_hash, g_str_equal,
					       g_free, NULL);
  
  self->access_node_map = g_hash_table_new(g_direct_hash, g_direct_equal);
  self->access_sib_map = g_hash_table_new(g_direct_hash,
					  g_direct_equal);
  self->subscription_map = g_hash_table_new_full(g_str_hash, g_str_equal,
						 g_free, NULL);
	
  if (-1 == dbushandler_initialize(self))
    {
      whiteboard_log_error("DBusHandler initialization failed.\n");
    }
	
  if (NULL != self)
    instantiated = TRUE;

  whiteboard_log_debug_fe();

  return self;
}

void dbushandler_destroy(DBusHandler *self)
{
  whiteboard_log_debug_fb();

  g_return_if_fail(NULL != self);

  g_main_loop_unref(self->loop);

  g_free(self->local_address);

  g_hash_table_destroy(self->connection_map);
  g_hash_table_destroy(self->access_node_map);
  g_hash_table_destroy(self->access_sib_map);
  g_hash_table_destroy(self->subscription_map);
  g_list_free(self->node_connections);
  g_list_free(self->control_connections);
  g_list_free(self->sib_connections);

	
  g_free(self);

  whiteboard_log_debug_fe();
}
#if 0
ServerObject *server_object_new(const gchar *path)
{
  GObject* object = NULL;
  ServerObject *self = NULL;
  
  whiteboard_log_debug_fb();

  //g_return_val_if_fail(username != NULL, NULL);

  object = g_object_new(OBJECT_TYPE_SERVER, NULL);
  if (object == NULL)
    {
      whiteboard_log_error("Out of memory!\n");
      return NULL;
    }
  self = SERVER_OBJECT(object);
  self->local_address = g_strdup(path);
  whiteboard_log_debug_fe();
  return self;
}

/* will create maman_bar_get_type and set maman_bar_parent_class */
G_DEFINE_TYPE (ServerObject, server_object, G_TYPE_OBJECT);

static GObject *
server_object_constructor (GType                  gtype,
			   guint                  n_properties,
			   GObjectConstructParam *properties)
{
  whiteboard_log_debug_fb();
  GObject *obj;

  {
    /* Always chain up to the parent constructor */
    ServerObjectClass *klass;
    GObjectClass *parent_class;  
    parent_class = G_OBJECT_CLASS (server_object_parent_class);
    obj = parent_class->constructor (gtype, n_properties, properties);
  }
  
  /* update the object state depending on constructor properties */
  whiteboard_log_debug_fe();
  return obj;
}


GType server_object_get_type()
{
  static GType type = 0;

  whiteboard_log_debug_fb();

  if (!type)
    {
      static const GTypeInfo info =
	{
	  sizeof(ServerObjectClass),
	  NULL,           /* base_init */
	  NULL,           /* base_finalize */
	  (GClassInitFunc) server_object_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof(ServerObject),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) server_object_init
	};

      type = g_type_register_static(G_TYPE_OBJECT,
				    "ServerObjectType", &info, 0);
    }

  whiteboard_log_debug_fe();

  return type;
}

void server_object_class_init(ServerObjectClass *klass)
{
	GError *error = NULL;
	whiteboard_log_debug_fb();
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	
	gobject_class->constructor = server_object_constructor;

	server_signals[SIGNAL_STARTING] =
	  g_signal_new("starting",
		       G_OBJECT_CLASS_TYPE(gobject_class),
		       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		       G_STRUCT_OFFSET(ServerObjectClass, starting_cb),
		       NULL,
		       NULL,
		       marshal_VOID__VOID,
		       G_TYPE_NONE,
		       0,
		       G_TYPE_NONE);
	
	
	/* Init the DBus connection, per-klass */
	klass->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (klass->connection == NULL)
	{
		g_warning("Unable to connect to dbus: %s", error->message);
		g_error_free (error);
		return;
	}

	/* &dbus_glib__object_info is provided in the server-bindings.h file */
	/* OBJECT_TYPE_SERVER is the GType of your server object */
	dbus_g_object_type_install_info (OBJECT_TYPE_SERVER, &dbus_glib_server_object_object_info);
	whiteboard_log_debug_fe();
}

void server_object_init(ServerObject *self)
{
  whiteboard_log_debug_fb();
  //ServerObject* self = SERVER_OBJECT(instance);

  g_return_if_fail(self != NULL);
  //g_return_if_fail(IS_SERVER_OBJECT(instance));
  

  whiteboard_log_debug_fe();
}

static gint server_register(ServerObject *self)
{
  gint ret = 0;
  GError *error = NULL;
  DBusGProxy *driver_proxy;
  ServerObjectClass *klass;
  int request_ret;
  whiteboard_log_debug_fb();
  klass = SERVER_OBJECT_GET_CLASS (self);
  
  /* Register DBUS path */
  dbus_g_connection_register_g_object (klass->connection,
				       self->local_address,
				       G_OBJECT (self));
  
  /* Register the service name, the constant here are defined in dbus-glib-bindings.h */
  driver_proxy = dbus_g_proxy_new_for_name (klass->connection,
					    DBUS_SERVICE_DBUS,
					    DBUS_PATH_DBUS,
					    DBUS_INTERFACE_DBUS);
  
  if(!org_freedesktop_DBus_request_name (driver_proxy,
					 "com.nokia.whiteboard.register",
					 0,
					 &request_ret,    /* See tutorial for more infos about these */
					 &error))
    {
      g_warning("Unable to register service: %s", error->message);
      g_error_free (error);
      ret = -1;
    }
  g_object_unref (driver_proxy);
  whiteboard_log_debug_fe();
  return ret;
}
#endif
/**
 * Set callback for source registration.
 *
 * @param self DBusHandler instance
 * @param cb Callback function
 * @param user_data User data pointer
 */
void dbushandler_set_callback_sib_registered(DBusHandler *self, 
					      WhiteBoardSIBRegisteredCB cb,
					      gpointer user_data)
{
  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != cb);

  self->sib_registered_cb = cb;
  self->user_data_sib_registered = user_data;
}

void dbushandler_set_callback_sib_handler( DBusHandler *self, 
					   WhiteBoardCommonPacketCB cb,
					   gpointer user_data)
{
  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != cb);
	
  self->sib_handler_cb = cb;
  self->user_data_sib_handler = user_data;
}

void dbushandler_set_callback_node_disconnected(DBusHandler *self, 
						WhiteBoardNodeDisconnectedCB cb,
						gpointer user_data)
{
  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != cb);
  
  self->node_disconnected_cb = cb;
  self->user_data_node_disconnected = user_data;
}

GList *dbushandler_get_node_connections(DBusHandler *self)
{
  g_return_val_if_fail(NULL != self, NULL);

  return self->node_connections;
}

GList *dbushandler_get_control_connections(DBusHandler *self)
{
  g_return_val_if_fail(NULL != self, NULL);

  return self->control_connections;
}

GList *dbushandler_get_sib_connections(DBusHandler *self)
{
  g_return_val_if_fail(NULL != self, NULL);

  return self->sib_connections;
}

GList *dbushandler_get_discovery_connections(DBusHandler *self)
{
  g_return_val_if_fail(NULL != self, NULL);

  return self->discovery_connections;
}

DBusConnection *dbushandler_get_session_bus(DBusHandler *self)
{
  g_return_val_if_fail(NULL != self, NULL);

  return self->session_bus;
}

/* Private functions */

static int dbushandler_register_node(DBusHandler *self, DBusConnection *conn,
				     DBusMessage *msg)
{
  gchar* registered_uuid = NULL;
  gchar* unique_name = NULL;
  gint status = -1;
  whiteboard_log_debug_fb();

  /* TODO: browse_id -> unique_id? -> util? */

  whiteboard_util_parse_message(msg, DBUS_TYPE_STRING, &registered_uuid,
				DBUS_TYPE_INVALID);
  unique_name = g_strdup_printf(":%d", whiteboard_sib_handler_get_access_id());
  whiteboard_log_debug("Setting unique name %s for ui connection: %s\n", registered_uuid, unique_name);
  
  dbus_bus_set_unique_name(conn, unique_name); 
  dbushandler_add_connection_by_uuid(self, g_strdup(unique_name), conn);
  
  dbushandler_add_connection_by_uuid(self, g_strdup(registered_uuid), conn);

  g_free(unique_name);

  self->node_connections = g_list_prepend(self->node_connections, conn);

  status = 0;
  whiteboard_util_send_method_return(conn, msg,
				     DBUS_TYPE_INT32, &status,
				     WHITEBOARD_UTIL_LIST_END);
  
  whiteboard_log_debug_fe();

  return DBUS_HANDLER_RESULT_HANDLED;
}

static int dbushandler_unregister_node(DBusHandler *self, DBusConnection *conn,
				       DBusMessage *msg)
{
  gchar* uuid = NULL;

  whiteboard_log_debug_fb();

  
  whiteboard_util_parse_message(msg,
				DBUS_TYPE_STRING, &uuid,
				DBUS_TYPE_INVALID);

  
  self->node_disconnected_cb(self, uuid, self->user_data_node_disconnected);
  
  if( dbushandler_remove_connection_by_uuid(self, uuid))
    {
      whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS, "Node connection w/ uuid: %s removed\n", uuid);
    }

  whiteboard_log_debug_fe();

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int dbushandler_register_control(DBusHandler *self, DBusConnection *conn,
					DBusMessage *msg)
{
  gchar *registered_uuid = NULL;
  gchar* unique_name = NULL;
  int status = -1;
  whiteboard_log_debug_fb();
  unique_name = g_strdup_printf(":%d", whiteboard_sib_handler_get_access_id());
  whiteboard_log_debug("Setting unique name for control connection: %s\n", unique_name);

  whiteboard_util_parse_message(msg, DBUS_TYPE_STRING, &registered_uuid,
				DBUS_TYPE_INVALID);

  whiteboard_log_debug("Registered uuid: %s\n", registered_uuid);

  dbushandler_add_connection_by_uuid(self, registered_uuid, conn);

  dbus_bus_set_unique_name(conn, unique_name); 
  g_free(unique_name);

  self->control_connections = g_list_prepend(self->control_connections,
					     conn);


  status = 0;
  whiteboard_util_send_method_return(conn, msg,
				     DBUS_TYPE_INT32, &status,
				     WHITEBOARD_UTIL_LIST_END);
  
  whiteboard_log_debug_fe();

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int dbushandler_register_sib(DBusHandler *self,
				     DBusConnection *conn,
				     DBusMessage *msg)
{
  gchar *registered_uuid = NULL;
  gchar *friendly_name = NULL;
  gchar *mimetypes = NULL;
  gboolean local = FALSE;
  gchar* unique_name = NULL;
  gint status = -1;
  whiteboard_log_debug_fb();
  unique_name = g_strdup_printf(":%d", whiteboard_sib_handler_get_access_id());
  whiteboard_log_debug("Setting unique name for node connection: %s\n", unique_name);

  whiteboard_util_parse_message(msg,
				DBUS_TYPE_STRING, &registered_uuid,
				DBUS_TYPE_STRING, &friendly_name,
				DBUS_TYPE_STRING, &mimetypes, // Not used for sources
				DBUS_TYPE_BOOLEAN, &local,
				DBUS_TYPE_INVALID);
	
  whiteboard_log_debug("Registered uuid: %s\n", registered_uuid);

  dbushandler_add_connection_by_uuid(self, registered_uuid, conn);

  dbus_bus_set_unique_name(conn, unique_name); 
  g_free(unique_name);

  self->sib_connections = g_list_prepend(self->sib_connections, conn);

  /* TODO: Pass local variable to callback */
  self->sib_registered_cb(self, 
			  registered_uuid,
			  friendly_name,
			  self->user_data_sib_registered);

  status = 0;
  whiteboard_util_send_method_return(conn, msg,
				     DBUS_TYPE_INT32, &status,
				     WHITEBOARD_UTIL_LIST_END);
  
  
  whiteboard_log_debug_fe();

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int dbushandler_register_discovery(DBusHandler *self,
					  DBusConnection *conn,
					  DBusMessage *msg)
{
  gchar* registered_uuid = NULL;
  gchar* unique_name = NULL;
  gint status = -1;
  whiteboard_log_debug_fb();

  /* TODO: browse_id -> unique_id? -> util? */

  whiteboard_util_parse_message(msg, DBUS_TYPE_STRING, &registered_uuid,
				DBUS_TYPE_INVALID);
  unique_name = g_strdup_printf(":%d", whiteboard_sib_handler_get_access_id());
  whiteboard_log_debug("Setting unique name for discovery connection: %s\n", registered_uuid, unique_name);
  
  dbus_bus_set_unique_name(conn, unique_name); 
  dbushandler_add_connection_by_uuid(self, g_strdup(unique_name), conn);
  
  dbushandler_add_connection_by_uuid(self, g_strdup(registered_uuid), conn);

  g_free(unique_name);

  self->discovery_connections = g_list_prepend(self->discovery_connections, conn);

  status = 0;
  whiteboard_util_send_method_return(conn, msg,
				     DBUS_TYPE_INT32, &status,
				     WHITEBOARD_UTIL_LIST_END);
  
  whiteboard_log_debug_fe();

  return DBUS_HANDLER_RESULT_HANDLED;
}

static gint dbushandler_initialize(DBusHandler *self)
{
  DBusServer *server = NULL;
  gint name_request_result = 0;
  gint retval = 0;
  DBusError err;
  GString *address = g_string_new("unix:path=");
  address = g_string_append(address, self->local_address);
  DBusObjectPathVTable vtable = { dbushandler_unregister_handler,
                                  dbushandler_handle_message,
                                  NULL, NULL, NULL, NULL};

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, -1);

  dbus_error_init(&err);

  if (NULL == (server = dbus_server_listen(address->str, &err)))
    {
      whiteboard_log_error("Could not create DBusServer: %s.\n", err.message);
      whiteboard_log_debug("TODO: error handler!\n");
      retval = -1;
    }
  g_string_free(address, FALSE);

				  
  /* TODO: check if dbushandler_delete should be forwarded here */
  dbus_server_set_new_connection_function(server,
					  dbushandler_handle_connection,
					  self, NULL);
  dbus_error_free(&err);

  if (NULL == (self->session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)))
    {
      whiteboard_log_error("Could not get session bus.\n");
      whiteboard_log_debug("TODO: error handler!\n");
      retval = -1;
    }

  dbus_error_free(&err);
  name_request_result = dbus_bus_request_name(self->session_bus,
					      WHITEBOARD_DBUS_SERVICE,
					      DBUS_NAME_FLAG_REPLACE_EXISTING,
					      &err);
  if ( -1 == name_request_result )
    {
      whiteboard_log_error("Could not register name to session bus.\n");
      whiteboard_log_debug("TODO: error handler!\n");
      whiteboard_log_debug("Return flag %d\n", name_request_result);
      retval = -1;
    }

  /* For discovery */
  dbus_connection_setup_with_g_main(self->session_bus,
				    g_main_loop_get_context(self->loop));

  if( !dbus_connection_register_object_path( self->session_bus,
					    WHITEBOARD_DBUS_OBJECT,
					    &vtable,
					    self) )
    {
      whiteboard_log_error("Could not register handlerrs for  session bus.\n");
      retval = -1;
    }
  
  //  dbushandler_handle_connection(NULL, self->session_bus, self);

  /* Send alive message to session bus, this should be caught by existing
   * sinks & sources and they should shutdown or do some tricks to avoid
   * duplicate processes.
   */
  whiteboard_log_debug("Sending alive message to session bus\n");
  whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
			      WHITEBOARD_DBUS_CONTROL_INTERFACE,
			      WHITEBOARD_DBUS_CONTROL_SIGNAL_STARTING,
			      self->session_bus,
			      WHITEBOARD_UTIL_LIST_END);

  dbus_connection_flush(self->session_bus);

  /* Point to point connections */
  dbus_server_setup_with_g_main(server,
				g_main_loop_get_context(self->loop));

  dbus_error_free(&err);

  whiteboard_log_debug_fe();

  return retval;
}
#if 0

static gint dbushandler_initialize(DBusHandler *self)
{
  whiteboard_log_debug_fb();
  gint name_request_result = 0;
  gint retval=0;
  DBusError err;
  DBusObjectPathVTable vtable = { dbushandler_unregister_handler,
                                  dbushandler_handle_message,
                                  NULL, NULL, NULL, NULL};
  
  ServerObject *server = server_object_new(self->local_address);
  if(!server)
    {
      whiteboard_log_debug("Could not create ServerObject\n");
      retval = -1;
    }
  
  g_return_val_if_fail(server_register(server)==0, -1);
  
  dbus_error_init(&err);
  
  if (NULL == (self->session_bus = dbus_bus_get(DBUS_BUS_SESSION, &err)))
    {
      whiteboard_log_error("Could not get session bus.\n");
      whiteboard_log_debug("TODO: error handler!\n");
      retval = -1;
    }

  dbus_error_free(&err);
  name_request_result = dbus_bus_request_name(self->session_bus,
					      WHITEBOARD_DBUS_SERVICE,
					      DBUS_NAME_FLAG_REPLACE_EXISTING,
					      &err);
  if ( -1 == name_request_result )
    {
      whiteboard_log_error("Could not register name to session bus.\n");
      whiteboard_log_debug("TODO: error handler!\n");
      whiteboard_log_debug("Return flag %d\n", name_request_result);
      retval = -1;
    }

  /* For discovery */
  dbus_connection_setup_with_g_main(self->session_bus,
				    g_main_loop_get_context(self->loop));

  if( !dbus_connection_register_object_path( self->session_bus,
					    WHITEBOARD_DBUS_OBJECT,
					    &vtable,
					    self) )
    {
      whiteboard_log_error("Could not register handlerrs for  session bus.\n");
      retval = -1;
    }
  
  //  dbushandler_handle_connection(NULL, self->session_bus, self);

  /* Send alive message to session bus, this should be caught by existing
   * sinks & sources and they should shutdown or do some tricks to avoid
   * duplicate processes.
   */
  whiteboard_log_debug("Sending alive message to session bus\n");
  //  whiteboard_util_send_signal(WHITEBOARD_DBUS_OBJECT,
  //			      WHITEBOARD_DBUS_CONTROL_INTERFACE,
  //			      WHITEBOARD_DBUS_CONTROL_SIGNAL_STARTING,
  //			      self->session_bus,
  //			      WHITEBOARD_UTIL_LIST_END);

  
  dbus_connection_flush(self->session_bus);

  /* Point to point connections */
  //  dbus_server_setup_with_g_main(server,
  //				g_main_loop_get_context(self->loop));

  dbus_error_free(&err);

  whiteboard_log_debug_fe();

  return retval;
}
#endif
static void dbushandler_handle_connection(DBusServer *server,
					  DBusConnection *conn,
					  gpointer data)
{
  DBusHandler *self = (DBusHandler *) data;

  whiteboard_log_debug_fb();

  /* TODO: check that connections are 
   * unreferenced correctly on dbushandler delete */
  dbus_connection_ref(conn);

  whiteboard_log_debug("Connection pointer: %p\n", conn);
  g_return_if_fail(NULL != self);

  dbus_connection_add_filter(conn, &dbushandler_handle_message, data, NULL);
  dbus_connection_setup_with_g_main(conn,
				    g_main_loop_get_context(self->loop));

  whiteboard_log_debug_fe();
}

static DBusHandlerResult dbushandler_whiteboard_register_message(
								 DBusHandler* self, DBusConnection* conn, DBusMessage* msg)
{
  const gchar* interface = NULL;
  const gchar* member = NULL;
  gint type = 0;
  DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  whiteboard_log_debug_fb();

  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);	

  switch (type)
    {
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
      /* Here we create a key -> connection pair for 
       * registered connections and add UI connections to 
       * connection list.
       *
       * Later we use these mappings to find out proper source /
       * sink when routing packets between sources, sinks and 
       * UIs.
       */
      if (!strcmp(member, WHITEBOARD_DBUS_REGISTER_METHOD_NODE))
	{
	  result = dbushandler_register_node(self, conn, msg);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_REGISTER_METHOD_CONTROL))
	{
	  result =  dbushandler_register_control(self, conn, msg);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_REGISTER_METHOD_SIB))
	{
	   result = dbushandler_register_sib(self, conn, msg);
	}
      else if (!strcmp(member, WHITEBOARD_DBUS_REGISTER_METHOD_DISCOVERY))
	{
	  result =  dbushandler_register_discovery(self, conn, msg);
	}
      else
	{
	  whiteboard_log_warning("Method %s not defined " \
				 " in interface %s", member,
				 interface);
	}
      break;

    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      whiteboard_log_warning("Return message %s not defined " \
			     " in interface %s", member,
			     interface);
      break;

    case DBUS_MESSAGE_TYPE_ERROR:
      whiteboard_log_warning("Error message %s not defined " \
			     " in interface %s", member,
			     interface);
      break;

    case DBUS_MESSAGE_TYPE_SIGNAL:
      if (!strcmp(member, WHITEBOARD_DBUS_REGISTER_SIGNAL_UNREGISTER_NODE))
	{
	  result = dbushandler_unregister_node(self, conn, msg);
	}
      else
	{
	  whiteboard_log_warning("Signal message %s not defined "	\
				 " in interface %s", member,
				 interface);
	}
      break;

    default:
      whiteboard_log_error("Unknown message type: %d\n", type);
      break;
    }

  whiteboard_log_debug_fe();

  return result;
}

static DBusHandlerResult dbushandler_whiteboard_general_message(
								DBusHandler* self, DBusConnection* conn, DBusMessage* msg)
{
  const gchar* interface = NULL;
  const gchar* member = NULL;
  const gchar* connection_name = NULL;
  //WhiteBoardPacket* packet = NULL;
  gint type = 0;
  GString *address;
  whiteboard_log_debug_fb();

  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);
  connection_name = dbus_bus_get_unique_name(conn);

  switch (type)
    {
    case DBUS_MESSAGE_TYPE_METHOD_CALL:
      if (!strcmp(member, WHITEBOARD_DBUS_METHOD_DISCOVERY))
	{
	  whiteboard_log_debug("Discovery request.\n");
	  address = g_string_new("unix:path=");
	  address = g_string_append(address,self->local_address); 
	  whiteboard_util_send_method_return(conn, msg, 
					     DBUS_TYPE_STRING, &address->str,
					     WHITEBOARD_UTIL_LIST_END);
	  g_string_free(address,FALSE);
	}
      else if (!strcmp(member, WHITEBOARD_METHOD_CUSTOM_COMMAND))
	{
	  whiteboard_log_debug("Custom command request.\n");

	  if ( NULL != connection_name )
	    dbus_message_set_sender(msg, connection_name);

	  dbushandler_custom_command_request(self, msg);
	}
      else
	{
	  whiteboard_log_warning("Method %s not defined " \
				 " in interface %s\n", member,
				 interface);
	}
      break;

    case DBUS_MESSAGE_TYPE_METHOD_RETURN:
      if (!strcmp(member, WHITEBOARD_DBUS_METHOD_CUSTOM_COMMAND))
	{
	  whiteboard_log_debug("Custom command response.\n");

	  if ( NULL != connection_name )
	    dbus_message_set_sender(msg, connection_name);

	  dbushandler_custom_command_response(self, msg);
	}
      else
	{
	  whiteboard_log_warning("Method return %s not defined " \
				 " in interface %s", member,
				 interface);
	}
      break;

    case DBUS_MESSAGE_TYPE_ERROR:
      whiteboard_log_warning("Error message %s not defined " \
			     " in interface %s", member,
			     interface);
      break;

    case DBUS_MESSAGE_TYPE_SIGNAL:
      whiteboard_log_warning("Signal message %s not defined " \
			     " in interface %s", member,
			     interface);
      break;

    default:
      whiteboard_log_error("Unknown message type: %d\n", type);
      break;
    }

  return DBUS_HANDLER_RESULT_HANDLED;

  whiteboard_log_debug_fe();
}

static void dbushandler_custom_command_request(DBusHandler* self,
					       DBusMessage* msg)
{
  WhiteBoardCmd* cmd = NULL;
  const gchar* uuid = NULL;
  const gchar* method = NULL;
  DBusConnection* conn = NULL;
  guint serial = 0;

  whiteboard_log_debug_fb();

  cmd = whiteboard_cmd_new_with_msg(msg);
  uuid = whiteboard_cmd_get_target_uuid(cmd);
  method = whiteboard_cmd_get_method_name(cmd);

  whiteboard_log_debug("Custom command [%s] to [%s] received.\n",
		       method, uuid);

  conn = dbushandler_get_connection_by_uuid(self, (gchar*) uuid);
  if (conn != NULL)
    {
      dbus_message_set_interface(msg, WHITEBOARD_DBUS_CONTROL_INTERFACE);
      if (dbus_connection_send(conn, msg, &serial) == FALSE)
	{
	  whiteboard_log_error(
			       "Unable to re-route custom cmd to %s.\n", uuid);
	}
    }

  whiteboard_cmd_unref(cmd);

  whiteboard_log_debug_fe();
}

static void dbushandler_custom_command_response(DBusHandler* self,
						DBusMessage* msg)
{
  DBusConnection* connection = NULL;
  gchar* destination = NULL;

  whiteboard_log_debug_fb();

  destination = (gchar*) dbus_message_get_destination(msg);

  whiteboard_log_debug("Getting message destination (%s)\n", destination);
  connection = dbushandler_get_connection_by_uuid(self, destination);

  whiteboard_util_forward_packet(connection, msg, NULL, NULL, NULL, NULL);

  whiteboard_log_debug_fe();
}

static void dbushandler_org_freedesktop_dbus_message(DBusHandler* self,
						     DBusConnection* conn,
						     DBusMessage* msg)
{
  const gchar *interface = NULL;
  const gchar *member = NULL;
  gint type;
  const gchar *rsp = ":1.12345";
  gchar *rules=NULL;
  gchar *nameowner;
  //DBusError err;
  whiteboard_log_debug_fb();
  
  /* TODO? */

  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);
  
  whiteboard_log_debug("interface: %s\n", interface);
  whiteboard_log_debug("member: %s\n", member);
  
  if(type == DBUS_MESSAGE_TYPE_METHOD_CALL)
    {
      if(!strcmp(member, "GetNameOwner") )
	{
	  whiteboard_util_parse_message(msg,
					DBUS_TYPE_STRING, &nameowner,
					DBUS_TYPE_INVALID);
	  if(nameowner)
	    {
	      whiteboard_log_debug("Got GetNameOwner request for %s\n", nameowner);
	      whiteboard_util_send_method_return(conn, msg,
						 DBUS_TYPE_STRING,
						 &rsp,
						 WHITEBOARD_UTIL_LIST_END);
	    }
	  
	}
      else if(!strcmp(member, "AddMatch") )
	{
	  whiteboard_util_parse_message(msg,
					DBUS_TYPE_STRING, &rules,
					DBUS_TYPE_INVALID);
	  if(rules)
	    {
	      //dbus_error_init(&err);
	      //	      dbus_bus_add_match(conn,
	      //				 rules, &err);
	      //if(!dbus_error_is_set(&err))
	      //		{
	      whiteboard_log_debug("Added match rules: %s\n", rules);
	      //	}
	      //else
	      //	{
	      //	  whiteboard_log_debug("Could not add match rules: %s\n", err.message);
	      //	}
	      
	      // dbus_error_free(&err);
	    }
	}
    }
  else
    {
      switch(type)
	{
	case DBUS_MESSAGE_TYPE_METHOD_CALL:
	  whiteboard_log_debug("type: method_call\n");
	  break;
	case DBUS_MESSAGE_TYPE_METHOD_RETURN:
	  whiteboard_log_debug("type: method_return\n");
	  break;
	case DBUS_MESSAGE_TYPE_SIGNAL:
	  whiteboard_log_debug("type: signal\n");
	  break;
	case DBUS_MESSAGE_TYPE_ERROR:
	  whiteboard_log_debug("type: error\n");
	  break;
	}
      
    }
  
  whiteboard_log_debug_fe();
}

static void dbushandler_org_freedesktop_dbus_local( DBusHandler* self,
						    DBusConnection* conn,
						    DBusMessage* msg)
{
   const gchar* interface = NULL;
  const gchar* member = NULL;
  gint type = 0;
  
  whiteboard_log_debug_fb();
  
  g_return_if_fail( NULL != self );
  g_return_if_fail( NULL != msg );
  
  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);
  
  if(!strcmp(member, "Disconnected"))
    {
      whiteboard_log_debug("Got Disconnected message\n");
      dbushandler_handle_disconnect( self, conn);
    }
  else
    {
      whiteboard_log_debug("Unknown message on org.freedesktop.DBus.Local interface, member %s\n", member);
    }
  whiteboard_log_debug_fe();
}

static DBusHandlerResult dbushandler_org_freedesktop_dbus_introspectable( DBusHandler* self,
							     DBusConnection* conn,
							     DBusMessage* msg)
{
  DBusHandlerResult retval = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
   const gchar* interface = NULL;
  const gchar* member = NULL;
  gint type = 0;
  const gchar *introspect_rsp = "";
  whiteboard_log_debug_fb();
  
  g_return_val_if_fail( NULL != self, retval );
  g_return_val_if_fail( NULL != msg, retval );
  
  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);
  
  if(!strcmp(member, "Introspect"))
    {
      whiteboard_log_debug("Got Introspect\n");
      
      whiteboard_util_send_method_return(conn, msg,
      					 DBUS_TYPE_STRING,
      					 &introspect_rsp,
      					 WHITEBOARD_UTIL_LIST_END);
      retval = DBUS_HANDLER_RESULT_HANDLED;
    }
  else
    {
      whiteboard_log_debug("Unknown message on org.freedesktop.DBus.Local interface, member %s\n", member);
    }
  whiteboard_log_debug_fe();
  return retval;
}

static void dbushandler_handle_disconnect( DBusHandler* self,
					   DBusConnection* conn)
{
  whiteboard_log_debug_fb();
  // Assume that it was a node connection that left
  // TODO: apply also for SIB access
  rmData *rm = NULL;
  gchar *nodeid = NULL;
  rm = g_new0(rmData,1);
  rm->value = conn;
  
  while( g_hash_table_find(self->connection_map, dbushandler_compare_hashtable_value, rm ) )
    {
      nodeid = g_strdup((gchar *)rm->key);
      self->node_disconnected_cb(self, nodeid, self->user_data_node_disconnected);
      
      dbushandler_remove_connection_by_uuid(self, nodeid);
      
      g_free(nodeid);
    }
  g_free(rm);
  whiteboard_log_debug_fe(); 
}
static gboolean dbushandler_compare_hashtable_value(gpointer _key, gpointer _value, gpointer _data)
{
  rmData *rm = (rmData *)_data;
  g_return_val_if_fail(rm != NULL, FALSE);
  if( _value == rm->value)
    {
      rm->key = _key;
      return TRUE;
    }
  return FALSE;
}

static DBusHandlerResult dbushandler_handle_message(DBusConnection *conn,
						    DBusMessage *msg,
						    gpointer data)
{
  DBusHandler* self = NULL;
  const gchar* interface = NULL;
  const gchar* member = NULL;
  const gchar* connection_name = NULL;
  WhiteBoardPacket* packet = NULL;
  gint type = 0;
  DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  whiteboard_log_debug_fb();

  self = (DBusHandler *) data;
  g_return_val_if_fail(NULL != self, DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

  interface = dbus_message_get_interface(msg);
  member = dbus_message_get_member(msg);
  type = dbus_message_get_type(msg);
  connection_name = dbus_bus_get_unique_name(conn);

  whiteboard_log_debug("%s %s %d\n", interface, member, type);
  g_return_val_if_fail(NULL != interface,
		       DBUS_HANDLER_RESULT_NOT_YET_HANDLED);

  packet = g_new0(WhiteBoardPacket, 1);
  g_return_val_if_fail(NULL != packet, DBUS_HANDLER_RESULT_NEED_MEMORY);

  /* TODO: Could be optimized, sender is not needed in many
   * of the routed packets.
   */
  if (!strcmp(interface, WHITEBOARD_DBUS_NODE_INTERFACE))
    {
      whiteboard_log_debug("Got node_access packet\n");
	    
      if ( NULL != connection_name )
	dbus_message_set_sender(msg, connection_name);
	  
      packet->message = msg;
      packet->connection = conn;
      self->sib_handler_cb(self, packet, self->user_data_sib_handler);
      result = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (!strcmp(interface, WHITEBOARD_DBUS_SIB_ACCESS_INTERFACE))
    {
      whiteboard_log_debug("Got node packet\n");
	    
      if ( NULL != connection_name )
	dbus_message_set_sender(msg, connection_name);
	    
      packet->message = msg;
      packet->connection = conn;
      self->sib_handler_cb(self, packet, self->user_data_sib_handler);
      result = DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (!strcmp(interface, WHITEBOARD_DBUS_DISCOVERY_INTERFACE))
    {
      whiteboard_log_debug("Got discovery packet\n");
	    
      if ( NULL != connection_name )
	dbus_message_set_sender(msg, connection_name);
	    
      packet->message = msg;
      packet->connection = conn;
      self->sib_handler_cb(self, packet, self->user_data_sib_handler);
    }
  else if (!strcmp(interface, WHITEBOARD_DBUS_REGISTER_INTERFACE))
    {
      whiteboard_log_debug("Got register packet\n");

      //if ( NULL != connection_name )
      //	dbus_message_set_sender(msg, connection_name);

      result = dbushandler_whiteboard_register_message(self, conn, msg);
    }
  else if (!strcmp(interface, WHITEBOARD_DBUS_INTERFACE))
    {
      whiteboard_log_debug("Got general whiteboard packet\n");

      /* Don't set the sender because session bus goes wacko */

      dbushandler_whiteboard_general_message(self, conn, msg);
      result = DBUS_HANDLER_RESULT_HANDLED;      
    }
  else if (!strcmp(interface, WHITEBOARD_DBUS_LOG_INTERFACE))
    {
      GList* node_connections = dbushandler_get_node_connections(self); 
	  
      whiteboard_log_debug("Got log message packet\n"); 
	  
      if ( NULL != connection_name ) 
	dbus_message_set_sender(msg, connection_name); 
	  
      whiteboard_util_send_message_to_list(node_connections, msg);
      result = DBUS_HANDLER_RESULT_HANDLED;      
      
    }
  else if (!strcmp(interface, DBUS_INTERFACE_DBUS))
    {
      whiteboard_log_debug("Got generic DBus packet\n");
      dbushandler_org_freedesktop_dbus_message(self, conn, msg);
      result = DBUS_HANDLER_RESULT_HANDLED;      
    }
  else if (!strcmp(interface, DBUS_INTERFACE_LOCAL))
    {
      whiteboard_log_debug("Got Local DBus packet\n");
      dbushandler_org_freedesktop_dbus_local( self, conn,msg);
    }
  else if( !strcmp(interface, DBUS_INTERFACE_INTROSPECTABLE ) )
    {
      whiteboard_log_debug("Got Introspectable DBus packet\n");
      result = dbushandler_org_freedesktop_dbus_introspectable( self, conn,msg);
    }
  else
    {
      whiteboard_log_warning("Unknown interface: %s (member: %s)\n", 
			     interface, member);
    }
	
  g_free(packet);

  whiteboard_log_debug_fe();

  /* TODO: Check what should be returned here */
  return result;
}

/*****************************************************************************
 * Connection map manipulation
 *****************************************************************************/

void dbushandler_add_connection_by_uuid(DBusHandler* self, gchar* uuid,
					DBusConnection* conn)
{
  whiteboard_log_debug_fb();

  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != uuid);
  g_return_if_fail(NULL != conn);

  g_hash_table_insert(self->connection_map, g_strdup(uuid), conn);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Insert UUID: %s, conn: %p. Map size: %d\n",
			uuid, conn, g_hash_table_size(self->connection_map));

  whiteboard_log_debug_fe();
}

DBusConnection *dbushandler_get_connection_by_uuid(DBusHandler *self,
						   gchar *uuid)
{
  DBusConnection* conn = NULL;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, NULL);
  g_return_val_if_fail(NULL != uuid, NULL);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Trying to get UUID: %s, Map size: %d\n",
			uuid, g_hash_table_size(self->connection_map));
	
  conn = (DBusConnection*) g_hash_table_lookup(self->connection_map, uuid);

  whiteboard_log_debug_fe();

  return conn;
}

gboolean dbushandler_remove_connection_by_uuid(DBusHandler* self, gchar* uuid)
{
  DBusConnection* conn = NULL;
  gboolean retval = FALSE;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, FALSE);
  g_return_val_if_fail(NULL != uuid, FALSE);

  conn = dbushandler_get_connection_by_uuid(self, uuid);
  if (conn != NULL)
    {
      /* Remove the connection from the hash map */
      retval = g_hash_table_remove(self->connection_map, uuid);

      whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			    "Removed:%s, conn:%p, ok:%s. Map size:%d\n",
			    uuid, conn, (retval) ? "TRUE" : "FALSE",
			    g_hash_table_size(self->connection_map));

      self->node_connections = 
	g_list_remove(self->node_connections, conn);
      self->sib_connections = 
	g_list_remove(self->sib_connections, conn);
      
      // dbus_connection_unref(conn);
    }

  whiteboard_log_debug_fe();

  return retval;
}

/*****************************************************************************
 * Connection lists by access ID
 *****************************************************************************/

DBusConnection *dbushandler_get_sib_connection_by_access_id(DBusHandler *self,
							     gint accessid)
{
  DBusConnection* conn = NULL;
  
  whiteboard_log_debug_fb();
  
  conn = (DBusConnection*) g_hash_table_lookup(self->access_sib_map,
					       GINT_TO_POINTER(accessid));
  
  whiteboard_log_debug_fe();
  
  return conn;
}

DBusConnection *dbushandler_get_node_connection_by_access_id(DBusHandler *self,
							   gint accessid)
{
  DBusConnection* conn = NULL;
  
  whiteboard_log_debug_fb();
  
  conn = (DBusConnection*) g_hash_table_lookup(self->access_node_map,
					       GINT_TO_POINTER(accessid));
  
  whiteboard_log_debug_fe();
  
  return conn;
}

void dbushandler_set_sib_connection_with_access_id(DBusHandler *self,
						   gint accessid,
						   DBusConnection* conn)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug("Validating (sib) access id: %d\n", accessid);
  g_hash_table_insert(self->access_sib_map,
		      GINT_TO_POINTER(accessid), (gpointer) conn);
  whiteboard_log_debug_fe();
}

void dbushandler_set_node_connection_with_access_id(DBusHandler *self,
						    gint accessid,
						    DBusConnection* conn)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug("Validating (node) access id: %d\n", accessid);
  g_hash_table_insert(self->access_node_map,
		      GINT_TO_POINTER(accessid), (gpointer) conn);
  whiteboard_log_debug_fe();
}

void dbushandler_associate_access_id(DBusHandler* self,
				     gint accessid, 
				     DBusConnection* node_conn,
				     DBusConnection* sib_conn)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug("Validating access id: %d\n", accessid);
  g_hash_table_insert(self->access_node_map,
		      GINT_TO_POINTER(accessid),
		      (gpointer) node_conn);
  
  g_hash_table_insert(self->access_sib_map,
		      GINT_TO_POINTER(accessid),
		      (gpointer) sib_conn);
  
  whiteboard_log_debug_fe();
}

void dbushandler_invalidate_access_id(DBusHandler *self, gint accessid)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug("Invalidating access id: %d\n", accessid);
  g_hash_table_remove(self->access_node_map,
		      GINT_TO_POINTER(accessid));
  g_hash_table_remove(self->access_sib_map,
		      GINT_TO_POINTER(accessid));
  whiteboard_log_debug_fe();
}


void dbushandler_add_connection_by_subscription_id(DBusHandler* self,
						   gchar* subscription_id,
						   DBusConnection* conn)
{
  whiteboard_log_debug_fb();

  g_return_if_fail(NULL != self);
  g_return_if_fail(NULL != subscription_id);
  g_return_if_fail(NULL != conn);

  g_hash_table_insert(self->subscription_map, g_strdup(subscription_id), conn);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Insert subscr_id: %s, conn: %p. Map size: %d\n",
			subscription_id, conn, g_hash_table_size(self->subscription_map));

  whiteboard_log_debug_fe();
}

DBusConnection *dbushandler_get_connection_by_subscription_id(DBusHandler *self,
							      gchar *subscription_id)
{
  DBusConnection* conn = NULL;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, NULL);
  g_return_val_if_fail(NULL != subscription_id, NULL);

  whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			"Trying to get SUBSCRIPTION_ID: %s, Map size: %d\n",
			subscription_id, g_hash_table_size(self->subscription_map));
	
  conn = (DBusConnection*) g_hash_table_lookup(self->subscription_map, subscription_id);

  whiteboard_log_debug_fe();

  return conn;
}

gboolean dbushandler_remove_connection_by_subscription_id(DBusHandler* self, gchar* subscription_id)
{
  DBusConnection* conn = NULL;
  gboolean retval = FALSE;

  whiteboard_log_debug_fb();

  g_return_val_if_fail(NULL != self, FALSE);
  g_return_val_if_fail(NULL != subscription_id, FALSE);

  conn = dbushandler_get_connection_by_subscription_id(self, subscription_id);
  if (conn != NULL)
    {
      /* Remove the connection from the hash map */
      retval = g_hash_table_remove(self->subscription_map, subscription_id);

      whiteboard_log_debugc(WHITEBOARD_DEBUG_DBUS,
			    "Removed:%s, conn:%p, ok:%s. Map size:%d\n",
			    subscription_id, conn, (retval) ? "TRUE" : "FALSE",
			    g_hash_table_size(self->subscription_map));
		
      // dbus_connection_unref(conn);
    }

  whiteboard_log_debug_fe();

  return retval;
}

static void dbushandler_unregister_handler(DBusConnection* conn,gpointer data)
{
  // TODO 
}
#if 0
gboolean discover_discover (ServerObject *server, gchar *uuid, gchar **address, GError **error)
{
  GString *path = g_string_new("unix:path=");
  whiteboard_log_debug_fb();
  path = g_string_append(path,server->local_address);
  *address = g_strdup(path->str);
  
  g_string_free(path,FALSE);
  
  whiteboard_log_debug_fe();
  return TRUE;
}

gboolean register_node (ServerObject *server, gchar *uuid, gint *success,GError **error)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug_fe();
  return TRUE;
}

gboolean register_sib (ServerObject *server, gchar *uuid, gint *success, GError **error)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug_fe();
  return TRUE;
}

gboolean register_control (ServerObject *server, gchar *uuid, gint *success, GError **error)
{
  whiteboard_log_debug_fb();
  whiteboard_log_debug_fe();
  return TRUE;
}

#endif
/* Keep this preprocessor instruction always at the end of the file */
#endif /* UNIT_TEST_INCLUDE_IMPLEMENTATION */
