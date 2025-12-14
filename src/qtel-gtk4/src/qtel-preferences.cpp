/**
@file    qtel-preferences.cpp
@brief   Preferences window for Qtel GTK4
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

#include "qtel-preferences.h"
#include "qtel-config.h"
#include "settings.h"

#include <AsyncAudioIO.h>
#include <AsyncAudioSource.h>
#include <AsyncAudioSink.h>
#include <cmath>

using namespace Async;

// Internal sample rate used by svxlink audio system
#ifndef INTERNAL_SAMPLE_RATE
#define INTERNAL_SAMPLE_RATE 16000
#endif

// Forward declarations for audio test classes
class ToneGenerator;
class LevelMeter;

struct _QtelPreferences
{
  AdwPreferencesWindow parent_instance;

  GSettings *settings;

  // User page widgets
  GtkWidget *callsign_entry;
  GtkWidget *name_entry;
  GtkWidget *password_entry;
  GtkWidget *confirm_password_entry;
  GtkWidget *location_entry;
  GtkWidget *info_entry;

  // Network page widgets
  GtkWidget *servers_entry;
  GtkWidget *refresh_time_spin;
  GtkWidget *start_busy_switch;
  GtkWidget *bind_address_entry;

  // Proxy widgets
  GtkWidget *proxy_enable_switch;
  GtkWidget *proxy_server_entry;
  GtkWidget *proxy_port_spin;
  GtkWidget *proxy_password_entry;

  // Audio page widgets
  GtkWidget *mic_device_entry;
  GtkWidget *spkr_device_entry;
  GtkWidget *sample_rate_dropdown;
  GtkWidget *full_duplex_switch;
  GtkWidget *connect_sound_entry;

  // Audio test widgets
  GtkWidget *spkr_test_button;
  GtkWidget *mic_test_button;
  GtkWidget *mic_level_bar;
  guint mic_test_timeout_id;

  // Audio test state
  AudioIO *test_spkr_audio;
  AudioIO *test_mic_audio;
  ToneGenerator *tone_gen;
  LevelMeter *level_meter;
  bool mic_testing;

  // VOX widgets
  GtkWidget *vox_enable_switch;
  GtkWidget *vox_threshold_spin;
  GtkWidget *vox_delay_spin;

  // QSO page widgets
  GtkWidget *chat_encoding_dropdown;
};

G_DEFINE_TYPE(QtelPreferences, qtel_preferences, ADW_TYPE_PREFERENCES_WINDOW)

// Simple tone generator for audio output testing
class ToneGenerator : public AudioSource
{
public:
  ToneGenerator(int freq = 440, float amplitude = 0.5f)
    : m_freq(freq), m_amplitude(amplitude), m_phase(0.0), m_samples_left(0) {}

  void start(int duration_ms)
  {
    m_samples_left = (INTERNAL_SAMPLE_RATE * duration_ms) / 1000;
    m_phase = 0.0;
    resumeOutput();
  }

  void stop()
  {
    m_samples_left = 0;
  }

  void resumeOutput() override
  {
    if (m_samples_left <= 0)
      return;

    const int BLOCK_SIZE = 512;
    float buf[BLOCK_SIZE];

    while (m_samples_left > 0)
    {
      int to_write = std::min(BLOCK_SIZE, m_samples_left);
      for (int i = 0; i < to_write; i++)
      {
        buf[i] = m_amplitude * std::sin(m_phase);
        m_phase += 2.0 * M_PI * m_freq / INTERNAL_SAMPLE_RATE;
        if (m_phase > 2.0 * M_PI)
          m_phase -= 2.0 * M_PI;
      }
      m_samples_left -= to_write;
      int written = sinkWriteSamples(buf, to_write);
      if (written == 0)
        break;  // Sink is full
    }

    if (m_samples_left <= 0)
    {
      sinkFlushSamples();
    }
  }

  void allSamplesFlushed() override {}

private:
  int m_freq;
  float m_amplitude;
  double m_phase;
  int m_samples_left;
};

// Simple level meter for audio input testing
class LevelMeter : public AudioSink
{
public:
  LevelMeter() : m_level(0.0f), m_peak(0.0f) {}

  float level() const { return m_level; }
  float peak() const { return m_peak; }
  void resetPeak() { m_peak = 0.0f; }

  int writeSamples(const float *samples, int count) override
  {
    float sum = 0.0f;
    float max_sample = 0.0f;
    for (int i = 0; i < count; i++)
    {
      float abs_sample = std::fabs(samples[i]);
      sum += abs_sample * abs_sample;
      if (abs_sample > max_sample)
        max_sample = abs_sample;
    }
    // RMS level
    m_level = std::sqrt(sum / count);
    if (max_sample > m_peak)
      m_peak = max_sample;
    return count;
  }

  void flushSamples() override
  {
    sourceAllSamplesFlushed();
  }

private:
  float m_level;
  float m_peak;
};

// Forward declarations
static void stop_speaker_test(QtelPreferences *self);
static void stop_mic_test(QtelPreferences *self);

static void
on_proxy_enable_changed(GObject *object, GParamSpec *pspec, gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);
  gboolean enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->proxy_enable_switch));

  gtk_widget_set_sensitive(self->proxy_server_entry, enabled);
  gtk_widget_set_sensitive(self->proxy_port_spin, enabled);
  gtk_widget_set_sensitive(self->proxy_password_entry, enabled);
}

static AdwPreferencesPage *
create_user_page(QtelPreferences *self)
{
  AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(page, "User");
  adw_preferences_page_set_icon_name(page, "user-info-symbolic");

  // User info group
  AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(group, "User Information");
  adw_preferences_group_set_description(group,
    "Enter your EchoLink registration information");

  // Callsign
  self->callsign_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->callsign_entry), "Callsign");
  g_settings_bind(self->settings, "callsign",
                  self->callsign_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(group, self->callsign_entry);

  // Name
  self->name_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->name_entry), "Name");
  g_settings_bind(self->settings, "name",
                  self->name_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(group, self->name_entry);

  // Password
  self->password_entry = adw_password_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->password_entry), "Password");
  g_settings_bind(self->settings, "password",
                  self->password_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(group, self->password_entry);

  // Confirm Password (not bound to settings, just for validation)
  self->confirm_password_entry = adw_password_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->confirm_password_entry),
                                 "Confirm Password");
  adw_preferences_group_add(group, self->confirm_password_entry);

  // Location
  self->location_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->location_entry), "Location");
  g_settings_bind(self->settings, "location",
                  self->location_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(group, self->location_entry);

  // Info message
  self->info_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->info_entry), "Info Message");
  g_settings_bind(self->settings, "info",
                  self->info_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(group, self->info_entry);

  adw_preferences_page_add(page, group);

  return page;
}

static AdwPreferencesPage *
create_network_page(QtelPreferences *self)
{
  AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(page, "Network");
  adw_preferences_page_set_icon_name(page, "network-server-symbolic");

  // Directory servers group
  AdwPreferencesGroup *dir_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(dir_group, "Directory Server");

  // Servers
  self->servers_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->servers_entry), "Servers");
  g_settings_bind(self->settings, "directory-servers",
                  self->servers_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(dir_group, self->servers_entry);

  // Refresh time
  self->refresh_time_spin = adw_spin_row_new_with_range(1, 60, 1);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->refresh_time_spin),
                                 "Refresh Time (minutes)");
  g_settings_bind(self->settings, "list-refresh-time",
                  self->refresh_time_spin, "value", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(dir_group, self->refresh_time_spin);

  // Start as busy
  self->start_busy_switch = adw_switch_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->start_busy_switch),
                                 "Start as Busy");
  g_settings_bind(self->settings, "start-as-busy",
                  self->start_busy_switch, "active", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(dir_group, self->start_busy_switch);

  // Bind address
  self->bind_address_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->bind_address_entry),
                                 "Bind Address");
  g_settings_bind(self->settings, "bind-address",
                  self->bind_address_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(dir_group, self->bind_address_entry);

  adw_preferences_page_add(page, dir_group);

  // Proxy group
  AdwPreferencesGroup *proxy_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(proxy_group, "Proxy");

  // Enable proxy
  self->proxy_enable_switch = adw_switch_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->proxy_enable_switch),
                                 "Enable Proxy");
  g_settings_bind(self->settings, "proxy-enabled",
                  self->proxy_enable_switch, "active", G_SETTINGS_BIND_DEFAULT);
  g_signal_connect(self->proxy_enable_switch, "notify::active",
                   G_CALLBACK(on_proxy_enable_changed), self);
  adw_preferences_group_add(proxy_group, self->proxy_enable_switch);

  // Proxy server
  self->proxy_server_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->proxy_server_entry),
                                 "Server");
  gtk_widget_set_sensitive(self->proxy_server_entry, FALSE);
  g_settings_bind(self->settings, "proxy-server",
                  self->proxy_server_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(proxy_group, self->proxy_server_entry);

  // Proxy port
  self->proxy_port_spin = adw_spin_row_new_with_range(1, 65535, 1);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->proxy_port_spin), "Port");
  gtk_widget_set_sensitive(self->proxy_port_spin, FALSE);
  g_settings_bind(self->settings, "proxy-port",
                  self->proxy_port_spin, "value", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(proxy_group, self->proxy_port_spin);

  // Proxy password
  self->proxy_password_entry = adw_password_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->proxy_password_entry),
                                 "Password");
  gtk_widget_set_sensitive(self->proxy_password_entry, FALSE);
  g_settings_bind(self->settings, "proxy-password",
                  self->proxy_password_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(proxy_group, self->proxy_password_entry);

  adw_preferences_page_add(page, proxy_group);

  return page;
}

// Common audio device options (svxlink uses "alsa:" prefix for ALSA devices)
// Static arrays so they persist for callbacks
static const char *audio_devices[] = {
  "alsa:default",      // System default - works on most systems
  "alsa:pulse",        // PulseAudio - recommended for desktop Linux
  "alsa:pipewire",     // PipeWire - modern systems
  "alsa:hw:0,0",       // First hardware device
  "alsa:plughw:0,0",   // First device with automatic conversion
  "alsa:hw:1,0",       // Second hardware device
  "alsa:plughw:1,0",   // Second device with automatic conversion
  NULL
};

// Friendly display names for the devices
static const char *audio_device_names[] = {
  "System Default (alsa:default)",
  "PulseAudio (alsa:pulse)",
  "PipeWire (alsa:pipewire)",
  "Hardware Device 0 (alsa:hw:0,0)",
  "Hardware Device 0 with conversion (alsa:plughw:0,0)",
  "Hardware Device 1 (alsa:hw:1,0)",
  "Hardware Device 1 with conversion (alsa:plughw:1,0)",
  NULL
};

// Helper to find index of audio device in list
static guint
find_audio_device_index(const char **devices, const char *current)
{
  if (current == nullptr || current[0] == '\0')
    return 0;  // Default to first (System Default)

  for (guint i = 0; devices[i] != nullptr; i++)
  {
    if (g_strcmp0(devices[i], current) == 0)
      return i;
  }
  return 0;  // Default to first if not found
}

// Callback when mic device changes
static void
on_mic_device_changed(GObject *combo, GParamSpec *pspec, gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);
  guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(combo));
  // Use the actual device value, not the display name
  const char *device = audio_devices[selected];
  g_settings_set_string(self->settings, "mic-audio-device", device);
}

// Callback when speaker device changes
static void
on_spkr_device_changed(GObject *combo, GParamSpec *pspec, gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);
  guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(combo));
  // Use the actual device value, not the display name
  const char *device = audio_devices[selected];
  g_settings_set_string(self->settings, "spkr-audio-device", device);
}

// Stop speaker test and clean up
static void
stop_speaker_test(QtelPreferences *self)
{
  if (self->tone_gen)
  {
    self->tone_gen->stop();
    delete self->tone_gen;
    self->tone_gen = nullptr;
  }
  if (self->test_spkr_audio)
  {
    self->test_spkr_audio->close();
    delete self->test_spkr_audio;
    self->test_spkr_audio = nullptr;
  }
  gtk_button_set_label(GTK_BUTTON(self->spkr_test_button), "Test");
}

// Callback when speaker test button is clicked
static void
on_spkr_test_clicked(GtkButton *button, gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);

  // If already testing, stop
  if (self->test_spkr_audio != nullptr)
  {
    stop_speaker_test(self);
    return;
  }

  // Get current speaker device
  guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(self->spkr_device_entry));
  const char *device = audio_devices[selected];

  g_message("Testing speaker device: %s", device);

  // Create audio output
  self->test_spkr_audio = new AudioIO(device, 0);
  if (!self->test_spkr_audio->open(AudioIO::MODE_WR))
  {
    g_warning("Failed to open audio device %s for testing", device);
    GtkWidget *dialog = adw_message_dialog_new(
      GTK_WINDOW(self),
      "Audio Test Failed",
      nullptr);
    adw_message_dialog_format_body(ADW_MESSAGE_DIALOG(dialog),
      "Could not open audio device:\n%s\n\nPlease check the device is available.",
      device);
    adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "ok", "OK");
    gtk_window_present(GTK_WINDOW(dialog));
    delete self->test_spkr_audio;
    self->test_spkr_audio = nullptr;
    return;
  }

  // Create tone generator and connect to audio output
  self->tone_gen = new ToneGenerator(440, 0.3f);  // 440 Hz, 30% volume
  self->tone_gen->registerSink(self->test_spkr_audio);

  // Update button label
  gtk_button_set_label(GTK_BUTTON(button), "Stop");

  // Play 2 second test tone
  self->tone_gen->start(2000);

  // Schedule stop after 2 seconds
  g_timeout_add(2100, [](gpointer data) -> gboolean {
    QtelPreferences *self = QTEL_PREFERENCES(data);
    if (self->test_spkr_audio)
      stop_speaker_test(self);
    return G_SOURCE_REMOVE;
  }, self);
}

// Stop microphone test and clean up
static void
stop_mic_test(QtelPreferences *self)
{
  self->mic_testing = false;

  if (self->mic_test_timeout_id > 0)
  {
    g_source_remove(self->mic_test_timeout_id);
    self->mic_test_timeout_id = 0;
  }
  if (self->test_mic_audio)
  {
    self->test_mic_audio->close();
    delete self->test_mic_audio;
    self->test_mic_audio = nullptr;
  }
  if (self->level_meter)
  {
    delete self->level_meter;
    self->level_meter = nullptr;
  }
  gtk_level_bar_set_value(GTK_LEVEL_BAR(self->mic_level_bar), 0.0);
  gtk_button_set_label(GTK_BUTTON(self->mic_test_button), "Test");
}

// Update microphone level display
static gboolean
update_mic_level(gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);

  if (!self->mic_testing || !self->level_meter)
  {
    self->mic_test_timeout_id = 0;
    return G_SOURCE_REMOVE;
  }

  // Get level and display (level is 0.0-1.0, scale for level bar)
  float level = self->level_meter->level();
  // Convert to dB-like scale for better visual feedback
  float display_level = level * 3.0f;  // Amplify for visibility
  if (display_level > 1.0f)
    display_level = 1.0f;

  gtk_level_bar_set_value(GTK_LEVEL_BAR(self->mic_level_bar), display_level);

  return G_SOURCE_CONTINUE;
}

// Callback when microphone test button is clicked
static void
on_mic_test_clicked(GtkButton *button, gpointer user_data)
{
  QtelPreferences *self = QTEL_PREFERENCES(user_data);

  // If already testing, stop
  if (self->mic_testing)
  {
    stop_mic_test(self);
    return;
  }

  // Get current mic device
  guint selected = adw_combo_row_get_selected(ADW_COMBO_ROW(self->mic_device_entry));
  const char *device = audio_devices[selected];

  g_message("Testing microphone device: %s", device);

  // Create audio input
  self->test_mic_audio = new AudioIO(device, 0);
  if (!self->test_mic_audio->open(AudioIO::MODE_RD))
  {
    g_warning("Failed to open audio device %s for testing", device);
    GtkWidget *dialog = adw_message_dialog_new(
      GTK_WINDOW(self),
      "Audio Test Failed",
      nullptr);
    adw_message_dialog_format_body(ADW_MESSAGE_DIALOG(dialog),
      "Could not open audio device:\n%s\n\nPlease check the device is available.",
      device);
    adw_message_dialog_add_response(ADW_MESSAGE_DIALOG(dialog), "ok", "OK");
    gtk_window_present(GTK_WINDOW(dialog));
    delete self->test_mic_audio;
    self->test_mic_audio = nullptr;
    return;
  }

  // Create level meter and connect to audio input
  self->level_meter = new LevelMeter();
  self->test_mic_audio->registerSink(self->level_meter);

  self->mic_testing = true;

  // Update button label
  gtk_button_set_label(GTK_BUTTON(button), "Stop");

  // Start periodic level update
  self->mic_test_timeout_id = g_timeout_add(50, update_mic_level, self);
}

static AdwPreferencesPage *
create_audio_page(QtelPreferences *self)
{
  AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(page, "Audio");
  adw_preferences_page_set_icon_name(page, "audio-card-symbolic");

  // Devices group
  AdwPreferencesGroup *devices_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(devices_group, "Audio Devices");
  adw_preferences_group_set_description(devices_group,
    "Select audio devices for microphone input and speaker output. "
    "PulseAudio or PipeWire is recommended for best compatibility.");

  // Microphone device dropdown
  GtkStringList *mic_model = gtk_string_list_new(audio_device_names);
  self->mic_device_entry = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(self->mic_device_entry),
                           G_LIST_MODEL(mic_model));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->mic_device_entry),
                                 "Microphone");
  adw_action_row_set_subtitle(ADW_ACTION_ROW(self->mic_device_entry),
                               "Audio input device for transmitting");

  // Find and select current mic device
  const gchar *current_mic = g_settings_get_string(self->settings, "mic-audio-device");
  guint mic_index = find_audio_device_index(audio_devices, current_mic);
  adw_combo_row_set_selected(ADW_COMBO_ROW(self->mic_device_entry), mic_index);

  g_signal_connect(self->mic_device_entry, "notify::selected",
                   G_CALLBACK(on_mic_device_changed), self);
  adw_preferences_group_add(devices_group, self->mic_device_entry);

  // Speaker device dropdown
  GtkStringList *spkr_model = gtk_string_list_new(audio_device_names);
  self->spkr_device_entry = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(self->spkr_device_entry),
                           G_LIST_MODEL(spkr_model));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->spkr_device_entry),
                                 "Speaker");
  adw_action_row_set_subtitle(ADW_ACTION_ROW(self->spkr_device_entry),
                               "Audio output device for receiving");

  // Find and select current speaker device
  const gchar *current_spkr = g_settings_get_string(self->settings, "spkr-audio-device");
  guint spkr_index = find_audio_device_index(audio_devices, current_spkr);
  adw_combo_row_set_selected(ADW_COMBO_ROW(self->spkr_device_entry), spkr_index);

  g_signal_connect(self->spkr_device_entry, "notify::selected",
                   G_CALLBACK(on_spkr_device_changed), self);
  adw_preferences_group_add(devices_group, self->spkr_device_entry);

  // Sample rate dropdown
  const char *sample_rate_names[] = { "16 kHz (lower quality, less bandwidth)", "48 kHz (higher quality)", NULL };
  const char *sample_rates[] = { "16000", "48000", NULL };
  GtkStringList *rate_model = gtk_string_list_new(sample_rate_names);
  self->sample_rate_dropdown = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(self->sample_rate_dropdown),
                           G_LIST_MODEL(rate_model));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->sample_rate_dropdown),
                                 "Sample Rate");
  // Manual binding for sample rate since it's stored as int
  gint rate = g_settings_get_int(self->settings, "card-sample-rate");
  adw_combo_row_set_selected(ADW_COMBO_ROW(self->sample_rate_dropdown),
                              rate == 48000 ? 1 : 0);
  adw_preferences_group_add(devices_group, self->sample_rate_dropdown);

  // Full duplex
  self->full_duplex_switch = adw_switch_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->full_duplex_switch),
                                 "Full Duplex");
  adw_action_row_set_subtitle(ADW_ACTION_ROW(self->full_duplex_switch),
                               "Allow simultaneous transmit and receive (requires compatible hardware)");
  g_settings_bind(self->settings, "use-full-duplex",
                  self->full_duplex_switch, "active", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(devices_group, self->full_duplex_switch);

  // Connect sound (optional file path - shown via title only since AdwEntryRow doesn't support subtitles)
  self->connect_sound_entry = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->connect_sound_entry),
                                 "Connect Sound File (optional)");
  g_settings_bind(self->settings, "connect-sound",
                  self->connect_sound_entry, "text", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(devices_group, self->connect_sound_entry);

  adw_preferences_page_add(page, devices_group);

  // Audio Test group
  AdwPreferencesGroup *test_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(test_group, "Audio Test");
  adw_preferences_group_set_description(test_group,
    "Test your audio devices to verify they work correctly.");

  // Speaker test row
  AdwActionRow *spkr_test_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(spkr_test_row), "Speaker Test");
  adw_action_row_set_subtitle(spkr_test_row, "Play a 440 Hz test tone");
  self->spkr_test_button = gtk_button_new_with_label("Test");
  gtk_widget_set_valign(self->spkr_test_button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(self->spkr_test_button, "suggested-action");
  g_signal_connect(self->spkr_test_button, "clicked",
                   G_CALLBACK(on_spkr_test_clicked), self);
  adw_action_row_add_suffix(spkr_test_row, self->spkr_test_button);
  adw_preferences_group_add(test_group, GTK_WIDGET(spkr_test_row));

  // Microphone test row with level meter
  AdwActionRow *mic_test_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(mic_test_row), "Microphone Test");
  adw_action_row_set_subtitle(mic_test_row, "Check input level (speak into mic)");

  // Create horizontal box for level meter and button
  GtkWidget *mic_test_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_valign(mic_test_box, GTK_ALIGN_CENTER);

  // Level bar
  self->mic_level_bar = gtk_level_bar_new_for_interval(0.0, 1.0);
  gtk_widget_set_size_request(self->mic_level_bar, 100, -1);
  gtk_widget_set_valign(self->mic_level_bar, GTK_ALIGN_CENTER);
  gtk_level_bar_set_value(GTK_LEVEL_BAR(self->mic_level_bar), 0.0);
  gtk_box_append(GTK_BOX(mic_test_box), self->mic_level_bar);

  // Test button
  self->mic_test_button = gtk_button_new_with_label("Test");
  gtk_widget_add_css_class(self->mic_test_button, "suggested-action");
  g_signal_connect(self->mic_test_button, "clicked",
                   G_CALLBACK(on_mic_test_clicked), self);
  gtk_box_append(GTK_BOX(mic_test_box), self->mic_test_button);

  adw_action_row_add_suffix(mic_test_row, mic_test_box);
  adw_preferences_group_add(test_group, GTK_WIDGET(mic_test_row));

  adw_preferences_page_add(page, test_group);

  // VOX group
  AdwPreferencesGroup *vox_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(vox_group, "VOX (Voice Operated Transmission)");

  // VOX enable
  self->vox_enable_switch = adw_switch_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->vox_enable_switch),
                                 "Enable VOX");
  g_settings_bind(self->settings, "vox-enabled",
                  self->vox_enable_switch, "active", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(vox_group, self->vox_enable_switch);

  // VOX threshold
  self->vox_threshold_spin = adw_spin_row_new_with_range(-60, 0, 1);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->vox_threshold_spin),
                                 "Threshold (dB)");
  g_settings_bind(self->settings, "vox-threshold",
                  self->vox_threshold_spin, "value", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(vox_group, self->vox_threshold_spin);

  // VOX delay
  self->vox_delay_spin = adw_spin_row_new_with_range(0, 3000, 100);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->vox_delay_spin),
                                 "Delay (ms)");
  g_settings_bind(self->settings, "vox-delay",
                  self->vox_delay_spin, "value", G_SETTINGS_BIND_DEFAULT);
  adw_preferences_group_add(vox_group, self->vox_delay_spin);

  adw_preferences_page_add(page, vox_group);

  return page;
}

static AdwPreferencesPage *
create_qso_page(QtelPreferences *self)
{
  AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
  adw_preferences_page_set_title(page, "QSO");
  adw_preferences_page_set_icon_name(page, "chat-symbolic");

  // Chat group
  AdwPreferencesGroup *chat_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_group_set_title(chat_group, "Chat Settings");

  // Chat encoding dropdown
  const char *encodings[] = {
    "UTF-8", "ISO-8859-1", "ISO-8859-15", "Windows-1252", NULL
  };
  GtkStringList *encoding_model = gtk_string_list_new(encodings);
  self->chat_encoding_dropdown = adw_combo_row_new();
  adw_combo_row_set_model(ADW_COMBO_ROW(self->chat_encoding_dropdown),
                           G_LIST_MODEL(encoding_model));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self->chat_encoding_dropdown),
                                 "Chat Encoding");
  adw_action_row_set_subtitle(ADW_ACTION_ROW(self->chat_encoding_dropdown),
                               "Character encoding for chat messages");

  // Find current encoding in list
  const gchar *current = g_settings_get_string(self->settings, "chat-encoding");
  guint selected = 0;
  for (guint i = 0; encodings[i] != NULL; i++)
  {
    if (g_strcmp0(encodings[i], current) == 0)
    {
      selected = i;
      break;
    }
  }
  adw_combo_row_set_selected(ADW_COMBO_ROW(self->chat_encoding_dropdown), selected);

  adw_preferences_group_add(chat_group, self->chat_encoding_dropdown);

  adw_preferences_page_add(page, chat_group);

  return page;
}

static void
qtel_preferences_finalize(GObject *object)
{
  QtelPreferences *self = QTEL_PREFERENCES(object);

  // Stop any ongoing audio tests
  stop_speaker_test(self);
  stop_mic_test(self);

  g_clear_object(&self->settings);

  G_OBJECT_CLASS(qtel_preferences_parent_class)->finalize(object);
}

static void
qtel_preferences_class_init(QtelPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = qtel_preferences_finalize;
}

static void
qtel_preferences_init(QtelPreferences *self)
{
  self->settings = g_settings_new(APP_ID);

  gtk_window_set_default_size(GTK_WINDOW(self), 600, 700);

  // Add pages
  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(self), create_user_page(self));
  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(self), create_network_page(self));
  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(self), create_audio_page(self));
  adw_preferences_window_add(ADW_PREFERENCES_WINDOW(self), create_qso_page(self));

  // Update proxy sensitivity based on current setting
  on_proxy_enable_changed(NULL, NULL, self);
}

QtelPreferences *
qtel_preferences_new(GtkWindow *parent)
{
  return static_cast<QtelPreferences*>(
    g_object_new(QTEL_TYPE_PREFERENCES,
                 "transient-for", parent,
                 "modal", TRUE,
                 nullptr)
  );
}
