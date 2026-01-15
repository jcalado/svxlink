/**
@file    qtel-preferences.h
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

#ifndef QTEL_PREFERENCES_H
#define QTEL_PREFERENCES_H

#include <adwaita.h>

G_BEGIN_DECLS

#define QTEL_TYPE_PREFERENCES (qtel_preferences_get_type())

G_DECLARE_FINAL_TYPE(QtelPreferences, qtel_preferences, QTEL, PREFERENCES, AdwPreferencesWindow)

/**
 * @brief Create a new preferences window
 * @param parent Parent window
 * @return A new QtelPreferences
 */
QtelPreferences *qtel_preferences_new(GtkWindow *parent);

G_END_DECLS

#endif /* QTEL_PREFERENCES_H */
