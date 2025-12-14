/**
@file    qtel-call-dialog.h
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

#ifndef QTEL_CALL_DIALOG_H
#define QTEL_CALL_DIALOG_H

#include <adwaita.h>

#ifdef __cplusplus
#include <EchoLinkQso.h>
#include <AsyncAudioIO.h>
#include <AsyncAudioFifo.h>
#include <AsyncAudioValve.h>
#include <AsyncAudioSplitter.h>
#include <AsyncDnsLookup.h>
#include <AsyncIpAddress.h>
#endif

G_BEGIN_DECLS

#define QTEL_TYPE_CALL_DIALOG (qtel_call_dialog_get_type())

G_DECLARE_FINAL_TYPE(QtelCallDialog, qtel_call_dialog, QTEL, CALL_DIALOG, AdwWindow)

/**
 * @brief Create a new call dialog for a station
 * @param parent Parent window
 * @param callsign Station callsign
 * @param description Station description
 * @param node_id EchoLink node ID
 * @param ip_address Station IP address
 * @return A new QtelCallDialog
 */
QtelCallDialog *qtel_call_dialog_new(GtkWindow *parent,
                                      const gchar *callsign,
                                      const gchar *description,
                                      gint node_id,
                                      const gchar *ip_address);

/**
 * @brief Create a new call dialog for connecting by IP
 * @param parent Parent window
 * @param host Hostname or IP address
 * @return A new QtelCallDialog
 */
QtelCallDialog *qtel_call_dialog_new_from_host(GtkWindow *parent,
                                                const gchar *host);

/**
 * @brief Accept an incoming connection
 * @param self The call dialog
 */
void qtel_call_dialog_accept(QtelCallDialog *self);

G_END_DECLS

#endif /* QTEL_CALL_DIALOG_H */
