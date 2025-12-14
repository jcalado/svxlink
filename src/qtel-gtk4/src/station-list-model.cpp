/**
@file    station-list-model.cpp
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

#include "station-list-model.h"
#include <vector>
#include <algorithm>

struct _StationListModel
{
  GObject parent_instance;

  std::vector<StationObject*> *stations;
};

static void station_list_model_iface_init(GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE(StationListModel, station_list_model, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, station_list_model_iface_init))

static GType
station_list_model_get_item_type(GListModel *model)
{
  return STATION_TYPE_OBJECT;
}

static guint
station_list_model_get_n_items(GListModel *model)
{
  StationListModel *self = STATION_LIST_MODEL(model);
  return static_cast<guint>(self->stations->size());
}

static gpointer
station_list_model_get_item(GListModel *model, guint position)
{
  StationListModel *self = STATION_LIST_MODEL(model);

  if (position >= self->stations->size())
    return nullptr;

  StationObject *station = self->stations->at(position);
  return g_object_ref(station);
}

static void
station_list_model_iface_init(GListModelInterface *iface)
{
  iface->get_item_type = station_list_model_get_item_type;
  iface->get_n_items = station_list_model_get_n_items;
  iface->get_item = station_list_model_get_item;
}

static void
station_list_model_finalize(GObject *object)
{
  StationListModel *self = STATION_LIST_MODEL(object);

  // Unref all stations
  for (StationObject *station : *self->stations)
  {
    g_object_unref(station);
  }
  delete self->stations;

  G_OBJECT_CLASS(station_list_model_parent_class)->finalize(object);
}

static void
station_list_model_class_init(StationListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = station_list_model_finalize;
}

static void
station_list_model_init(StationListModel *self)
{
  self->stations = new std::vector<StationObject*>();
}

StationListModel *
station_list_model_new(void)
{
  return static_cast<StationListModel*>(
    g_object_new(STATION_TYPE_LIST_MODEL, nullptr)
  );
}

void
station_list_model_clear(StationListModel *self)
{
  g_return_if_fail(STATION_IS_LIST_MODEL(self));

  guint old_size = static_cast<guint>(self->stations->size());
  if (old_size == 0)
    return;

  // Unref all stations
  for (StationObject *station : *self->stations)
  {
    g_object_unref(station);
  }
  self->stations->clear();

  // Notify that items were removed
  g_list_model_items_changed(G_LIST_MODEL(self), 0, old_size, 0);
}

void
station_list_model_add(StationListModel *self, StationObject *station)
{
  g_return_if_fail(STATION_IS_LIST_MODEL(self));
  g_return_if_fail(STATION_IS_OBJECT(station));

  guint position = static_cast<guint>(self->stations->size());
  self->stations->push_back(g_object_ref(station));

  // Notify that an item was added
  g_list_model_items_changed(G_LIST_MODEL(self), position, 0, 1);
}

StationObject *
station_list_model_find_by_callsign(StationListModel *self, const gchar *callsign)
{
  g_return_val_if_fail(STATION_IS_LIST_MODEL(self), nullptr);
  g_return_val_if_fail(callsign != nullptr, nullptr);

  for (StationObject *station : *self->stations)
  {
    if (g_strcmp0(station_object_get_callsign(station), callsign) == 0)
    {
      return station;
    }
  }
  return nullptr;
}

StationObject *
station_list_model_find_by_id(StationListModel *self, gint id)
{
  g_return_val_if_fail(STATION_IS_LIST_MODEL(self), nullptr);

  for (StationObject *station : *self->stations)
  {
    if (station_object_get_id(station) == id)
    {
      return station;
    }
  }
  return nullptr;
}

void
station_list_model_update_or_add(StationListModel *self,
                                  const gchar *callsign,
                                  const gchar *description,
                                  StationStatus status,
                                  const gchar *time,
                                  gint id,
                                  const gchar *ip_address)
{
  g_return_if_fail(STATION_IS_LIST_MODEL(self));

  // Try to find existing station by callsign
  StationObject *existing = station_list_model_find_by_callsign(self, callsign);

  if (existing != nullptr)
  {
    // Update existing station
    station_object_set_status(existing, status);
    station_object_set_description(existing, description);
    station_object_set_time(existing, time);
    // Note: id and ip_address typically don't change, but could be updated if needed
  }
  else
  {
    // Add new station
    StationObject *station = station_object_new(callsign, description, status,
                                                 time, id, ip_address);
    station_list_model_add(self, station);
    g_object_unref(station); // add() takes a ref
  }
}

guint
station_list_model_get_count(StationListModel *self)
{
  g_return_val_if_fail(STATION_IS_LIST_MODEL(self), 0);
  return static_cast<guint>(self->stations->size());
}
