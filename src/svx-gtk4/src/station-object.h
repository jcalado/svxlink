/**
@file    station-object.h
@brief   GObject wrapper for station data
@author  Joel (GTK4 port)
@date    2024-12-14

\verbatim
Qtel - The Qt EchoLink client (GTK4 port)
Copyright (C) 2003-2024 Tobias Blomberg / SM0SVX

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\endverbatim
*/

#ifndef STATION_OBJECT_H
#define STATION_OBJECT_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * Station status enumeration (mirrors EchoLink::StationData::Status)
 */
typedef enum {
  STATION_STATUS_UNKNOWN = 0,
  STATION_STATUS_OFFLINE,
  STATION_STATUS_ONLINE,
  STATION_STATUS_BUSY
} StationStatus;

/**
 * Station type enumeration (derived from callsign pattern)
 */
typedef enum {
  STATION_TYPE_USER = 0,      // Regular user
  STATION_TYPE_REPEATER,      // Callsign ends with -R
  STATION_TYPE_LINK,          // Callsign ends with -L
  STATION_TYPE_CONFERENCE     // Callsign starts with *
} StationType;

#define STATION_TYPE_OBJECT (station_object_get_type())

G_DECLARE_FINAL_TYPE(StationObject, station_object, STATION, OBJECT, GObject)

/**
 * @brief Create a new StationObject
 * @param callsign Station callsign
 * @param description Station description/location
 * @param status Station status
 * @param time Local time at station
 * @param id EchoLink node ID
 * @param ip_address IP address string
 * @return A new StationObject
 */
StationObject *station_object_new(const gchar *callsign,
                                   const gchar *description,
                                   StationStatus status,
                                   const gchar *time,
                                   gint id,
                                   const gchar *ip_address);

// Property getters
const gchar *station_object_get_callsign(StationObject *self);
const gchar *station_object_get_description(StationObject *self);
StationStatus station_object_get_status(StationObject *self);
const gchar *station_object_get_status_string(StationObject *self);
const gchar *station_object_get_time(StationObject *self);
gint station_object_get_id(StationObject *self);
const gchar *station_object_get_ip_address(StationObject *self);
StationType station_object_get_station_type(StationObject *self);

// Property setters (for updates)
void station_object_set_status(StationObject *self, StationStatus status);
void station_object_set_description(StationObject *self, const gchar *description);
void station_object_set_time(StationObject *self, const gchar *time);

// Utility functions
const gchar *station_status_to_string(StationStatus status);
const gchar *station_object_get_status_icon_name(StationObject *self);

G_END_DECLS

#endif /* STATION_OBJECT_H */
