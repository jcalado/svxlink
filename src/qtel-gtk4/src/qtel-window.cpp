/**
@file    qtel-window.cpp
@brief   Main window class for Qtel GTK4
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

#include "qtel-window.h"
#include "qtel-config.h"
#include "settings.h"
#include "station-object.h"
#include "station-list-model.h"
#include "qtel-call-dialog.h"

#include <EchoLinkDirectory.h>

#include <vector>
#include <string>
#include <sstream>

/**
 * Category filter enumeration
 */
typedef enum {
  CATEGORY_BOOKMARKS = 0,
  CATEGORY_CONFERENCES,
  CATEGORY_LINKS,
  CATEGORY_REPEATERS,
  CATEGORY_STATIONS,
  N_CATEGORIES
} StationCategory;

struct _QtelWindow
{
  AdwApplicationWindow parent_instance;

  // Header bar widgets
  GtkWidget *header_bar;
  GtkWidget *category_dropdown;
  GtkWidget *busy_toggle;
  GtkWidget *search_button;
  GtkWidget *menu_button;

  // Main content widgets
  GtkWidget *main_paned;        // Horizontal pane: sidebar | content
  GtkWidget *station_column_view;
  GtkWidget *search_entry;
  GtkWidget *content_stack;
  GtkWidget *messages_view;
  GtkWidget *incoming_view;
  GtkWidget *refresh_spinner;

  // Station model and filtering
  StationListModel *station_model;
  GtkFilterListModel *filter_model;
  GtkSingleSelection *selection_model;
  StationCategory current_category;
  gchar *search_text;

  // Settings
  GSettings *settings;

  // EchoLink Directory
  EchoLink::Directory *directory;
  gboolean is_refreshing;
};

G_DEFINE_TYPE(QtelWindow, qtel_window, ADW_TYPE_APPLICATION_WINDOW)

static void
on_busy_toggled(GtkToggleButton *button, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  gboolean is_busy = gtk_toggle_button_get_active(button);
  g_message("Busy toggled: %s", is_busy ? "true" : "false");
  // TODO: Update EchoLink status
}

static void update_filter(QtelWindow *self);
static void populate_station_list(QtelWindow *self);
static void show_toast(QtelWindow *self, const gchar *message);

// Directory callbacks - forward declarations
static void on_directory_status_changed(EchoLink::StationData::Status status, QtelWindow *self);
static void on_directory_station_list_updated(QtelWindow *self);
static void on_directory_error(const std::string& msg, QtelWindow *self);

static void
on_category_changed(GtkDropDown *dropdown, GParamSpec *pspec, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  guint selected = gtk_drop_down_get_selected(dropdown);
  const char *categories[] = { "Bookmarks", "Conferences", "Links", "Repeaters", "Stations" };

  if (selected < G_N_ELEMENTS(categories))
  {
    g_message("Category changed to: %s", categories[selected]);
    self->current_category = static_cast<StationCategory>(selected);
    update_filter(self);
  }
}

static void
on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  g_free(self->search_text);
  self->search_text = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
  update_filter(self);
}

static void
on_refresh_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);

  if (self->directory == nullptr)
  {
    g_warning("Directory not initialized - cannot refresh");
    show_toast(self, "Not logged in to directory server");
    return;
  }

  if (self->is_refreshing)
  {
    g_message("Already refreshing, ignoring request");
    return;
  }

  g_message("Refresh station list from directory server");
  self->is_refreshing = TRUE;

  // Show spinner in header bar
  if (self->refresh_spinner != nullptr)
  {
    gtk_spinner_start(GTK_SPINNER(self->refresh_spinner));
    gtk_widget_set_visible(self->refresh_spinner, TRUE);
  }

  // Request station list from directory server
  self->directory->getCalls();
}

// Helper to convert EchoLink::StationData::Status to our StationStatus
static StationStatus
convert_status(EchoLink::StationData::Status status)
{
  switch (status)
  {
    case EchoLink::StationData::STAT_ONLINE:
      return STATION_STATUS_ONLINE;
    case EchoLink::StationData::STAT_BUSY:
      return STATION_STATUS_BUSY;
    case EchoLink::StationData::STAT_OFFLINE:
      return STATION_STATUS_OFFLINE;
    default:
      return STATION_STATUS_UNKNOWN;
  }
}

// Called when directory station list is updated
static void
on_directory_station_list_updated(QtelWindow *self)
{
  g_message("Station list updated from directory server");

  self->is_refreshing = FALSE;

  // Hide spinner
  if (self->refresh_spinner != nullptr)
  {
    gtk_spinner_stop(GTK_SPINNER(self->refresh_spinner));
    gtk_widget_set_visible(self->refresh_spinner, FALSE);
  }

  // Populate the model with real data
  populate_station_list(self);

  // Show server message if any
  if (self->directory != nullptr)
  {
    const std::string& msg = self->directory->message();
    if (!msg.empty())
    {
      GtkTextIter end;
      GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->messages_view));
      gtk_text_buffer_get_end_iter(buffer, &end);
      gtk_text_buffer_insert(buffer, &end, msg.c_str(), -1);
      gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }
  }

  guint count = station_list_model_get_count(self->station_model);
  gchar *toast_msg = g_strdup_printf("Loaded %u stations", count);
  show_toast(self, toast_msg);
  g_free(toast_msg);
}

// Called when directory status changes
static void
on_directory_status_changed(EchoLink::StationData::Status status, QtelWindow *self)
{
  const char *status_str = EchoLink::StationData::statusStr(status).c_str();
  g_message("Directory status changed: %s", status_str);

  // Update busy toggle to match
  if (status == EchoLink::StationData::STAT_BUSY)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->busy_toggle), TRUE);
  }
  else if (status == EchoLink::StationData::STAT_ONLINE)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->busy_toggle), FALSE);
  }

  // Auto-refresh when we go online
  if (status == EchoLink::StationData::STAT_ONLINE ||
      status == EchoLink::StationData::STAT_BUSY)
  {
    // Trigger a refresh
    on_refresh_activated(nullptr, nullptr, self);
  }
}

// Called on directory error
static void
on_directory_error(const std::string& msg, QtelWindow *self)
{
  g_warning("Directory error: %s", msg.c_str());

  self->is_refreshing = FALSE;

  // Hide spinner
  if (self->refresh_spinner != nullptr)
  {
    gtk_spinner_stop(GTK_SPINNER(self->refresh_spinner));
    gtk_widget_set_visible(self->refresh_spinner, FALSE);
  }

  show_toast(self, msg.c_str());
}

// Convert potentially non-UTF-8 string to valid UTF-8
// EchoLink data may contain Latin-1 or other encodings
static gchar*
to_utf8(const char *str)
{
  if (str == nullptr || str[0] == '\0')
    return g_strdup("");

  // First check if it's already valid UTF-8
  if (g_utf8_validate(str, -1, nullptr))
    return g_strdup(str);

  // Try to convert from Latin-1 (ISO-8859-1) which is common
  gchar *utf8 = g_convert(str, -1, "UTF-8", "ISO-8859-1", nullptr, nullptr, nullptr);
  if (utf8 != nullptr)
    return utf8;

  // If that fails, just use ASCII characters
  GString *result = g_string_new(nullptr);
  for (const char *p = str; *p != '\0'; p++)
  {
    if ((*p & 0x80) == 0) // ASCII character
      g_string_append_c(result, *p);
    else
      g_string_append_c(result, '?'); // Replace non-ASCII
  }
  return g_string_free(result, FALSE);
}

// Populate station list from directory
static void
populate_station_list(QtelWindow *self)
{
  if (self->directory == nullptr)
    return;

  station_list_model_clear(self->station_model);

  // Add conferences
  const std::list<EchoLink::StationData>& conferences = self->directory->conferences();
  for (const auto& stn : conferences)
  {
    g_autofree gchar *desc_utf8 = to_utf8(stn.description().c_str());
    station_list_model_update_or_add(self->station_model,
      stn.callsign().c_str(),
      desc_utf8,
      convert_status(stn.status()),
      stn.time().c_str(),
      stn.id(),
      stn.ipStr().c_str());
  }

  // Add links
  const std::list<EchoLink::StationData>& links = self->directory->links();
  for (const auto& stn : links)
  {
    g_autofree gchar *desc_utf8 = to_utf8(stn.description().c_str());
    station_list_model_update_or_add(self->station_model,
      stn.callsign().c_str(),
      desc_utf8,
      convert_status(stn.status()),
      stn.time().c_str(),
      stn.id(),
      stn.ipStr().c_str());
  }

  // Add repeaters
  const std::list<EchoLink::StationData>& repeaters = self->directory->repeaters();
  for (const auto& stn : repeaters)
  {
    g_autofree gchar *desc_utf8 = to_utf8(stn.description().c_str());
    station_list_model_update_or_add(self->station_model,
      stn.callsign().c_str(),
      desc_utf8,
      convert_status(stn.status()),
      stn.time().c_str(),
      stn.id(),
      stn.ipStr().c_str());
  }

  // Add stations
  const std::list<EchoLink::StationData>& stations = self->directory->stations();
  for (const auto& stn : stations)
  {
    g_autofree gchar *desc_utf8 = to_utf8(stn.description().c_str());
    station_list_model_update_or_add(self->station_model,
      stn.callsign().c_str(),
      desc_utf8,
      convert_status(stn.status()),
      stn.time().c_str(),
      stn.id(),
      stn.ipStr().c_str());
  }

  g_message("Populated %u total stations", station_list_model_get_count(self->station_model));
}

static void
on_connect_ip_response(AdwAlertDialog *dialog, const gchar *response, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);

  if (g_strcmp0(response, "connect") == 0)
  {
    GtkWidget *entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "entry"));
    const gchar *host = gtk_editable_get_text(GTK_EDITABLE(entry));

    if (host != nullptr && host[0] != '\0')
    {
      g_message("Connecting to IP: %s", host);

      // Save to settings for next time
      GSettings *settings = g_settings_new(APP_ID);
      g_settings_set_string(settings, "connect-to-ip", host);
      g_object_unref(settings);

      // Open call dialog
      QtelCallDialog *call_dialog = qtel_call_dialog_new_from_host(GTK_WINDOW(self), host);
      gtk_window_present(GTK_WINDOW(call_dialog));
    }
  }
}

static void
on_connect_ip_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);

  AdwAlertDialog *dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new(
    "Connect to IP Address",
    "Enter the IP address or hostname of the EchoLink station to connect to."));

  adw_alert_dialog_add_responses(dialog,
    "cancel", "Cancel",
    "connect", "Connect",
    NULL);
  adw_alert_dialog_set_response_appearance(dialog, "connect", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(dialog, "connect");
  adw_alert_dialog_set_close_response(dialog, "cancel");

  // Create entry widget
  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "IP address or hostname");
  gtk_widget_set_margin_start(entry, 12);
  gtk_widget_set_margin_end(entry, 12);

  // Load last used address
  GSettings *settings = g_settings_new(APP_ID);
  const gchar *last_ip = g_settings_get_string(settings, "connect-to-ip");
  if (last_ip != nullptr && last_ip[0] != '\0')
  {
    gtk_editable_set_text(GTK_EDITABLE(entry), last_ip);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
  }
  g_object_unref(settings);

  adw_alert_dialog_set_extra_child(dialog, entry);
  g_object_set_data(G_OBJECT(dialog), "entry", entry);

  g_signal_connect(dialog, "response", G_CALLBACK(on_connect_ip_response), self);

  adw_alert_dialog_choose(dialog, GTK_WIDGET(self), NULL, NULL, NULL);
}

// Custom filter function for category and search
static gboolean
station_filter_func(GObject *item, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  StationObject *station = STATION_OBJECT(item);

  // First apply category filter
  StationType type = station_object_get_station_type(station);
  gboolean category_match = FALSE;

  switch (self->current_category)
  {
    case CATEGORY_BOOKMARKS:
      {
        Settings *settings = settings_get_default();
        g_auto(GStrv) bookmarks = settings_get_bookmarks(settings);
        const gchar *callsign = station_object_get_callsign(station);

        for (int i = 0; bookmarks != nullptr && bookmarks[i] != nullptr; i++)
        {
          if (g_strcmp0(bookmarks[i], callsign) == 0)
          {
            category_match = TRUE;
            break;
          }
        }
      }
      break;
    case CATEGORY_CONFERENCES:
      category_match = (type == STATION_TYPE_CONFERENCE);
      break;
    case CATEGORY_LINKS:
      category_match = (type == STATION_TYPE_LINK);
      break;
    case CATEGORY_REPEATERS:
      category_match = (type == STATION_TYPE_REPEATER);
      break;
    case CATEGORY_STATIONS:
    default:
      category_match = TRUE; // Show all
      break;
  }

  if (!category_match)
    return FALSE;

  // Then apply search filter if there's search text
  if (self->search_text != nullptr && self->search_text[0] != '\0')
  {
    const gchar *callsign = station_object_get_callsign(station);
    const gchar *description = station_object_get_description(station);

    gchar *search_lower = g_utf8_strdown(self->search_text, -1);
    gchar *callsign_lower = g_utf8_strdown(callsign ? callsign : "", -1);
    gchar *desc_lower = g_utf8_strdown(description ? description : "", -1);

    gboolean match = (g_strstr_len(callsign_lower, -1, search_lower) != nullptr) ||
                     (g_strstr_len(desc_lower, -1, search_lower) != nullptr);

    g_free(search_lower);
    g_free(callsign_lower);
    g_free(desc_lower);

    return match;
  }

  return TRUE;
}

static void
update_filter(QtelWindow *self)
{
  if (self->filter_model != nullptr)
  {
    GtkFilter *filter = gtk_filter_list_model_get_filter(self->filter_model);
    if (filter != nullptr)
    {
      gtk_filter_changed(filter, GTK_FILTER_CHANGE_DIFFERENT);
    }
  }
}

// Column view factory callbacks for status icon
static void
setup_status_icon(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *image = gtk_image_new();
  gtk_list_item_set_child(list_item, image);
}

static void
bind_status_icon(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *image = gtk_list_item_get_child(list_item);
  StationObject *station = STATION_OBJECT(gtk_list_item_get_item(list_item));

  const gchar *icon_name = station_object_get_status_icon_name(station);
  gtk_image_set_from_icon_name(GTK_IMAGE(image), icon_name);
}

// Column view factory callbacks for callsign
static void
setup_callsign(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_label_new(nullptr);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_list_item_set_child(list_item, label);
}

static void
bind_callsign(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_list_item_get_child(list_item);
  StationObject *station = STATION_OBJECT(gtk_list_item_get_item(list_item));

  gtk_label_set_text(GTK_LABEL(label), station_object_get_callsign(station));
}

// Column view factory callbacks for description
static void
setup_description(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_label_new(nullptr);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
  gtk_list_item_set_child(list_item, label);
}

static void
bind_description(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_list_item_get_child(list_item);
  StationObject *station = STATION_OBJECT(gtk_list_item_get_item(list_item));

  gtk_label_set_text(GTK_LABEL(label), station_object_get_description(station));
}

// Column view factory callbacks for time
static void
setup_time(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_label_new(nullptr);
  gtk_label_set_xalign(GTK_LABEL(label), 0.5);
  gtk_list_item_set_child(list_item, label);
}

static void
bind_time(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_list_item_get_child(list_item);
  StationObject *station = STATION_OBJECT(gtk_list_item_get_item(list_item));

  gtk_label_set_text(GTK_LABEL(label), station_object_get_time(station));
}

// Column view factory callbacks for node ID
static void
setup_node_id(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_label_new(nullptr);
  gtk_label_set_xalign(GTK_LABEL(label), 1.0);
  gtk_list_item_set_child(list_item, label);
}

static void
bind_node_id(GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
  GtkWidget *label = gtk_list_item_get_child(list_item);
  StationObject *station = STATION_OBJECT(gtk_list_item_get_item(list_item));

  gchar *id_str = g_strdup_printf("%d", station_object_get_id(station));
  gtk_label_set_text(GTK_LABEL(label), id_str);
  g_free(id_str);
}

// Double-click handler for station activation
static void
on_station_activated(GtkColumnView *column_view, guint position, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  GtkSelectionModel *model = gtk_column_view_get_model(column_view);
  StationObject *station = STATION_OBJECT(g_list_model_get_item(G_LIST_MODEL(model), position));

  if (station != nullptr)
  {
    g_message("Connecting to station: %s (ID: %d)",
              station_object_get_callsign(station),
              station_object_get_id(station));

    // Open call dialog
    QtelCallDialog *dialog = qtel_call_dialog_new(
      GTK_WINDOW(self),
      station_object_get_callsign(station),
      station_object_get_description(station),
      station_object_get_id(station),
      station_object_get_ip_address(station)
    );
    gtk_window_present(GTK_WINDOW(dialog));

    g_object_unref(station);
  }
}

// Context menu action handlers
static void
on_connect_station(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  StationObject *station = STATION_OBJECT(
    gtk_single_selection_get_selected_item(self->selection_model));

  if (station != nullptr)
  {
    g_message("Connect to: %s", station_object_get_callsign(station));

    // Open call dialog
    QtelCallDialog *dialog = qtel_call_dialog_new(
      GTK_WINDOW(self),
      station_object_get_callsign(station),
      station_object_get_description(station),
      station_object_get_id(station),
      station_object_get_ip_address(station)
    );
    gtk_window_present(GTK_WINDOW(dialog));
  }
}

static void
show_toast(QtelWindow *self, const gchar *message)
{
  AdwToast *toast = adw_toast_new(message);
  adw_toast_set_timeout(toast, 2);

  // The content is a toast overlay wrapping the toolbar view
  GtkWidget *content = adw_application_window_get_content(ADW_APPLICATION_WINDOW(self));
  if (ADW_IS_TOAST_OVERLAY(content))
  {
    adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(content), toast);
  }
}

static void
on_add_bookmark(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  StationObject *station = STATION_OBJECT(
    gtk_single_selection_get_selected_item(self->selection_model));

  if (station != nullptr)
  {
    const gchar *callsign = station_object_get_callsign(station);
    g_message("Add bookmark: %s", callsign);

    Settings *settings = settings_get_default();
    settings_add_bookmark(settings, callsign);

    gchar *msg = g_strdup_printf("Added %s to bookmarks", callsign);
    show_toast(self, msg);
    g_free(msg);

    update_filter(self);
  }
}

static void
on_remove_bookmark(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  StationObject *station = STATION_OBJECT(
    gtk_single_selection_get_selected_item(self->selection_model));

  if (station != nullptr)
  {
    const gchar *callsign = station_object_get_callsign(station);
    g_message("Remove bookmark: %s", callsign);

    Settings *settings = settings_get_default();
    settings_remove_bookmark(settings, callsign);

    gchar *msg = g_strdup_printf("Removed %s from bookmarks", callsign);
    show_toast(self, msg);
    g_free(msg);

    update_filter(self);
  }
}

static void
on_show_station_info(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  QtelWindow *self = QTEL_WINDOW(user_data);
  StationObject *station = STATION_OBJECT(
    gtk_single_selection_get_selected_item(self->selection_model));

  if (station != nullptr)
  {
    const gchar *callsign = station_object_get_callsign(station);
    const gchar *description = station_object_get_description(station);
    const gchar *ip_address = station_object_get_ip_address(station);
    const gchar *time = station_object_get_time(station);
    gint id = station_object_get_id(station);
    StationStatus status = station_object_get_status(station);

    const gchar *status_str = "Unknown";
    switch (status)
    {
      case STATION_STATUS_ONLINE: status_str = "Online"; break;
      case STATION_STATUS_BUSY: status_str = "Busy"; break;
      case STATION_STATUS_OFFLINE: status_str = "Offline"; break;
    }

    gchar *body = g_strdup_printf(
      "Callsign: %s\n"
      "Description: %s\n"
      "Status: %s\n"
      "Node ID: %d\n"
      "IP Address: %s\n"
      "Time: %s",
      callsign, description, status_str, id, ip_address, time);

    AdwAlertDialog *dialog = ADW_ALERT_DIALOG(adw_alert_dialog_new("Station Information", body));
    g_free(body);

    adw_alert_dialog_add_responses(dialog,
      "close", "Close",
      "connect", "Connect",
      NULL);
    adw_alert_dialog_set_response_appearance(dialog, "connect", ADW_RESPONSE_SUGGESTED);
    adw_alert_dialog_set_default_response(dialog, "connect");
    adw_alert_dialog_set_close_response(dialog, "close");

    // Store station info for connect action
    g_object_set_data_full(G_OBJECT(dialog), "callsign", g_strdup(callsign), g_free);
    g_object_set_data_full(G_OBJECT(dialog), "description", g_strdup(description), g_free);
    g_object_set_data_full(G_OBJECT(dialog), "ip_address", g_strdup(ip_address), g_free);
    g_object_set_data(G_OBJECT(dialog), "node_id", GINT_TO_POINTER(id));

    g_signal_connect(dialog, "response", G_CALLBACK(+[](AdwAlertDialog *dlg,
                                                        const gchar *response,
                                                        gpointer data) {
      if (g_strcmp0(response, "connect") == 0)
      {
        QtelWindow *win = QTEL_WINDOW(data);
        const gchar *cs = static_cast<const gchar*>(g_object_get_data(G_OBJECT(dlg), "callsign"));
        const gchar *desc = static_cast<const gchar*>(g_object_get_data(G_OBJECT(dlg), "description"));
        const gchar *ip = static_cast<const gchar*>(g_object_get_data(G_OBJECT(dlg), "ip_address"));
        gint nid = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dlg), "node_id"));

        QtelCallDialog *call = qtel_call_dialog_new(GTK_WINDOW(win), cs, desc, nid, ip);
        gtk_window_present(GTK_WINDOW(call));
      }
    }), self);

    adw_alert_dialog_choose(dialog, GTK_WIDGET(self), NULL, NULL, NULL);
  }
}

// Right-click handler for context menu
static void
on_station_right_click(GtkGestureClick *gesture, int n_press, double x, double y,
                        gpointer user_data)
{
  GtkWidget *popover = GTK_WIDGET(g_object_get_data(G_OBJECT(gesture), "popover"));
  if (popover != nullptr)
  {
    GdkRectangle rect = { static_cast<int>(x), static_cast<int>(y), 1, 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_popup(GTK_POPOVER(popover));
  }
}

static GtkWidget *
create_header_bar(QtelWindow *self)
{
  GtkWidget *header = adw_header_bar_new();

  // Category dropdown
  const char *categories[] = { "Bookmarks", "Conferences", "Links", "Repeaters", "Stations", nullptr };
  GtkStringList *category_model = gtk_string_list_new(categories);
  self->category_dropdown = gtk_drop_down_new(G_LIST_MODEL(category_model), nullptr);
  gtk_drop_down_set_selected(GTK_DROP_DOWN(self->category_dropdown), 4); // Default to "Stations"
  g_signal_connect(self->category_dropdown, "notify::selected",
                   G_CALLBACK(on_category_changed), self);
  adw_header_bar_pack_start(ADW_HEADER_BAR(header), self->category_dropdown);

  // Busy toggle button
  self->busy_toggle = gtk_toggle_button_new();
  gtk_button_set_icon_name(GTK_BUTTON(self->busy_toggle), "user-busy-symbolic");
  gtk_widget_set_tooltip_text(self->busy_toggle, "Toggle busy status");
  g_signal_connect(self->busy_toggle, "toggled", G_CALLBACK(on_busy_toggled), self);
  adw_header_bar_pack_start(ADW_HEADER_BAR(header), self->busy_toggle);

  // Refresh spinner (shown during refresh)
  self->refresh_spinner = gtk_spinner_new();
  gtk_widget_set_visible(self->refresh_spinner, FALSE);
  adw_header_bar_pack_start(ADW_HEADER_BAR(header), self->refresh_spinner);

  // Search button
  self->search_button = gtk_toggle_button_new();
  gtk_button_set_icon_name(GTK_BUTTON(self->search_button), "system-search-symbolic");
  gtk_widget_set_tooltip_text(self->search_button, "Search stations");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), self->search_button);

  // Menu button
  GMenu *menu = g_menu_new();
  g_menu_append(menu, "Refresh", "win.refresh");
  g_menu_append(menu, "Connect to IP...", "win.connect-ip");

  GMenu *section = g_menu_new();
  g_menu_append(section, "Preferences", "app.preferences");
  g_menu_append(section, "Keyboard Shortcuts", "app.shortcuts");
  g_menu_append(section, "About Qtel", "app.about");
  g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));

  self->menu_button = gtk_menu_button_new();
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(self->menu_button), "open-menu-symbolic");
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(self->menu_button), G_MENU_MODEL(menu));
  gtk_widget_set_tooltip_text(self->menu_button, "Main menu");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), self->menu_button);

  return header;
}

static GtkWidget *
create_sidebar(QtelWindow *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Search entry
  self->search_entry = gtk_search_entry_new();
  gtk_widget_set_margin_start(self->search_entry, 6);
  gtk_widget_set_margin_end(self->search_entry, 6);
  gtk_widget_set_margin_top(self->search_entry, 6);
  gtk_widget_set_margin_bottom(self->search_entry, 6);
  g_signal_connect(self->search_entry, "search-changed",
                   G_CALLBACK(on_search_changed), self);
  gtk_box_append(GTK_BOX(box), self->search_entry);

  // Create the station model
  self->station_model = station_list_model_new();

  // Create a custom filter
  GtkCustomFilter *filter = gtk_custom_filter_new(
    (GtkCustomFilterFunc)station_filter_func, self, nullptr);

  // Create filter list model
  self->filter_model = gtk_filter_list_model_new(
    G_LIST_MODEL(g_object_ref(self->station_model)),
    GTK_FILTER(filter));

  // Create single selection model
  self->selection_model = gtk_single_selection_new(
    G_LIST_MODEL(g_object_ref(self->filter_model)));
  gtk_single_selection_set_autoselect(self->selection_model, FALSE);

  // Create column view
  self->station_column_view = gtk_column_view_new(
    GTK_SELECTION_MODEL(g_object_ref(self->selection_model)));
  gtk_column_view_set_show_column_separators(
    GTK_COLUMN_VIEW(self->station_column_view), FALSE);
  gtk_column_view_set_show_row_separators(
    GTK_COLUMN_VIEW(self->station_column_view), FALSE);

  // Status column (icon)
  GtkListItemFactory *status_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(status_factory, "setup", G_CALLBACK(setup_status_icon), nullptr);
  g_signal_connect(status_factory, "bind", G_CALLBACK(bind_status_icon), nullptr);
  GtkColumnViewColumn *status_column = gtk_column_view_column_new("", status_factory);
  gtk_column_view_column_set_fixed_width(status_column, 32);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(self->station_column_view), status_column);

  // Callsign column
  GtkListItemFactory *callsign_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(callsign_factory, "setup", G_CALLBACK(setup_callsign), nullptr);
  g_signal_connect(callsign_factory, "bind", G_CALLBACK(bind_callsign), nullptr);
  GtkColumnViewColumn *callsign_column = gtk_column_view_column_new("Callsign", callsign_factory);
  gtk_column_view_column_set_resizable(callsign_column, TRUE);
  gtk_column_view_column_set_fixed_width(callsign_column, 100);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(self->station_column_view), callsign_column);

  // Description column
  GtkListItemFactory *desc_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(desc_factory, "setup", G_CALLBACK(setup_description), nullptr);
  g_signal_connect(desc_factory, "bind", G_CALLBACK(bind_description), nullptr);
  GtkColumnViewColumn *desc_column = gtk_column_view_column_new("Description", desc_factory);
  gtk_column_view_column_set_resizable(desc_column, TRUE);
  gtk_column_view_column_set_expand(desc_column, TRUE);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(self->station_column_view), desc_column);

  // Time column
  GtkListItemFactory *time_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(time_factory, "setup", G_CALLBACK(setup_time), nullptr);
  g_signal_connect(time_factory, "bind", G_CALLBACK(bind_time), nullptr);
  GtkColumnViewColumn *time_column = gtk_column_view_column_new("Time", time_factory);
  gtk_column_view_column_set_fixed_width(time_column, 60);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(self->station_column_view), time_column);

  // Node ID column
  GtkListItemFactory *id_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(id_factory, "setup", G_CALLBACK(setup_node_id), nullptr);
  g_signal_connect(id_factory, "bind", G_CALLBACK(bind_node_id), nullptr);
  GtkColumnViewColumn *id_column = gtk_column_view_column_new("ID", id_factory);
  gtk_column_view_column_set_fixed_width(id_column, 70);
  gtk_column_view_append_column(GTK_COLUMN_VIEW(self->station_column_view), id_column);

  // Connect double-click/activate signal
  g_signal_connect(self->station_column_view, "activate",
                   G_CALLBACK(on_station_activated), self);

  // Create context menu
  GMenu *context_menu = g_menu_new();
  g_menu_append(context_menu, "Connect", "win.connect-station");

  GMenu *bookmark_section = g_menu_new();
  g_menu_append(bookmark_section, "Add to Bookmarks", "win.add-bookmark");
  g_menu_append(bookmark_section, "Remove from Bookmarks", "win.remove-bookmark");
  g_menu_append_section(context_menu, nullptr, G_MENU_MODEL(bookmark_section));

  GMenu *info_section = g_menu_new();
  g_menu_append(info_section, "Station Info", "win.station-info");
  g_menu_append_section(context_menu, nullptr, G_MENU_MODEL(info_section));

  GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(context_menu));
  gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
  gtk_widget_set_parent(popover, self->station_column_view);

  // Right-click gesture for context menu
  GtkGesture *click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
  g_object_set_data(G_OBJECT(click), "popover", popover);
  g_signal_connect(click, "pressed", G_CALLBACK(on_station_right_click), self);
  gtk_widget_add_controller(self->station_column_view, GTK_EVENT_CONTROLLER(click));

  // Scrolled window for the column view
  GtkWidget *scrolled = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(scrolled, TRUE);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                 self->station_column_view);

  gtk_box_append(GTK_BOX(box), scrolled);

  return box;
}

static GtkWidget *
create_content(QtelWindow *self)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // View stack for Messages / Incoming
  GtkWidget *view_switcher_bar = adw_view_switcher_bar_new();
  self->content_stack = adw_view_stack_new();

  // Messages page
  GtkWidget *messages_scrolled = gtk_scrolled_window_new();
  self->messages_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(self->messages_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->messages_view), GTK_WRAP_WORD_CHAR);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(messages_scrolled), self->messages_view);

  AdwViewStackPage *messages_page = adw_view_stack_add_titled(
    ADW_VIEW_STACK(self->content_stack),
    messages_scrolled,
    "messages",
    "Messages"
  );
  adw_view_stack_page_set_icon_name(messages_page, "mail-unread-symbolic");

  // Incoming connections page
  GtkWidget *incoming_scrolled = gtk_scrolled_window_new();
  GtkWidget *incoming_list = gtk_list_box_new();
  gtk_list_box_set_placeholder(GTK_LIST_BOX(incoming_list),
    gtk_label_new("No incoming connections"));
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(incoming_scrolled), incoming_list);
  self->incoming_view = incoming_list;

  AdwViewStackPage *incoming_page = adw_view_stack_add_titled(
    ADW_VIEW_STACK(self->content_stack),
    incoming_scrolled,
    "incoming",
    "Incoming"
  );
  adw_view_stack_page_set_icon_name(incoming_page, "call-incoming-symbolic");

  gtk_widget_set_vexpand(self->content_stack, TRUE);
  gtk_box_append(GTK_BOX(box), self->content_stack);

  adw_view_switcher_bar_set_stack(ADW_VIEW_SWITCHER_BAR(view_switcher_bar),
                                   ADW_VIEW_STACK(self->content_stack));
  adw_view_switcher_bar_set_reveal(ADW_VIEW_SWITCHER_BAR(view_switcher_bar), TRUE);
  gtk_box_append(GTK_BOX(box), view_switcher_bar);

  return box;
}

static void
qtel_window_constructed(GObject *object)
{
  QtelWindow *self = QTEL_WINDOW(object);

  G_OBJECT_CLASS(qtel_window_parent_class)->constructed(object);

  // Set up window actions
  static const GActionEntry win_actions[] = {
    { "refresh", on_refresh_activated, nullptr, nullptr, nullptr },
    { "connect-ip", on_connect_ip_activated, nullptr, nullptr, nullptr },
    { "connect-station", on_connect_station, nullptr, nullptr, nullptr },
    { "add-bookmark", on_add_bookmark, nullptr, nullptr, nullptr },
    { "remove-bookmark", on_remove_bookmark, nullptr, nullptr, nullptr },
    { "station-info", on_show_station_info, nullptr, nullptr, nullptr },
    { "search", [](GSimpleAction*, GVariant*, gpointer user_data) {
        QtelWindow *self = QTEL_WINDOW(user_data);
        gtk_widget_grab_focus(self->search_entry);
      }, nullptr, nullptr, nullptr },
  };
  g_action_map_add_action_entries(G_ACTION_MAP(self), win_actions,
                                   G_N_ELEMENTS(win_actions), self);

  // Note: Keyboard shortcuts are set up in the application startup
}

static void
qtel_window_dispose(GObject *object)
{
  QtelWindow *self = QTEL_WINDOW(object);

  // Save window state
  if (self->settings != nullptr)
  {
    int width, height;
    gtk_window_get_default_size(GTK_WINDOW(self), &width, &height);
    g_settings_set_int(self->settings, "window-width", width);
    g_settings_set_int(self->settings, "window-height", height);
    g_settings_set_boolean(self->settings, "window-maximized",
      gtk_window_is_maximized(GTK_WINDOW(self)));

    g_clear_object(&self->settings);
  }

  // Clean up EchoLink directory
  if (self->directory != nullptr)
  {
    self->directory->makeOffline();
    delete self->directory;
    self->directory = nullptr;
  }

  g_clear_object(&self->selection_model);
  g_clear_object(&self->filter_model);
  g_clear_object(&self->station_model);
  g_clear_pointer(&self->search_text, g_free);

  G_OBJECT_CLASS(qtel_window_parent_class)->dispose(object);
}

static void
qtel_window_class_init(QtelWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->constructed = qtel_window_constructed;
  object_class->dispose = qtel_window_dispose;
}

static void
qtel_window_init(QtelWindow *self)
{
  // Initialize model pointers
  self->station_model = nullptr;
  self->filter_model = nullptr;
  self->selection_model = nullptr;
  self->search_text = nullptr;
  self->current_category = CATEGORY_STATIONS;
  self->directory = nullptr;
  self->is_refreshing = FALSE;
  self->refresh_spinner = nullptr;

  // Initialize settings
  self->settings = g_settings_new(APP_ID);

  // Initialize EchoLink directory
  Settings *app_settings = settings_get_default();
  const gchar *callsign = settings_get_callsign(app_settings);
  const gchar *password = settings_get_password(app_settings);
  const gchar *location = settings_get_location(app_settings);
  const gchar *servers_str = settings_get_directory_servers(app_settings);

  // Check if we have credentials
  if (callsign != nullptr && callsign[0] != '\0' &&
      password != nullptr && password[0] != '\0')
  {
    // Parse servers string into vector
    std::vector<std::string> servers;
    std::string servers_s(servers_str ? servers_str : "");
    std::istringstream iss(servers_s);
    std::string server;
    while (iss >> server)
    {
      if (!server.empty())
      {
        servers.push_back(server);
      }
    }

    if (servers.empty())
    {
      // Default servers
      servers.push_back("nasouth.echolink.org");
      servers.push_back("naeast.echolink.org");
      servers.push_back("nawest.echolink.org");
      servers.push_back("servers.echolink.org");
    }

    g_message("Initializing EchoLink directory with callsign: %s", callsign);

    self->directory = new EchoLink::Directory(
      servers,
      std::string(callsign),
      std::string(password),
      std::string(location ? location : "")
    );

    // Connect signals
    self->directory->statusChanged.connect(
      sigc::bind(sigc::ptr_fun(on_directory_status_changed), self));
    self->directory->stationListUpdated.connect(
      sigc::bind(sigc::ptr_fun(on_directory_station_list_updated), self));
    self->directory->error.connect(
      sigc::bind(sigc::ptr_fun(on_directory_error), self));

    // Check start as busy setting
    gboolean start_busy = settings_get_start_as_busy(app_settings);
    if (start_busy)
    {
      self->directory->makeBusy();
    }
    else
    {
      self->directory->makeOnline();
    }
  }
  else
  {
    g_message("No EchoLink credentials configured - directory disabled");
  }

  // Restore window state
  int width = g_settings_get_int(self->settings, "window-width");
  int height = g_settings_get_int(self->settings, "window-height");
  gtk_window_set_default_size(GTK_WINDOW(self), width, height);

  if (g_settings_get_boolean(self->settings, "window-maximized"))
  {
    gtk_window_maximize(GTK_WINDOW(self));
  }

  // Set window title
  gtk_window_set_title(GTK_WINDOW(self), "Qtel");

  // Create the main layout using AdwToolbarView
  GtkWidget *toolbar_view = adw_toolbar_view_new();

  // Add header bar
  self->header_bar = create_header_bar(self);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), self->header_bar);

  // Create horizontal paned for sidebar/content (like Qt's QSplitter)
  self->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_shrink_start_child(GTK_PANED(self->main_paned), FALSE);
  gtk_paned_set_shrink_end_child(GTK_PANED(self->main_paned), FALSE);
  gtk_paned_set_position(GTK_PANED(self->main_paned), 250); // Default sidebar width

  // Sidebar (left side - station list)
  GtkWidget *sidebar = create_sidebar(self);
  gtk_paned_set_start_child(GTK_PANED(self->main_paned), sidebar);

  // Content (right side - messages/incoming)
  GtkWidget *content = create_content(self);
  gtk_paned_set_end_child(GTK_PANED(self->main_paned), content);

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), self->main_paned);

  // Wrap in toast overlay for notifications
  GtkWidget *toast_overlay = adw_toast_overlay_new();
  adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(toast_overlay), toolbar_view);

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(self), toast_overlay);
}

QtelWindow *
qtel_window_new(QtelApplication *app)
{
  return static_cast<QtelWindow*>(
    g_object_new(QTEL_TYPE_WINDOW,
                 "application", app,
                 nullptr)
  );
}
