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
/**
 *
 * @file dbushandler.h
 * @brief Dbushandler for the daemon.
 *
 * Copyright 2007 Nokia Corporation
 *
 * @mainpage WhiteBoard daemon for Maemo platform
 *
 * @section intro_sec Introduction
 *
 * WhiteBoard daemon is a background process that is aware of all Node applications and of all SIB access components.  It routes requests from Nodes to correct SIB access instance and also routes response and indication messages from different SIBs to correct Node application. All Nodes and SIB access processes must register to the WhiteBoard daemon process to enable daemon to route messages to correct destination.
 *
 * @section dbus_interfaces_sec DBUS interfaces
 *
 * @section register_if_ssec Register interace
 * 
 * All processes connecting to Whiteboard daemon must register themselves with the daemon. The whiteboard daemon provides dedicated interface for registering. The register interface contains method calls for registering WhiteBoardNode, WhiteBoardSibAccess and WhiteBoardControl instances. Also when descructing the WhiteBoardNode instance, the Node process should unregister itself so that the daemon knows not to send anymore messages to the corresponding WhiteBoardNode instance.
 *
 * @subsection node_if_ssec NODE interface
 *
 * All nodes access Whiteboard infrastucture via Node interface. All DBUS message sending and receiving is done by the WhiteBoardNode GObject provided by WhiteBoard library, so Node applications do not need to handle any DBUS messages themselves, but create WhiteBoardNode instance. For details, see WhiteBoard library documentation.
 *
 * @section sib_access_if_ssec SIB Access interface
 *
 * 
 *
 * @section control_if_ssec Control interface
 *
 * @section install_sec Building and installing WhiteBoard daemon
 *
 * @subsection install_sbox_sec Scratchbox environment
 *
 * > ./autogen.sh
 *
 * > ./configure --with-debug --prefix=/usr/local
 *
 * > make
 *
 * > make install
 *
 * The configure script checks whether Whiteboard library (libwhiteboard) is installed. If the check fails, make sure that your PKG_CONFIG_PATH environment variable contains path where the pkg-config file of the libwhiteboard is installed. Usually this is PREFIX/lib/pkgconfig, where PREFIX is the value of the parameter --prefix used when building Whiteboard library. Normally PREFIX used is /usr or /usr/local. Make sure that your LD_LIBRARY_PATH environment variable includes /usr/local/lib
 *
 * Because the Node applications discover the Whiteboard daemon is done via DBUS session bus, the Whiteboard deamon process (whiteboardd) must be executed by the same user executing the Node application. Otherwise the DBUS session bus intances are different and discovery fails. If the WhiteboardNode fails to discover the whitebard daemon (most probably due to Whiteboard deamon process not started), it tries to start the Whiteboard daemon process thru DBUS StartServiceByName method call. For this to work, the .service file of the Whiteboard daemon must be present in path /usr/share/dbus-1/services. Normally in N800 the .service file is copied to the right location when installing the Whiteboard daemon component, but in Scratchbox enviroment you may need to copy the file by hand. The .service file is located in etc subfolder under the code tree of the Whiteboard daemon. If you want to run the Whiteboard daemon by hand, it is started by giving following command inside Scratchbox:
 *
 * > run-standalone.sh whiteboardd
 *
 * The Whiteboard daemon will start SIB access processes from the location PREFIX/lib/whiteboard/libexec, so make sure that desired SIB access modules are installed properly before running the Whiteboard daemon. In the libexec directory must not be any other executables than the executables of desired SIB access modules, because the Whiteboard deamon will start all executables found in the libexec directory.
 *
 * @subsection install_n800_sec N800 device
 *
 * Building and installing Whiteboard daemon to N800 device is straightforward. First, one must create the installation .deb package, transfer it to the memory card of the N800 device and, finally, install the package with N800's Application Manager.
 *
 * Before you start building the installation package, make sure that you have selected correct target and Whiteboard library is built and installed to the target. The compile target is selected with following command:
 *
 * > sb-conf select SDK_ARMEL
 *
 * Then change your working directory to the root directory of the Whiteboard daemon tree and type command:
 *
 * > dpkg-buildpackage -rfakeroot
 *
 * which will create the .deb installation file.  The dpkg-buildpackage reads parameters given for the configure script from the debian/rules files. So you might want to check the contents of the rules file prior running the dpkg-buildpackage.
 *
 * The installation file can be transferred to the memory card of the N800 for example via USB cable. When connecting the USB cable to your PC the memory cards of the N800 are seen as removable disks. The installation is done be browsing the location where you copied the .deb file with the File Manager and double clicking the .deb file which will launch the Application manager. Prior installation of the Whiteboard daemon, the Whiteboard library must be properly installed.
 *
 **/

#ifndef DBUSHANDLER_H
#define DBUSHANDLER_H

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

struct _DBusHandler;
typedef struct _DBusHandler DBusHandler;

typedef struct _WhiteBoardPacket
{
	DBusConnection *connection;
	DBusMessage *message;
} WhiteBoardPacket;

/**
 * Creates new dbushandler instance
 *
 * @return pointer to dbushandler instance
 */
DBusHandler *dbushandler_new();

/**
 * Destroys dbushandler instance
 *
 * @param self pointer to dbushandler instance
 */
void dbushandler_destroy(DBusHandler *self);


/**
 * Callback definition for sib registration events
 */
typedef void (*WhiteBoardSIBRegisteredCB) (DBusHandler* context,  
					   gchar* uuid,
					   gchar* name, 
					   gpointer user_data);


/**
 * Callback definition for messages
 */
typedef void (*WhiteBoardCommonPacketCB) (DBusHandler *context,
					  WhiteBoardPacket *packet,
					  gpointer user_data);

/**
 * Callback definition for sib registration events
 */
typedef void (*WhiteBoardNodeDisconnectedCB) (DBusHandler* context,  
					      gchar* uuid,
					      gpointer user_data);

/**
 * Set callback for sib access packets.
 *
 * @param self DBusHandler instance
 * @param cb Callback function
 * @param user_data User data pointer
 */
void dbushandler_set_callback_sib_handler(DBusHandler *self,
					 WhiteBoardCommonPacketCB cb,
					 gpointer user_data);

/**
 * Set callback for SIB registration.
 *
 * @param self DBusHandler instance
 * @param cb Callback function
 * @param user_data User data pointer
 */
void dbushandler_set_callback_sib_registered(DBusHandler *self, 
					     WhiteBoardSIBRegisteredCB cb,
					     gpointer user_data);

/**
 * Set callback for Node disconnected.
 *
 * @param self DBusHandler instance
 * @param cb Callback function
 * @param user_data User data pointer
 */
void dbushandler_set_callback_node_disconnected(DBusHandler *self, 
					     WhiteBoardNodeDisconnectedCB cb,
					     gpointer user_data);

/**
 * Get Dbus connection reference to session daemon.
 *
 * @param self DBusHandler instance
 *
 * @return DBusConnection pointer
 */
DBusConnection *dbushandler_get_session_bus(DBusHandler *self);

/**
 * Get all available control connections.
 *
 * @param self DBusHandler instance
 *
 * @return Pointer to GList structure containing the needed dbus connections.
 */
GList * dbushandler_get_control_connections(DBusHandler *self); 

/**
 * Get all available Node connections.
 *
 * @param self DBusHandler instance
 *
 * @return Pointer to GList structure containing the needed dbus connections.
 */
GList * dbushandler_get_node_connections(DBusHandler *self); 



/**
 * Get all available Discovery connections.
 *
 * @param self DBusHandler instance
 *
 * @return Pointer to GList structure containing the needed dbus connections.
 */
GList * dbushandler_get_discovery_connections(DBusHandler *self); 


/**
 * Add an association between a DBusConnection and a UUID to the connection map
 *
 * @param self DBusHandler instance
 * @param uuid UUID
 * @param conn A DBusConnection instance to associate with the UUID
 */
void dbushandler_add_connection_by_uuid(DBusHandler* self, gchar* uuid,
					DBusConnection* conn);

/**
 * Get DBusConnection reference by uuid
 *
 * @param self DBusHandler instance
 * @param uuid UUID
 *
 * @return Registered DBusConnection reference
 */
DBusConnection *dbushandler_get_connection_by_uuid(DBusHandler *self,
						   gchar *uuid);

/**
 * Remove a DBusConnection reference from connection map by a UUID
 * associated with the connection.
 *
 * @param self DBusHandler instance
 * @param uuid UUID
 * @return TRUE if successful, otherwise FALSE
 */
gboolean dbushandler_remove_connection_by_uuid(DBusHandler* self, gchar* uuid);


/**
 * Add an association between a DBusConnection and a subscription_id to the subscription_map
 *
 * @param self DBusHandler instance
 * @param subscription_id Subscription identifier
 * @param conn A DBusConnection instance to associate with the subscription_id
 */
void dbushandler_add_connection_by_subscription_id(DBusHandler* self,
						   gchar* subscription_id,
						   DBusConnection* conn);

/**
 * Get DBusConnection reference by subscription_id
 *
 * @param self DBusHandler instance
 * @param subscription_id Subscription identifier
 *
 * @return Registered DBusConnection reference
 */
DBusConnection *dbushandler_get_connection_by_subscription_id(DBusHandler *self,
						   gchar *subscription_id);

/**
 * Remove a DBusConnection reference from subscription_map by a subscription_id
 * associated with the connection.
 *
 * @param self DBusHandler instance
 * @param subscription_id Subscription identifier
 * @return TRUE if successful, otherwise FALSE
 */
gboolean dbushandler_remove_connection_by_subscription_id(DBusHandler* self,
							  gchar* subscription_id);



DBusConnection *dbushandler_get_sib_access_connection_by_access_id(DBusHandler *self,
								   gint accessid);

DBusConnection *dbushandler_get_node_connection_by_access_id(DBusHandler *self,
							     gint accessid);

void dbushandler_set_sib_connection_with_access_id(DBusHandler *self,
						   gint accessid,
						   DBusConnection* sib_access_conn);

void dbushandler_set_node_connection_with_access_id(DBusHandler *self,
						    gint accessid,
						    DBusConnection* node_conn);

void dbushandler_associate_access_id(DBusHandler* self,
				     gint accessid, 
				     DBusConnection* node_conn,
				     DBusConnection* sib_access_conn);

void dbushandler_invalidate_access_id(DBusHandler *self, gint accessid);

#endif
