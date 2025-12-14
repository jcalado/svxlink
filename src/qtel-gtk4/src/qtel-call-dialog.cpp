/**
@file    qtel-call-dialog.cpp
@brief   Communication dialog for Qtel GTK4
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

#include "qtel-call-dialog.h"
#include "vox.h"
#include "settings.h"
#include <cstring>
#include <string>

// Include EchoLink and Async headers
#include <EchoLinkQso.h>
#include <AsyncAudioIO.h>
#include <AsyncAudioFifo.h>
#include <AsyncAudioValve.h>
#include <AsyncAudioSplitter.h>
#include <AsyncAudioDecimator.h>
#include <AsyncAudioInterpolator.h>
#include <AsyncDnsLookup.h>
#include <AsyncIpAddress.h>

// Multirate filter coefficients (from svxlink qtel)
#include "multirate_filter_coeff.h"

// Define INTERNAL_SAMPLE_RATE if not defined by build system
#ifndef INTERNAL_SAMPLE_RATE
#define INTERNAL_SAMPLE_RATE 16000
#endif

using namespace Async;
using namespace EchoLink;
using namespace std;

/**
 * Connection state enum
 */
typedef enum {
  CONNECTION_STATE_DISCONNECTED,
  CONNECTION_STATE_CONNECTING,
  CONNECTION_STATE_CONNECTED,
  CONNECTION_STATE_BYE_RECEIVED
} ConnectionState;

struct _QtelCallDialog
{
  AdwWindow parent_instance;

  // Station info
  gchar *callsign;
  gchar *description;
  gint node_id;
  gchar *ip_address;

  // Connection state
  ConnectionState state;
  gboolean is_transmitting;
  gboolean is_receiving;
  gboolean accept_connection;
  gboolean ctrl_pressed;
  gboolean audio_full_duplex;

  // Header widgets
  GtkWidget *header_bar;
  GtkWidget *connect_button;
  GtkWidget *disconnect_button;

  // Station info widgets
  GtkWidget *callsign_label;
  GtkWidget *description_label;
  GtkWidget *status_label;
  GtkWidget *ip_label;
  GtkWidget *time_label;

  // Chat widgets
  GtkWidget *chat_view;
  GtkWidget *info_view;
  GtkWidget *chat_entry;
  GtkTextBuffer *chat_buffer;
  GtkTextBuffer *info_buffer;

  // Indicator widgets
  GtkWidget *rx_indicator;
  GtkWidget *tx_indicator;

  // PTT widgets
  GtkWidget *ptt_button;
  gboolean ptt_toggle_mode;

  // VOX widgets
  GtkWidget *vox_enable_switch;
  GtkWidget *vox_threshold_scale;
  GtkWidget *vox_delay_spin;
  GtkWidget *vox_level_bar;
  GtkWidget *vox_indicator;

  // VOX controller
  Vox *vox;

  // EchoLink QSO connection
  Qso *qso;

  // DNS lookup for host connections
  DnsLookup *dns;

  // Audio devices
  AudioIO *mic_audio_io;
  AudioIO *spkr_audio_io;

  // Audio pipeline components
  AudioFifo *rem_audio_fifo;
  AudioValve *rem_audio_valve;
  AudioValve *ptt_valve;
  AudioSplitter *tx_audio_splitter;

  // Store the IP for creating connection
  IpAddress station_ip;
};

G_DEFINE_TYPE(QtelCallDialog, qtel_call_dialog, ADW_TYPE_WINDOW)

// Forward declarations
static void update_ui_for_state(QtelCallDialog *self);
static void set_transmitting(QtelCallDialog *self, gboolean transmit);
static void check_transmit(QtelCallDialog *self);
static void set_receiving(QtelCallDialog *self, gboolean receiving);
static void create_connection(QtelCallDialog *self, const IpAddress &ip);
static bool open_audio_device(QtelCallDialog *self, AudioIO::Mode mode);
static void init_audio_pipeline(QtelCallDialog *self);
static void append_info(QtelCallDialog *self, const gchar *text);
static void append_chat(QtelCallDialog *self, const gchar *text);

// EchoLink signal handlers (C++ callbacks)
static void on_qso_state_change(Qso::State state, QtelCallDialog *self);
static void on_qso_chat_msg_received(const string &msg, QtelCallDialog *self);
static void on_qso_info_msg_received(const string &msg, QtelCallDialog *self);
static void on_qso_is_receiving(bool is_receiving, QtelCallDialog *self);
static void on_dns_results_ready(DnsLookup &dns, QtelCallDialog *self);

static void
on_connect_clicked(GtkButton *button, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);

  if (self->qso == nullptr)
  {
    g_warning("No QSO object - cannot connect");
    return;
  }

  g_message("Connecting to %s...", self->callsign);
  self->qso->connect();
}

static void
on_disconnect_clicked(GtkButton *button, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);

  if (self->qso == nullptr)
  {
    return;
  }

  g_message("Disconnecting from %s", self->callsign);
  self->qso->disconnect();
}

static void
on_ptt_pressed(GtkGestureClick *gesture, int n_press, double x, double y,
               gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);

  if (self->ptt_toggle_mode)
    return;

  check_transmit(self);
}

static void
on_ptt_released(GtkGestureClick *gesture, int n_press, double x, double y,
                gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);

  if (self->ptt_toggle_mode)
    return;

  check_transmit(self);
}

static void
on_ptt_toggled(GtkToggleButton *button, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  check_transmit(self);
}

static void
on_chat_entry_activate(GtkEntry *entry, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  if (text == nullptr || text[0] == '\0')
    return;

  if (self->qso == nullptr)
    return;

  // Get callsign from settings
  Settings *settings = settings_get_default();
  const gchar *my_callsign = settings_get_callsign(settings);

  // Add to chat view
  gchar *msg = g_strdup_printf("%s> %s\n", my_callsign, text);
  append_chat(self, msg);
  g_free(msg);

  // Send via EchoLink Qso
  self->qso->sendChatData(text);

  // Clear entry
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Focus PTT button
  gtk_widget_grab_focus(self->ptt_button);
}

static void
on_vox_enabled_changed(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  gboolean enabled = gtk_switch_get_active(sw);
  self->vox->setEnabled(enabled);

  // Update sensitivity of VOX controls
  gtk_widget_set_sensitive(self->vox_threshold_scale, enabled);
  gtk_widget_set_sensitive(self->vox_delay_spin, enabled);
}

static void
on_vox_threshold_changed(GtkRange *range, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  int threshold = static_cast<int>(gtk_range_get_value(range));
  self->vox->setThreshold(threshold);
}

static void
on_vox_delay_changed(GtkSpinButton *spin, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  int delay = gtk_spin_button_get_value_as_int(spin);
  self->vox->setDelay(delay);
}

static void
on_vox_level_changed(int level_db, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);
  // Convert dB to fraction (0-1) for level bar
  // -60dB = 0, 0dB = 1
  double fraction = (level_db + 60.0) / 60.0;
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  gtk_level_bar_set_value(GTK_LEVEL_BAR(self->vox_level_bar), fraction);
}

static void
on_vox_state_changed(VoxState state, gpointer user_data)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(user_data);

  // Update VOX indicator color
  const gchar *css_class = nullptr;
  switch (state)
  {
    case VoxState::IDLE:
      css_class = "indicator-idle";
      break;
    case VoxState::ACTIVE:
      css_class = "indicator-tx";
      break;
    case VoxState::HANG:
      css_class = "indicator-hang";
      break;
  }

  // Remove old classes and add new one
  gtk_widget_remove_css_class(self->vox_indicator, "indicator-idle");
  gtk_widget_remove_css_class(self->vox_indicator, "indicator-tx");
  gtk_widget_remove_css_class(self->vox_indicator, "indicator-hang");
  if (css_class != nullptr)
  {
    gtk_widget_add_css_class(self->vox_indicator, css_class);
  }

  check_transmit(self);
}

// EchoLink callback: QSO state changed
static void
on_qso_state_change(Qso::State state, QtelCallDialog *self)
{
  g_message("QSO state changed: %d", static_cast<int>(state));

  switch (state)
  {
    case Qso::STATE_CONNECTED:
      self->state = CONNECTION_STATE_CONNECTED;
      append_info(self, "Connected\n");
      if (self->qso != nullptr)
      {
        // Update remote name if available
        string remote_name = self->qso->remoteName();
        if (!remote_name.empty())
        {
          gtk_label_set_text(GTK_LABEL(self->description_label), remote_name.c_str());
        }
      }
      break;

    case Qso::STATE_CONNECTING:
      self->state = CONNECTION_STATE_CONNECTING;
      append_info(self, "Connecting...\n");
      break;

    case Qso::STATE_BYE_RECEIVED:
      self->state = CONNECTION_STATE_BYE_RECEIVED;
      append_info(self, "Bye received\n");
      break;

    case Qso::STATE_DISCONNECTED:
      self->state = CONNECTION_STATE_DISCONNECTED;
      append_info(self, "Disconnected\n");
      break;
  }

  update_ui_for_state(self);
  check_transmit(self);
}

// EchoLink callback: Chat message received
static void
on_qso_chat_msg_received(const string &msg, QtelCallDialog *self)
{
  append_chat(self, msg.c_str());
  append_chat(self, "\n");
}

// EchoLink callback: Info message received
static void
on_qso_info_msg_received(const string &msg, QtelCallDialog *self)
{
  append_info(self, "------------ INFO ------------\n");
  append_info(self, msg.c_str());
  append_info(self, "\n------------------------------\n");
}

// EchoLink callback: RX state changed
static void
on_qso_is_receiving(bool is_receiving, QtelCallDialog *self)
{
  set_receiving(self, is_receiving ? TRUE : FALSE);
}

// DNS lookup results ready
static void
on_dns_results_ready(DnsLookup &dns, QtelCallDialog *self)
{
  if (dns.addresses().empty())
  {
    append_info(self, "DNS lookup failed - no addresses found\n");
    g_warning("DNS lookup failed for %s", dns.label().c_str());
    return;
  }

  IpAddress ip = dns.addresses()[0];
  g_message("DNS resolved to %s", ip.toString().c_str());

  // Update IP label
  gtk_label_set_text(GTK_LABEL(self->ip_label), ip.toString().c_str());

  // Create connection
  create_connection(self, ip);
}

static void
check_transmit(QtelCallDialog *self)
{
  gboolean should_transmit =
    (self->state == CONNECTION_STATE_CONNECTED) &&
    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->ptt_button)) ||
     (self->vox->state() != VoxState::IDLE));

  set_transmitting(self, should_transmit);
}

static void
set_transmitting(QtelCallDialog *self, gboolean transmit)
{
  if (self->is_transmitting == transmit)
    return;

  self->is_transmitting = transmit;

  // Update TX indicator
  if (transmit)
  {
    gtk_widget_remove_css_class(self->tx_indicator, "indicator-idle");
    gtk_widget_add_css_class(self->tx_indicator, "indicator-tx");

    if (!self->audio_full_duplex)
    {
      // Half duplex: switch to TX mode
      if (self->rem_audio_valve != nullptr)
        self->rem_audio_valve->setOpen(false);
      if (self->mic_audio_io != nullptr)
        self->mic_audio_io->close();
      if (self->spkr_audio_io != nullptr)
        self->spkr_audio_io->close();
      open_audio_device(self, AudioIO::MODE_RD);
    }

    if (self->ptt_valve != nullptr)
      self->ptt_valve->setOpen(true);
  }
  else
  {
    gtk_widget_remove_css_class(self->tx_indicator, "indicator-tx");
    gtk_widget_add_css_class(self->tx_indicator, "indicator-idle");

    if (self->ptt_valve != nullptr)
      self->ptt_valve->setOpen(false);

    if (!self->audio_full_duplex)
    {
      // Half duplex: switch to RX mode
      if (self->mic_audio_io != nullptr)
        self->mic_audio_io->close();
      if (self->spkr_audio_io != nullptr)
        self->spkr_audio_io->close();
      open_audio_device(self, AudioIO::MODE_WR);
      if (self->rem_audio_valve != nullptr)
        self->rem_audio_valve->setOpen(true);
    }
  }

  g_message("TX: %s", transmit ? "ON" : "OFF");
}

static void
set_receiving(QtelCallDialog *self, gboolean receiving)
{
  if (self->is_receiving == receiving)
    return;

  self->is_receiving = receiving;

  // Update RX indicator
  if (receiving)
  {
    gtk_widget_remove_css_class(self->rx_indicator, "indicator-idle");
    gtk_widget_add_css_class(self->rx_indicator, "indicator-rx");
  }
  else
  {
    gtk_widget_remove_css_class(self->rx_indicator, "indicator-rx");
    gtk_widget_add_css_class(self->rx_indicator, "indicator-idle");
  }
}

static void
update_ui_for_state(QtelCallDialog *self)
{
  switch (self->state)
  {
    case CONNECTION_STATE_DISCONNECTED:
      gtk_widget_set_sensitive(self->connect_button, self->qso != nullptr);
      gtk_widget_set_sensitive(self->disconnect_button, FALSE);
      gtk_widget_set_sensitive(self->ptt_button, FALSE);
      gtk_widget_set_sensitive(self->chat_entry, FALSE);
      gtk_label_set_text(GTK_LABEL(self->status_label), "Disconnected");
      set_transmitting(self, FALSE);
      set_receiving(self, FALSE);
      break;

    case CONNECTION_STATE_CONNECTING:
      gtk_widget_set_sensitive(self->connect_button, FALSE);
      gtk_widget_set_sensitive(self->disconnect_button, TRUE);
      gtk_widget_set_sensitive(self->ptt_button, FALSE);
      gtk_widget_set_sensitive(self->chat_entry, FALSE);
      gtk_label_set_text(GTK_LABEL(self->status_label), "Connecting...");
      break;

    case CONNECTION_STATE_CONNECTED:
      gtk_widget_set_sensitive(self->connect_button, FALSE);
      gtk_widget_set_sensitive(self->disconnect_button, TRUE);
      gtk_widget_set_sensitive(self->ptt_button, TRUE);
      gtk_widget_set_sensitive(self->chat_entry, TRUE);
      gtk_label_set_text(GTK_LABEL(self->status_label), "Connected");
      gtk_widget_grab_focus(self->ptt_button);
      break;

    case CONNECTION_STATE_BYE_RECEIVED:
      gtk_widget_set_sensitive(self->connect_button, FALSE);
      gtk_widget_set_sensitive(self->disconnect_button, FALSE);
      gtk_widget_set_sensitive(self->ptt_button, FALSE);
      gtk_widget_set_sensitive(self->chat_entry, FALSE);
      gtk_label_set_text(GTK_LABEL(self->status_label), "Disconnecting...");
      set_transmitting(self, FALSE);
      break;
  }
}

// Convert text to valid UTF-8, replacing invalid sequences
static gchar *
ensure_utf8(const gchar *text)
{
  if (text == nullptr)
    return nullptr;

  // If already valid UTF-8, just duplicate
  if (g_utf8_validate(text, -1, nullptr))
    return g_strdup(text);

  // Try to convert from ISO-8859-1 (common for EchoLink)
  GError *error = nullptr;
  gchar *utf8 = g_convert(text, -1, "UTF-8", "ISO-8859-1", nullptr, nullptr, &error);
  if (utf8 != nullptr)
  {
    g_clear_error(&error);
    return utf8;
  }
  g_clear_error(&error);

  // Fallback: replace invalid bytes with replacement character
  GString *result = g_string_new(nullptr);
  const gchar *p = text;
  while (*p)
  {
    gunichar ch = g_utf8_get_char_validated(p, -1);
    if (ch == (gunichar)-1 || ch == (gunichar)-2)
    {
      // Invalid byte, replace with replacement character
      g_string_append(result, "\xEF\xBF\xBD"); // U+FFFD
      p++;
    }
    else
    {
      gchar buf[6];
      gint len = g_unichar_to_utf8(ch, buf);
      g_string_append_len(result, buf, len);
      p = g_utf8_next_char(p);
    }
  }
  return g_string_free(result, FALSE);
}

static void
append_info(QtelCallDialog *self, const gchar *text)
{
  gchar *utf8_text = ensure_utf8(text);
  if (utf8_text == nullptr)
    return;

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(self->info_buffer, &end);
  gtk_text_buffer_insert(self->info_buffer, &end, utf8_text, -1);

  // Scroll to end
  gtk_text_buffer_get_end_iter(self->info_buffer, &end);
  GtkTextMark *mark = gtk_text_buffer_create_mark(self->info_buffer, nullptr, &end, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(self->info_view), mark, 0.0, FALSE, 0.0, 0.0);
  gtk_text_buffer_delete_mark(self->info_buffer, mark);

  g_free(utf8_text);
}

static void
append_chat(QtelCallDialog *self, const gchar *text)
{
  gchar *utf8_text = ensure_utf8(text);
  if (utf8_text == nullptr)
    return;

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(self->chat_buffer, &end);
  gtk_text_buffer_insert(self->chat_buffer, &end, utf8_text, -1);

  // Scroll to end
  gtk_text_buffer_get_end_iter(self->chat_buffer, &end);
  GtkTextMark *mark = gtk_text_buffer_create_mark(self->chat_buffer, nullptr, &end, FALSE);
  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(self->chat_view), mark, 0.0, FALSE, 0.0, 0.0);
  gtk_text_buffer_delete_mark(self->chat_buffer, mark);

  g_free(utf8_text);
}

static bool
open_audio_device(QtelCallDialog *self, AudioIO::Mode mode)
{
  bool mic_open_ok = true;
  bool spkr_open_ok = true;

  if ((mode == AudioIO::MODE_RD) || (mode == AudioIO::MODE_RDWR))
  {
    if (self->mic_audio_io != nullptr)
    {
      mic_open_ok = self->mic_audio_io->open(AudioIO::MODE_RD);
      if (!mic_open_ok)
      {
        g_warning("Could not open mic audio device");
      }
    }
  }

  if ((mode == AudioIO::MODE_WR) || (mode == AudioIO::MODE_RDWR))
  {
    if (self->spkr_audio_io != nullptr)
    {
      spkr_open_ok = self->spkr_audio_io->open(AudioIO::MODE_WR);
      if (!spkr_open_ok)
      {
        g_warning("Could not open speaker audio device");
      }
    }
  }

  return mic_open_ok && spkr_open_ok;
}

static void
init_audio_pipeline(QtelCallDialog *self)
{
  Settings *settings = settings_get_default();

  // Get audio device settings
  const gchar *mic_device = settings_get_mic_audio_device(settings);
  const gchar *spkr_device = settings_get_spkr_audio_device(settings);

  // Use default ALSA device if not set
  if (mic_device == nullptr || mic_device[0] == '\0')
    mic_device = "alsa:default";
  if (spkr_device == nullptr || spkr_device[0] == '\0')
    spkr_device = "alsa:default";

  g_message("Mic device: %s, Speaker device: %s", mic_device, spkr_device);

  // Create audio IO devices
  self->mic_audio_io = new AudioIO(mic_device, 0);
  self->spkr_audio_io = new AudioIO(spkr_device, 0);

  // Set up receive audio path (QSO -> speaker)
  AudioSource *prev_src = nullptr;

  self->rem_audio_fifo = new AudioFifo(INTERNAL_SAMPLE_RATE);
  self->rem_audio_fifo->setOverwrite(true);
  self->rem_audio_fifo->setPrebufSamples(1280 * INTERNAL_SAMPLE_RATE / 8000);
  prev_src = self->rem_audio_fifo;

  self->rem_audio_valve = new AudioValve;
  self->rem_audio_valve->setOpen(false);
  prev_src->registerSink(self->rem_audio_valve);
  prev_src = self->rem_audio_valve;

  // Add interpolation if needed for speaker sample rate
#if (INTERNAL_SAMPLE_RATE == 8000)
  if (self->spkr_audio_io->sampleRate() > 8000)
#endif
  {
    // Interpolate sample rate to 16kHz
    AudioInterpolator *i1 = new AudioInterpolator(2, coeff_16_8,
                                                  coeff_16_8_taps);
    prev_src->registerSink(i1, true);
    prev_src = i1;
  }

  if (self->spkr_audio_io->sampleRate() > 16000)
  {
    // Interpolate sample rate to 48kHz
#if (INTERNAL_SAMPLE_RATE == 8000)
    AudioInterpolator *i2 = new AudioInterpolator(3, coeff_48_16_int,
                                                  coeff_48_16_int_taps);
#else
    AudioInterpolator *i2 = new AudioInterpolator(3, coeff_48_16,
                                                  coeff_48_16_taps);
#endif
    prev_src->registerSink(i2, true);
    prev_src = i2;
  }

  prev_src->registerSink(self->spkr_audio_io);
  prev_src = nullptr;

  // Set up transmit audio path (mic -> QSO)
  prev_src = self->mic_audio_io;

  // Buffer before decimators
  AudioFifo *mic_fifo = new AudioFifo(2048);
  prev_src->registerSink(mic_fifo, true);
  prev_src = mic_fifo;

  // Decimate from 48kHz to 16kHz if needed
  if (self->mic_audio_io->sampleRate() > 16000)
  {
    AudioDecimator *d1 = new AudioDecimator(3, coeff_48_16_wide,
                                            coeff_48_16_wide_taps);
    prev_src->registerSink(d1, true);
    prev_src = d1;
  }

#if (INTERNAL_SAMPLE_RATE < 16000)
  // Decimate from 16kHz to 8kHz if needed
  if (self->mic_audio_io->sampleRate() > 8000)
  {
    AudioDecimator *d2 = new AudioDecimator(2, coeff_16_8, coeff_16_8_taps);
    prev_src->registerSink(d2, true);
    prev_src = d2;
  }
#endif

  self->tx_audio_splitter = new AudioSplitter;
  prev_src->registerSink(self->tx_audio_splitter);
  prev_src = nullptr;

  // Note: VOX level metering would require Vox to inherit from AudioSink
  // For now, VOX is disabled until we properly implement AudioSink interface in Vox
  // TODO: Make Vox inherit from Async::AudioSink and enable this:
  // self->tx_audio_splitter->addSink(self->vox);

#if INTERNAL_SAMPLE_RATE == 16000
  AudioDecimator *down_sampler = new AudioDecimator(2, coeff_16_8, coeff_16_8_taps);
  self->tx_audio_splitter->addSink(down_sampler, true);
  self->ptt_valve = new AudioValve;
  self->ptt_valve->setOpen(false);  // Start with PTT valve closed - no TX until PTT pressed
  down_sampler->registerSink(self->ptt_valve);
#else
  self->ptt_valve = new AudioValve;
  self->ptt_valve->setOpen(false);  // Start with PTT valve closed - no TX until PTT pressed
  self->tx_audio_splitter->addSink(self->ptt_valve);
#endif

  // Load VOX settings
  gboolean vox_enabled = settings_get_vox_enabled(settings);
  gint vox_threshold = settings_get_vox_threshold(settings);
  gint vox_delay = settings_get_vox_delay(settings);

  self->vox->setEnabled(vox_enabled);
  self->vox->setThreshold(vox_threshold);
  self->vox->setDelay(vox_delay);

  gtk_switch_set_active(GTK_SWITCH(self->vox_enable_switch), vox_enabled);
  gtk_range_set_value(GTK_RANGE(self->vox_threshold_scale), vox_threshold);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->vox_delay_spin), vox_delay);

  // Check full duplex setting
  self->audio_full_duplex = settings_get_use_full_duplex(settings);

  if (self->audio_full_duplex)
  {
    if (open_audio_device(self, AudioIO::MODE_RDWR))
    {
      self->rem_audio_valve->setOpen(true);
    }
    gtk_widget_set_sensitive(self->vox_enable_switch, TRUE);
  }
  else
  {
    // Half duplex: Start in RX mode, VOX disabled
    // Open speaker for receiving audio
    if (open_audio_device(self, AudioIO::MODE_WR))
    {
      self->rem_audio_valve->setOpen(true);
    }
    gtk_widget_set_sensitive(self->vox_enable_switch, FALSE);
  }
}

static void
create_connection(QtelCallDialog *self, const IpAddress &ip)
{
  Settings *settings = settings_get_default();

  // Get user credentials
  string callsign = settings_get_callsign(settings);
  string name = settings_get_name(settings);
  string info = settings_get_info(settings);

  if (callsign.empty())
  {
    append_info(self, "Error: Callsign not configured\n");
    return;
  }

  g_message("Creating QSO to %s as %s", ip.toString().c_str(), callsign.c_str());

  // Create QSO object
  self->qso = new Qso(ip, callsign, name, info);

  if (!self->qso->initOk())
  {
    append_info(self, "Error: Could not create QSO connection\n");
    g_warning("QSO init failed");
    delete self->qso;
    self->qso = nullptr;
    return;
  }

  // Connect EchoLink signals
  self->qso->stateChange.connect(
    sigc::bind(sigc::ptr_fun(on_qso_state_change), self));
  self->qso->chatMsgReceived.connect(
    sigc::bind(sigc::ptr_fun(on_qso_chat_msg_received), self));
  self->qso->infoMsgReceived.connect(
    sigc::bind(sigc::ptr_fun(on_qso_info_msg_received), self));
  self->qso->isReceiving.connect(
    sigc::bind(sigc::ptr_fun(on_qso_is_receiving), self));

  // Connect audio pipeline to QSO
  if (self->ptt_valve != nullptr)
  {
    self->ptt_valve->registerSink(self->qso);
  }

  // Connect QSO output to receive audio fifo
  AudioSource *qso_src = self->qso;
  qso_src->registerSink(self->rem_audio_fifo);

  // Enable connect button
  gtk_widget_set_sensitive(self->connect_button, TRUE);
  gtk_widget_grab_focus(self->connect_button);

  // Auto-accept incoming connection if requested
  if (self->accept_connection)
  {
    self->qso->accept();
  }

  set_transmitting(self, FALSE);
}

static void
add_css_provider(void)
{
  static gboolean css_added = FALSE;
  if (css_added) return;

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider,
    ".indicator-idle { background-color: @window_bg_color; }\n"
    ".indicator-rx { background-color: #33d17a; }\n"  /* Green */
    ".indicator-tx { background-color: #e01b24; }\n"  /* Red */
    ".indicator-hang { background-color: #f6d32d; }\n" /* Yellow */
    ".indicator { padding: 8px 16px; border-radius: 6px; "
    "border: 1px solid @borders; }\n"
    ".ptt-button { min-height: 60px; min-width: 200px; }\n"
  );
  gtk_style_context_add_provider_for_display(
    gdk_display_get_default(),
    GTK_STYLE_PROVIDER(provider),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
  );
  g_object_unref(provider);
  css_added = TRUE;
}

static GtkWidget *
create_station_info_group(QtelCallDialog *self)
{
  GtkWidget *group = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), "Station");

  // Callsign row
  AdwActionRow *call_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(call_row), "Callsign");
  self->callsign_label = gtk_label_new(self->callsign);
  gtk_widget_set_valign(self->callsign_label, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix(call_row, self->callsign_label);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(call_row));

  // Description row
  AdwActionRow *desc_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(desc_row), "Description");
  self->description_label = gtk_label_new(self->description);
  gtk_widget_set_valign(self->description_label, GTK_ALIGN_CENTER);
  gtk_label_set_ellipsize(GTK_LABEL(self->description_label), PANGO_ELLIPSIZE_END);
  adw_action_row_add_suffix(desc_row, self->description_label);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(desc_row));

  // Status row
  AdwActionRow *status_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(status_row), "Status");
  self->status_label = gtk_label_new("Disconnected");
  gtk_widget_set_valign(self->status_label, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix(status_row, self->status_label);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(status_row));

  // IP row
  AdwActionRow *ip_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(ip_row), "IP Address");
  self->ip_label = gtk_label_new(self->ip_address ? self->ip_address : "?");
  gtk_widget_set_valign(self->ip_label, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix(ip_row, self->ip_label);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(ip_row));

  return group;
}

static GtkWidget *
create_chat_area(QtelCallDialog *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_start(box, 12);
  gtk_widget_set_margin_end(box, 12);

  // Stack for Chat / Info tabs
  GtkWidget *stack = adw_view_stack_new();

  // Chat page
  GtkWidget *chat_scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(chat_scroll), 150);
  self->chat_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(self->chat_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->chat_view), GTK_WRAP_WORD_CHAR);
  self->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->chat_view));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(chat_scroll), self->chat_view);

  AdwViewStackPage *chat_page = adw_view_stack_add_titled(
    ADW_VIEW_STACK(stack), chat_scroll, "chat", "Chat");
  adw_view_stack_page_set_icon_name(chat_page, "user-available-symbolic");

  // Info page
  GtkWidget *info_scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(info_scroll), 150);
  self->info_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(self->info_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->info_view), GTK_WRAP_WORD_CHAR);
  self->info_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->info_view));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(info_scroll), self->info_view);

  AdwViewStackPage *info_page = adw_view_stack_add_titled(
    ADW_VIEW_STACK(stack), info_scroll, "info", "Info");
  adw_view_stack_page_set_icon_name(info_page, "dialog-information-symbolic");

  gtk_widget_set_vexpand(stack, TRUE);

  // View switcher
  GtkWidget *switcher = adw_view_switcher_new();
  adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_STACK(stack));
  adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);

  gtk_box_append(GTK_BOX(box), switcher);
  gtk_box_append(GTK_BOX(box), stack);

  // Chat entry
  self->chat_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(self->chat_entry), "Type message...");
  gtk_widget_set_sensitive(self->chat_entry, FALSE);
  g_signal_connect(self->chat_entry, "activate", G_CALLBACK(on_chat_entry_activate), self);
  gtk_box_append(GTK_BOX(box), self->chat_entry);

  return box;
}

static GtkWidget *
create_indicators(QtelCallDialog *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(box, 6);
  gtk_widget_set_margin_bottom(box, 6);

  // RX indicator
  self->rx_indicator = gtk_label_new("RX");
  gtk_widget_add_css_class(self->rx_indicator, "indicator");
  gtk_widget_add_css_class(self->rx_indicator, "indicator-idle");
  gtk_box_append(GTK_BOX(box), self->rx_indicator);

  // TX indicator
  self->tx_indicator = gtk_label_new("TX");
  gtk_widget_add_css_class(self->tx_indicator, "indicator");
  gtk_widget_add_css_class(self->tx_indicator, "indicator-idle");
  gtk_box_append(GTK_BOX(box), self->tx_indicator);

  return box;
}

static GtkWidget *
create_vox_controls(QtelCallDialog *self)
{
  GtkWidget *group = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), "VOX");

  // Enable row
  AdwActionRow *enable_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(enable_row), "Enable VOX");
  self->vox_enable_switch = gtk_switch_new();
  gtk_widget_set_valign(self->vox_enable_switch, GTK_ALIGN_CENTER);
  g_signal_connect(self->vox_enable_switch, "notify::active",
                   G_CALLBACK(on_vox_enabled_changed), self);
  adw_action_row_add_suffix(enable_row, self->vox_enable_switch);
  adw_action_row_set_activatable_widget(enable_row, self->vox_enable_switch);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(enable_row));

  // Level meter row
  AdwActionRow *level_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(level_row), "Level");
  GtkWidget *level_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  self->vox_level_bar = gtk_level_bar_new_for_interval(0, 1);
  gtk_level_bar_set_mode(GTK_LEVEL_BAR(self->vox_level_bar), GTK_LEVEL_BAR_MODE_CONTINUOUS);
  gtk_widget_set_size_request(self->vox_level_bar, 150, -1);
  gtk_widget_set_valign(self->vox_level_bar, GTK_ALIGN_CENTER);
  self->vox_indicator = gtk_label_new("");
  gtk_widget_set_size_request(self->vox_indicator, 20, 20);
  gtk_widget_add_css_class(self->vox_indicator, "indicator-idle");
  gtk_box_append(GTK_BOX(level_box), self->vox_level_bar);
  gtk_box_append(GTK_BOX(level_box), self->vox_indicator);
  adw_action_row_add_suffix(level_row, level_box);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(level_row));

  // Threshold row
  AdwActionRow *threshold_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(threshold_row), "Threshold");
  self->vox_threshold_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -60, 0, 1);
  gtk_range_set_value(GTK_RANGE(self->vox_threshold_scale), -30);
  gtk_widget_set_size_request(self->vox_threshold_scale, 150, -1);
  gtk_widget_set_valign(self->vox_threshold_scale, GTK_ALIGN_CENTER);
  gtk_widget_set_sensitive(self->vox_threshold_scale, FALSE);
  g_signal_connect(self->vox_threshold_scale, "value-changed",
                   G_CALLBACK(on_vox_threshold_changed), self);
  adw_action_row_add_suffix(threshold_row, self->vox_threshold_scale);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(threshold_row));

  // Delay row
  AdwActionRow *delay_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(delay_row), "Delay (ms)");
  self->vox_delay_spin = gtk_spin_button_new_with_range(0, 3000, 100);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->vox_delay_spin), 1000);
  gtk_widget_set_valign(self->vox_delay_spin, GTK_ALIGN_CENTER);
  gtk_widget_set_sensitive(self->vox_delay_spin, FALSE);
  g_signal_connect(self->vox_delay_spin, "value-changed",
                   G_CALLBACK(on_vox_delay_changed), self);
  adw_action_row_add_suffix(delay_row, self->vox_delay_spin);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), GTK_WIDGET(delay_row));

  return group;
}

static GtkWidget *
create_ptt_button(QtelCallDialog *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(box, 12);
  gtk_widget_set_margin_bottom(box, 12);

  self->ptt_button = gtk_toggle_button_new_with_label("PTT");
  gtk_widget_add_css_class(self->ptt_button, "ptt-button");
  gtk_widget_add_css_class(self->ptt_button, "suggested-action");
  gtk_widget_set_sensitive(self->ptt_button, FALSE);

  // Press/release gesture for non-toggle mode
  GtkGesture *press = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), GDK_BUTTON_PRIMARY);
  g_signal_connect(press, "pressed", G_CALLBACK(on_ptt_pressed), self);
  g_signal_connect(press, "released", G_CALLBACK(on_ptt_released), self);
  gtk_widget_add_controller(self->ptt_button, GTK_EVENT_CONTROLLER(press));

  // Toggle signal
  g_signal_connect(self->ptt_button, "toggled", G_CALLBACK(on_ptt_toggled), self);

  gtk_box_append(GTK_BOX(box), self->ptt_button);

  // Hint label
  GtkWidget *hint = gtk_label_new("Hold to talk, or Ctrl+click to toggle");
  gtk_widget_add_css_class(hint, "dim-label");
  gtk_box_append(GTK_BOX(box), hint);

  return box;
}

static void
qtel_call_dialog_finalize(GObject *object)
{
  QtelCallDialog *self = QTEL_CALL_DIALOG(object);

  // Clean up EchoLink QSO
  delete self->qso;
  self->qso = nullptr;

  // Clean up DNS
  delete self->dns;
  self->dns = nullptr;

  // Clean up audio pipeline
  delete self->ptt_valve;
  delete self->tx_audio_splitter;
  delete self->rem_audio_valve;
  delete self->rem_audio_fifo;
  delete self->mic_audio_io;
  delete self->spkr_audio_io;

  // Clean up VOX
  delete self->vox;

  g_free(self->callsign);
  g_free(self->description);
  g_free(self->ip_address);

  G_OBJECT_CLASS(qtel_call_dialog_parent_class)->finalize(object);
}

static void
qtel_call_dialog_class_init(QtelCallDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = qtel_call_dialog_finalize;
}

static void
qtel_call_dialog_init(QtelCallDialog *self)
{
  add_css_provider();

  self->callsign = nullptr;
  self->description = nullptr;
  self->ip_address = nullptr;
  self->node_id = 0;
  self->state = CONNECTION_STATE_DISCONNECTED;
  self->is_transmitting = FALSE;
  self->is_receiving = FALSE;
  self->accept_connection = FALSE;
  self->ctrl_pressed = FALSE;
  self->ptt_toggle_mode = FALSE;
  self->audio_full_duplex = FALSE;

  // Initialize pointers
  self->qso = nullptr;
  self->dns = nullptr;
  self->mic_audio_io = nullptr;
  self->spkr_audio_io = nullptr;
  self->rem_audio_fifo = nullptr;
  self->rem_audio_valve = nullptr;
  self->ptt_valve = nullptr;
  self->tx_audio_splitter = nullptr;

  // Create VOX
  self->vox = new Vox();
  self->vox->levelChanged.connect(sigc::bind(sigc::ptr_fun(on_vox_level_changed), self));
  self->vox->stateChanged.connect(sigc::bind(sigc::ptr_fun(on_vox_state_changed), self));

  // Window setup
  gtk_window_set_default_size(GTK_WINDOW(self), 500, 700);
  gtk_window_set_resizable(GTK_WINDOW(self), TRUE);

  // Main layout
  GtkWidget *toolbar_view = adw_toolbar_view_new();

  // Header bar
  self->header_bar = adw_header_bar_new();

  // Connect button
  self->connect_button = gtk_button_new_with_label("Connect");
  gtk_widget_add_css_class(self->connect_button, "suggested-action");
  gtk_widget_set_sensitive(self->connect_button, FALSE);  // Disabled until QSO created
  g_signal_connect(self->connect_button, "clicked", G_CALLBACK(on_connect_clicked), self);
  adw_header_bar_pack_start(ADW_HEADER_BAR(self->header_bar), self->connect_button);

  // Disconnect button
  self->disconnect_button = gtk_button_new_with_label("Disconnect");
  gtk_widget_add_css_class(self->disconnect_button, "destructive-action");
  gtk_widget_set_sensitive(self->disconnect_button, FALSE);
  g_signal_connect(self->disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), self);
  adw_header_bar_pack_start(ADW_HEADER_BAR(self->header_bar), self->disconnect_button);

  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), self->header_bar);

  // Content
  GtkWidget *scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_start(content_box, 12);
  gtk_widget_set_margin_end(content_box, 12);
  gtk_widget_set_margin_top(content_box, 12);
  gtk_widget_set_margin_bottom(content_box, 12);

  // Station info
  gtk_box_append(GTK_BOX(content_box), create_station_info_group(self));

  // Chat area
  gtk_box_append(GTK_BOX(content_box), create_chat_area(self));

  // RX/TX indicators
  gtk_box_append(GTK_BOX(content_box), create_indicators(self));

  // VOX controls
  gtk_box_append(GTK_BOX(content_box), create_vox_controls(self));

  // PTT button
  gtk_box_append(GTK_BOX(content_box), create_ptt_button(self));

  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content_box);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), scroll);

  adw_window_set_content(ADW_WINDOW(self), toolbar_view);
}

QtelCallDialog *
qtel_call_dialog_new(GtkWindow *parent,
                      const gchar *callsign,
                      const gchar *description,
                      gint node_id,
                      const gchar *ip_address)
{
  QtelCallDialog *self = static_cast<QtelCallDialog*>(
    g_object_new(QTEL_TYPE_CALL_DIALOG,
                 "transient-for", parent,
                 nullptr)
  );

  self->callsign = g_strdup(callsign ? callsign : "?");
  self->description = g_strdup(description ? description : "");
  self->node_id = node_id;
  self->ip_address = g_strdup(ip_address ? ip_address : "");

  // Set window title
  gchar *title = g_strdup_printf("QSO: %s", self->callsign);
  gtk_window_set_title(GTK_WINDOW(self), title);
  g_free(title);

  // Update labels
  gtk_label_set_text(GTK_LABEL(self->callsign_label), self->callsign);
  gtk_label_set_text(GTK_LABEL(self->description_label), self->description);
  gtk_label_set_text(GTK_LABEL(self->ip_label), self->ip_address);

  // Initialize audio pipeline
  init_audio_pipeline(self);

  // Create connection if IP is available
  if (ip_address != nullptr && ip_address[0] != '\0')
  {
    IpAddress ip(ip_address);
    if (!ip.isEmpty())
    {
      create_connection(self, ip);
    }
  }

  return self;
}

QtelCallDialog *
qtel_call_dialog_new_from_host(GtkWindow *parent, const gchar *host)
{
  QtelCallDialog *self = static_cast<QtelCallDialog*>(
    g_object_new(QTEL_TYPE_CALL_DIALOG,
                 "transient-for", parent,
                 nullptr)
  );

  self->callsign = g_strdup(host ? host : "?");
  self->description = g_strdup("Direct connection");
  self->node_id = 0;
  self->ip_address = g_strdup("");

  // Set window title
  gchar *title = g_strdup_printf("QSO: %s", self->callsign);
  gtk_window_set_title(GTK_WINDOW(self), title);
  g_free(title);

  // Update labels
  gtk_label_set_text(GTK_LABEL(self->callsign_label), self->callsign);
  gtk_label_set_text(GTK_LABEL(self->description_label), self->description);
  gtk_label_set_text(GTK_LABEL(self->ip_label), "Resolving...");

  // Initialize audio pipeline
  init_audio_pipeline(self);

  // Do DNS lookup
  self->dns = new DnsLookup(host);
  self->dns->resultsReady.connect(
    sigc::bind(sigc::ptr_fun(on_dns_results_ready), self));

  return self;
}

void
qtel_call_dialog_accept(QtelCallDialog *self)
{
  g_return_if_fail(QTEL_IS_CALL_DIALOG(self));
  self->accept_connection = TRUE;

  // If QSO already exists, accept now
  if (self->qso != nullptr)
  {
    self->qso->accept();
  }
}
