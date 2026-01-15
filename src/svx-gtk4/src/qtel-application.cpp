/**
@file    qtel-application.cpp
@brief   Main application class for Qtel GTK4
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

#include "qtel-application.h"
#include "qtel-window.h"
#include "qtel-preferences.h"
#include "qtel-config.h"
#include "settings.h"

#include <AsyncAudioIO.h>

using namespace Async;

struct _QtelApplication
{
  AdwApplication parent_instance;

  Settings *settings;
};

G_DEFINE_TYPE(QtelApplication, qtel_application, ADW_TYPE_APPLICATION)

static void
qtel_application_activate(GApplication *app)
{
  QtelApplication *self = QTEL_APPLICATION(app);
  GtkWindow *window;

  // Get the current window or create a new one
  window = gtk_application_get_active_window(GTK_APPLICATION(app));
  if (window == nullptr)
  {
    window = GTK_WINDOW(qtel_window_new(QTEL_APPLICATION(app)));
  }

  gtk_window_present(window);
}

// Internal sample rate used by svxlink audio system
#ifndef INTERNAL_SAMPLE_RATE
#define INTERNAL_SAMPLE_RATE 16000
#endif

static void
setup_audio_params(int sample_rate)
{
  // Configure audio based on sample rate (same as Qt version)
  if (sample_rate == 48000)
  {
    AudioIO::setBlocksize(1024);
    AudioIO::setBlockCount(4);
  }
  else if (sample_rate == 16000)
  {
    AudioIO::setBlocksize(512);
    AudioIO::setBlockCount(2);
  }
#if INTERNAL_SAMPLE_RATE <= 8000
  else if (sample_rate == 8000)
  {
    AudioIO::setBlocksize(256);
    AudioIO::setBlockCount(2);
  }
#endif
  AudioIO::setSampleRate(sample_rate);
  // Use mono audio - the AudioIO system will duplicate to both stereo channels
  AudioIO::setChannels(1);
}

static void
qtel_application_startup(GApplication *app)
{
  QtelApplication *self = QTEL_APPLICATION(app);

  G_APPLICATION_CLASS(qtel_application_parent_class)->startup(app);

  // Initialize settings
  self->settings = settings_new();

  // Set up audio parameters - read sample rate from settings
  GSettings *gsettings = g_settings_new(APP_ID);
  int sample_rate = g_settings_get_int(gsettings, "card-sample-rate");
  if (sample_rate <= 0)
    sample_rate = INTERNAL_SAMPLE_RATE;
  setup_audio_params(sample_rate);
  g_object_unref(gsettings);

  // Set up application actions
  static const GActionEntry app_actions[] = {
    { "quit", [](GSimpleAction*, GVariant*, gpointer user_data) {
        g_application_quit(G_APPLICATION(user_data));
      }, nullptr, nullptr, nullptr },
    { "about", [](GSimpleAction*, GVariant*, gpointer user_data) {
        QtelApplication *self = QTEL_APPLICATION(user_data);
        GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(self));

        adw_show_about_dialog(
          GTK_WIDGET(window),
          "application-name", "Qtel",
          "application-icon", APP_ID,
          "version", APP_VERSION,
          "copyright", "Copyright 2003-2024 Tobias Blomberg / SM0SVX",
          "license-type", GTK_LICENSE_GPL_2_0,
          "developers", (const char *[]){
            "Tobias Blomberg / SM0SVX",
            "GTK4 port contributors",
            nullptr
          },
          "website", "https://www.svxlink.org",
          "issue-url", "https://github.com/sm0svx/svxlink/issues",
          "comments", "EchoLink client for amateur radio operators",
          nullptr
        );
      }, nullptr, nullptr, nullptr },
    { "preferences", [](GSimpleAction*, GVariant*, gpointer user_data) {
        QtelApplication *self = QTEL_APPLICATION(user_data);
        GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(self));
        QtelPreferences *prefs = qtel_preferences_new(window);
        gtk_window_present(GTK_WINDOW(prefs));
      }, nullptr, nullptr, nullptr },
    { "shortcuts", [](GSimpleAction*, GVariant*, gpointer user_data) {
        QtelApplication *self = QTEL_APPLICATION(user_data);
        GtkWindow *window = gtk_application_get_active_window(GTK_APPLICATION(self));

        GtkBuilder *builder = gtk_builder_new_from_resource("/org/svxlink/qtel/shortcuts.ui");
        GtkWidget *shortcuts = GTK_WIDGET(gtk_builder_get_object(builder, "shortcuts"));

        gtk_window_set_transient_for(GTK_WINDOW(shortcuts), window);
        gtk_window_present(GTK_WINDOW(shortcuts));

        g_object_unref(builder);
      }, nullptr, nullptr, nullptr },
  };

  g_action_map_add_action_entries(G_ACTION_MAP(app), app_actions,
                                   G_N_ELEMENTS(app_actions), app);

  // Set up keyboard shortcuts
  const char *quit_accels[] = { "<Control>q", nullptr };
  const char *prefs_accels[] = { "<Control>comma", nullptr };
  const char *refresh_accels[] = { "<Control>r", "F5", nullptr };
  const char *shortcuts_accels[] = { "<Control>question", nullptr };
  const char *connect_ip_accels[] = { "<Control>i", nullptr };
  const char *search_accels[] = { "<Control>f", nullptr };

  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.preferences", prefs_accels);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.shortcuts", shortcuts_accels);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.refresh", refresh_accels);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.connect-ip", connect_ip_accels);
  gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.search", search_accels);
}

static void
qtel_application_shutdown(GApplication *app)
{
  QtelApplication *self = QTEL_APPLICATION(app);

  // Clean up settings
  if (self->settings != nullptr)
  {
    g_object_unref(self->settings);
    self->settings = nullptr;
  }

  G_APPLICATION_CLASS(qtel_application_parent_class)->shutdown(app);
}

static void
qtel_application_class_init(QtelApplicationClass *klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

  app_class->activate = qtel_application_activate;
  app_class->startup = qtel_application_startup;
  app_class->shutdown = qtel_application_shutdown;
}

static void
qtel_application_init(QtelApplication *self)
{
  self->settings = nullptr;
}

QtelApplication *
qtel_application_new(void)
{
  return static_cast<QtelApplication*>(
    g_object_new(QTEL_TYPE_APPLICATION,
                 "application-id", APP_ID,
                 "flags", G_APPLICATION_DEFAULT_FLAGS,
                 nullptr)
  );
}
