/**
@file    main.cpp
@brief   Main entry point for Qtel GTK4 application
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

#include <glib/gi18n.h>
#include <locale.h>

#include <AsyncGlibApplication.h>

#include "qtel-config.h"
#include "qtel-application.h"

int main(int argc, char *argv[])
{
  // Set up localization
  setlocale(LC_ALL, "");
  bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  textdomain(GETTEXT_PACKAGE);

  // Initialize the Async library with GLib integration
  // This creates the Async::Application singleton which is needed
  // by EchoLink::Directory and other Async classes
  Async::GlibApplication async_app(argc, argv);

  // Create and run the GTK application
  // Note: g_application_run() uses the default GMainContext which
  // is also used by the Async::GlibApplication for its timers and watchers
  g_autoptr(QtelApplication) app = qtel_application_new();

  return g_application_run(G_APPLICATION(app), argc, argv);
}
