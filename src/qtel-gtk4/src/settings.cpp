/**
@file    settings.cpp
@brief   GSettings wrapper for Qtel GTK4
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

#include "settings.h"
#include "qtel-config.h"

struct _Settings
{
  GObject parent_instance;

  GSettings *gsettings;
};

G_DEFINE_TYPE(Settings, settings, G_TYPE_OBJECT)

static Settings *default_instance = nullptr;

enum {
  SIGNAL_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
on_gsettings_changed(GSettings *gsettings, const gchar *key, gpointer user_data)
{
  Settings *self = QTEL_SETTINGS(user_data);
  g_signal_emit(self, signals[SIGNAL_CHANGED], g_quark_from_string(key));
}

static void
settings_dispose(GObject *object)
{
  Settings *self = QTEL_SETTINGS(object);

  g_clear_object(&self->gsettings);

  if (default_instance == self)
  {
    default_instance = nullptr;
  }

  G_OBJECT_CLASS(settings_parent_class)->dispose(object);
}

static void
settings_class_init(SettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = settings_dispose;

  signals[SIGNAL_CHANGED] = g_signal_new(
    "changed",
    G_TYPE_FROM_CLASS(klass),
    static_cast<GSignalFlags>(G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED),
    0,
    nullptr, nullptr,
    nullptr,
    G_TYPE_NONE, 0
  );
}

static void
settings_init(Settings *self)
{
  self->gsettings = g_settings_new(APP_ID);
  g_signal_connect(self->gsettings, "changed",
                   G_CALLBACK(on_gsettings_changed), self);
}

Settings *
settings_new(void)
{
  return static_cast<Settings*>(g_object_new(SETTINGS_TYPE, nullptr));
}

Settings *
settings_get_default(void)
{
  if (default_instance == nullptr)
  {
    default_instance = settings_new();
  }
  return default_instance;
}

// User info getters
const gchar *
settings_get_callsign(Settings *self)
{
  return g_settings_get_string(self->gsettings, "callsign");
}

const gchar *
settings_get_password(Settings *self)
{
  return g_settings_get_string(self->gsettings, "password");
}

const gchar *
settings_get_name(Settings *self)
{
  return g_settings_get_string(self->gsettings, "name");
}

const gchar *
settings_get_location(Settings *self)
{
  return g_settings_get_string(self->gsettings, "location");
}

const gchar *
settings_get_info(Settings *self)
{
  return g_settings_get_string(self->gsettings, "info");
}

// Network getters
const gchar *
settings_get_directory_servers(Settings *self)
{
  return g_settings_get_string(self->gsettings, "directory-servers");
}

gint
settings_get_list_refresh_time(Settings *self)
{
  return g_settings_get_int(self->gsettings, "list-refresh-time");
}

gboolean
settings_get_start_as_busy(Settings *self)
{
  return g_settings_get_boolean(self->gsettings, "start-as-busy");
}

const gchar *
settings_get_bind_address(Settings *self)
{
  return g_settings_get_string(self->gsettings, "bind-address");
}

// Proxy getters
gboolean
settings_get_proxy_enabled(Settings *self)
{
  return g_settings_get_boolean(self->gsettings, "proxy-enabled");
}

const gchar *
settings_get_proxy_server(Settings *self)
{
  return g_settings_get_string(self->gsettings, "proxy-server");
}

guint16
settings_get_proxy_port(Settings *self)
{
  return static_cast<guint16>(g_settings_get_uint(self->gsettings, "proxy-port"));
}

const gchar *
settings_get_proxy_password(Settings *self)
{
  return g_settings_get_string(self->gsettings, "proxy-password");
}

// Audio getters
const gchar *
settings_get_mic_audio_device(Settings *self)
{
  return g_settings_get_string(self->gsettings, "mic-audio-device");
}

const gchar *
settings_get_spkr_audio_device(Settings *self)
{
  return g_settings_get_string(self->gsettings, "spkr-audio-device");
}

gboolean
settings_get_use_full_duplex(Settings *self)
{
  return g_settings_get_boolean(self->gsettings, "use-full-duplex");
}

const gchar *
settings_get_connect_sound(Settings *self)
{
  return g_settings_get_string(self->gsettings, "connect-sound");
}

gint
settings_get_card_sample_rate(Settings *self)
{
  return g_settings_get_int(self->gsettings, "card-sample-rate");
}

// VOX getters
gboolean
settings_get_vox_enabled(Settings *self)
{
  return g_settings_get_boolean(self->gsettings, "vox-enabled");
}

gint
settings_get_vox_threshold(Settings *self)
{
  return g_settings_get_int(self->gsettings, "vox-threshold");
}

gint
settings_get_vox_delay(Settings *self)
{
  return g_settings_get_int(self->gsettings, "vox-delay");
}

// QSO getters
const gchar *
settings_get_chat_encoding(Settings *self)
{
  return g_settings_get_string(self->gsettings, "chat-encoding");
}

// Bookmarks
gchar **
settings_get_bookmarks(Settings *self)
{
  return g_settings_get_strv(self->gsettings, "bookmarks");
}

void
settings_set_bookmarks(Settings *self, const gchar * const *bookmarks)
{
  g_settings_set_strv(self->gsettings, "bookmarks", bookmarks);
}

void
settings_add_bookmark(Settings *self, const gchar *callsign)
{
  g_auto(GStrv) bookmarks = settings_get_bookmarks(self);

  // Check if already exists
  for (int i = 0; bookmarks[i] != nullptr; i++)
  {
    if (g_strcmp0(bookmarks[i], callsign) == 0)
    {
      return; // Already bookmarked
    }
  }

  // Add new bookmark
  guint len = g_strv_length(bookmarks);
  gchar **new_bookmarks = g_new0(gchar*, len + 2);
  for (guint i = 0; i < len; i++)
  {
    new_bookmarks[i] = g_strdup(bookmarks[i]);
  }
  new_bookmarks[len] = g_strdup(callsign);
  new_bookmarks[len + 1] = nullptr;

  g_settings_set_strv(self->gsettings, "bookmarks",
                      const_cast<const gchar * const *>(new_bookmarks));
  g_strfreev(new_bookmarks);
}

void
settings_remove_bookmark(Settings *self, const gchar *callsign)
{
  g_auto(GStrv) bookmarks = settings_get_bookmarks(self);
  guint len = g_strv_length(bookmarks);

  gchar **new_bookmarks = g_new0(gchar*, len + 1);
  guint j = 0;

  for (guint i = 0; i < len; i++)
  {
    if (g_strcmp0(bookmarks[i], callsign) != 0)
    {
      new_bookmarks[j++] = g_strdup(bookmarks[i]);
    }
  }
  new_bookmarks[j] = nullptr;

  g_settings_set_strv(self->gsettings, "bookmarks",
                      const_cast<const gchar * const *>(new_bookmarks));
  g_strfreev(new_bookmarks);
}

GSettings *
settings_get_gsettings(Settings *self)
{
  return self->gsettings;
}
