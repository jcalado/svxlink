/**
@file    settings.h
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

#ifndef QTEL_SETTINGS_H
#define QTEL_SETTINGS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define SETTINGS_TYPE (settings_get_type())

G_DECLARE_FINAL_TYPE(Settings, settings, QTEL, SETTINGS, GObject)

/**
 * @brief Create a new Settings instance
 * @return A new Settings object
 */
Settings *settings_new(void);

/**
 * @brief Get the singleton Settings instance
 * @return The singleton Settings object
 */
Settings *settings_get_default(void);

// User info getters
const gchar *settings_get_callsign(Settings *self);
const gchar *settings_get_password(Settings *self);
const gchar *settings_get_name(Settings *self);
const gchar *settings_get_location(Settings *self);
const gchar *settings_get_info(Settings *self);

// Network getters
const gchar *settings_get_directory_servers(Settings *self);
gint settings_get_list_refresh_time(Settings *self);
gboolean settings_get_start_as_busy(Settings *self);
const gchar *settings_get_bind_address(Settings *self);

// Proxy getters
gboolean settings_get_proxy_enabled(Settings *self);
const gchar *settings_get_proxy_server(Settings *self);
guint16 settings_get_proxy_port(Settings *self);
const gchar *settings_get_proxy_password(Settings *self);

// Audio getters
const gchar *settings_get_mic_audio_device(Settings *self);
const gchar *settings_get_spkr_audio_device(Settings *self);
gboolean settings_get_use_full_duplex(Settings *self);
const gchar *settings_get_connect_sound(Settings *self);
gint settings_get_card_sample_rate(Settings *self);

// VOX getters
gboolean settings_get_vox_enabled(Settings *self);
gint settings_get_vox_threshold(Settings *self);
gint settings_get_vox_delay(Settings *self);

// QSO getters
const gchar *settings_get_chat_encoding(Settings *self);

// Bookmarks
gchar **settings_get_bookmarks(Settings *self);
void settings_set_bookmarks(Settings *self, const gchar * const *bookmarks);
void settings_add_bookmark(Settings *self, const gchar *callsign);
void settings_remove_bookmark(Settings *self, const gchar *callsign);

// Get the underlying GSettings object for direct access if needed
GSettings *settings_get_gsettings(Settings *self);

G_END_DECLS

#endif /* QTEL_SETTINGS_H */
