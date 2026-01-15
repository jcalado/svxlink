/**
@file    qtel-window.h
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

#ifndef QTEL_WINDOW_H
#define QTEL_WINDOW_H

#include <adwaita.h>
#include "qtel-application.h"

#ifdef __cplusplus
#include <EchoLinkDirectory.h>
#endif

G_BEGIN_DECLS

#define QTEL_TYPE_WINDOW (qtel_window_get_type())

G_DECLARE_FINAL_TYPE(QtelWindow, qtel_window, QTEL, WINDOW, AdwApplicationWindow)

/**
 * @brief Create a new QtelWindow instance
 * @param app The application instance
 * @return A new QtelWindow
 */
QtelWindow *qtel_window_new(QtelApplication *app);

G_END_DECLS

#endif /* QTEL_WINDOW_H */
