/**
@file    station-object.cpp
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

#include "station-object.h"
#include <cstring>

struct _StationObject
{
  GObject parent_instance;

  gchar *callsign;
  gchar *description;
  StationStatus status;
  gchar *time;
  gint id;
  gchar *ip_address;
  StationType station_type;
};

G_DEFINE_TYPE(StationObject, station_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CALLSIGN,
  PROP_DESCRIPTION,
  PROP_STATUS,
  PROP_TIME,
  PROP_ID,
  PROP_IP_ADDRESS,
  PROP_STATION_TYPE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static StationType
determine_station_type(const gchar *callsign)
{
  if (callsign == nullptr || callsign[0] == '\0')
    return STATION_TYPE_USER;

  // Conference: starts with *
  if (callsign[0] == '*')
    return STATION_TYPE_CONFERENCE;

  size_t len = strlen(callsign);
  if (len >= 2)
  {
    // Repeater: ends with -R
    if (callsign[len-2] == '-' && callsign[len-1] == 'R')
      return STATION_TYPE_REPEATER;

    // Link: ends with -L
    if (callsign[len-2] == '-' && callsign[len-1] == 'L')
      return STATION_TYPE_LINK;
  }

  return STATION_TYPE_USER;
}

static void
station_object_finalize(GObject *object)
{
  StationObject *self = STATION_OBJECT(object);

  g_free(self->callsign);
  g_free(self->description);
  g_free(self->time);
  g_free(self->ip_address);

  G_OBJECT_CLASS(station_object_parent_class)->finalize(object);
}

static void
station_object_get_property(GObject *object, guint prop_id,
                             GValue *value, GParamSpec *pspec)
{
  StationObject *self = STATION_OBJECT(object);

  switch (prop_id)
  {
    case PROP_CALLSIGN:
      g_value_set_string(value, self->callsign);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string(value, self->description);
      break;
    case PROP_STATUS:
      g_value_set_int(value, self->status);
      break;
    case PROP_TIME:
      g_value_set_string(value, self->time);
      break;
    case PROP_ID:
      g_value_set_int(value, self->id);
      break;
    case PROP_IP_ADDRESS:
      g_value_set_string(value, self->ip_address);
      break;
    case PROP_STATION_TYPE:
      g_value_set_int(value, self->station_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
station_object_set_property(GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
  StationObject *self = STATION_OBJECT(object);

  switch (prop_id)
  {
    case PROP_CALLSIGN:
      g_free(self->callsign);
      self->callsign = g_value_dup_string(value);
      self->station_type = determine_station_type(self->callsign);
      break;
    case PROP_DESCRIPTION:
      g_free(self->description);
      self->description = g_value_dup_string(value);
      break;
    case PROP_STATUS:
      self->status = static_cast<StationStatus>(g_value_get_int(value));
      break;
    case PROP_TIME:
      g_free(self->time);
      self->time = g_value_dup_string(value);
      break;
    case PROP_ID:
      self->id = g_value_get_int(value);
      break;
    case PROP_IP_ADDRESS:
      g_free(self->ip_address);
      self->ip_address = g_value_dup_string(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
station_object_class_init(StationObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->finalize = station_object_finalize;
  object_class->get_property = station_object_get_property;
  object_class->set_property = station_object_set_property;

  properties[PROP_CALLSIGN] =
    g_param_spec_string("callsign", "Callsign", "Station callsign",
                        "", G_PARAM_READWRITE);

  properties[PROP_DESCRIPTION] =
    g_param_spec_string("description", "Description", "Station description",
                        "", G_PARAM_READWRITE);

  properties[PROP_STATUS] =
    g_param_spec_int("status", "Status", "Station status",
                     STATION_STATUS_UNKNOWN, STATION_STATUS_BUSY,
                     STATION_STATUS_UNKNOWN, G_PARAM_READWRITE);

  properties[PROP_TIME] =
    g_param_spec_string("time", "Time", "Local time at station",
                        "", G_PARAM_READWRITE);

  properties[PROP_ID] =
    g_param_spec_int("id", "ID", "EchoLink node ID",
                     0, G_MAXINT, 0, G_PARAM_READWRITE);

  properties[PROP_IP_ADDRESS] =
    g_param_spec_string("ip-address", "IP Address", "Station IP address",
                        "", G_PARAM_READWRITE);

  properties[PROP_STATION_TYPE] =
    g_param_spec_int("station-type", "Station Type", "Type of station",
                     STATION_TYPE_USER, STATION_TYPE_CONFERENCE,
                     STATION_TYPE_USER, G_PARAM_READABLE);

  g_object_class_install_properties(object_class, N_PROPS, properties);
}

static void
station_object_init(StationObject *self)
{
  self->callsign = g_strdup("");
  self->description = g_strdup("");
  self->status = STATION_STATUS_UNKNOWN;
  self->time = g_strdup("");
  self->id = 0;
  self->ip_address = g_strdup("");
  self->station_type = STATION_TYPE_USER;
}

StationObject *
station_object_new(const gchar *callsign,
                    const gchar *description,
                    StationStatus status,
                    const gchar *time,
                    gint id,
                    const gchar *ip_address)
{
  return static_cast<StationObject*>(
    g_object_new(STATION_TYPE_OBJECT,
                 "callsign", callsign ? callsign : "",
                 "description", description ? description : "",
                 "status", status,
                 "time", time ? time : "",
                 "id", id,
                 "ip-address", ip_address ? ip_address : "",
                 nullptr)
  );
}

const gchar *
station_object_get_callsign(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), nullptr);
  return self->callsign;
}

const gchar *
station_object_get_description(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), nullptr);
  return self->description;
}

StationStatus
station_object_get_status(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), STATION_STATUS_UNKNOWN);
  return self->status;
}

const gchar *
station_object_get_status_string(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), "Unknown");
  return station_status_to_string(self->status);
}

const gchar *
station_object_get_time(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), nullptr);
  return self->time;
}

gint
station_object_get_id(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), 0);
  return self->id;
}

const gchar *
station_object_get_ip_address(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), nullptr);
  return self->ip_address;
}

StationType
station_object_get_station_type(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), STATION_TYPE_USER);
  return self->station_type;
}

void
station_object_set_status(StationObject *self, StationStatus status)
{
  g_return_if_fail(STATION_IS_OBJECT(self));
  if (self->status != status)
  {
    self->status = status;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_STATUS]);
  }
}

void
station_object_set_description(StationObject *self, const gchar *description)
{
  g_return_if_fail(STATION_IS_OBJECT(self));
  if (g_strcmp0(self->description, description) != 0)
  {
    g_free(self->description);
    self->description = g_strdup(description ? description : "");
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DESCRIPTION]);
  }
}

void
station_object_set_time(StationObject *self, const gchar *time)
{
  g_return_if_fail(STATION_IS_OBJECT(self));
  if (g_strcmp0(self->time, time) != 0)
  {
    g_free(self->time);
    self->time = g_strdup(time ? time : "");
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_TIME]);
  }
}

const gchar *
station_status_to_string(StationStatus status)
{
  switch (status)
  {
    case STATION_STATUS_OFFLINE: return "Offline";
    case STATION_STATUS_ONLINE:  return "Online";
    case STATION_STATUS_BUSY:    return "Busy";
    default:                     return "Unknown";
  }
}

const gchar *
station_object_get_status_icon_name(StationObject *self)
{
  g_return_val_if_fail(STATION_IS_OBJECT(self), "network-offline-symbolic");

  switch (self->status)
  {
    case STATION_STATUS_ONLINE:
      return "emblem-ok-symbolic";
    case STATION_STATUS_BUSY:
      return "user-busy-symbolic";
    case STATION_STATUS_OFFLINE:
    default:
      return "network-offline-symbolic";
  }
}
