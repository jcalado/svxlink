/**
@file    station-list-model.h
@brief   GListModel implementation for station list
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

#ifndef STATION_LIST_MODEL_H
#define STATION_LIST_MODEL_H

#include <gio/gio.h>
#include "station-object.h"

G_BEGIN_DECLS

#define STATION_TYPE_LIST_MODEL (station_list_model_get_type())

G_DECLARE_FINAL_TYPE(StationListModel, station_list_model, STATION, LIST_MODEL, GObject)

/**
 * @brief Create a new StationListModel
 * @return A new StationListModel implementing GListModel
 */
StationListModel *station_list_model_new(void);

/**
 * @brief Clear all stations from the model
 * @param self The model
 */
void station_list_model_clear(StationListModel *self);

/**
 * @brief Add a station to the model
 * @param self The model
 * @param station The station object to add (takes ownership)
 */
void station_list_model_add(StationListModel *self, StationObject *station);

/**
 * @brief Find a station by callsign
 * @param self The model
 * @param callsign The callsign to search for
 * @return The station object or NULL if not found
 */
StationObject *station_list_model_find_by_callsign(StationListModel *self,
                                                    const gchar *callsign);

/**
 * @brief Find a station by node ID
 * @param self The model
 * @param id The EchoLink node ID
 * @return The station object or NULL if not found
 */
StationObject *station_list_model_find_by_id(StationListModel *self, gint id);

/**
 * @brief Update an existing station or add if not found
 * @param self The model
 * @param callsign Station callsign
 * @param description Station description
 * @param status Station status
 * @param time Local time at station
 * @param id EchoLink node ID
 * @param ip_address IP address string
 */
void station_list_model_update_or_add(StationListModel *self,
                                       const gchar *callsign,
                                       const gchar *description,
                                       StationStatus status,
                                       const gchar *time,
                                       gint id,
                                       const gchar *ip_address);

/**
 * @brief Get the number of stations in the model
 * @param self The model
 * @return The number of stations
 */
guint station_list_model_get_count(StationListModel *self);

G_END_DECLS

#endif /* STATION_LIST_MODEL_H */
