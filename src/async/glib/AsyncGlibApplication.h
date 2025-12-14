/**
@file    AsyncGlibApplication.h
@brief   The core class for writing asynchronous GLib/GTK applications
@author  Joel (GTK4 port)
@date    2024-12-14

This file contains the AsyncGlibApplication class which is the core of an
application that uses the Async classes in a GLib/GTK application.

\verbatim
Async - A library for programming event driven applications
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

#ifndef ASYNC_GLIB_APPLICATION_INCLUDED
#define ASYNC_GLIB_APPLICATION_INCLUDED

/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <sigc++/sigc++.h>
#include <glib.h>

#include <utility>
#include <map>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncApplication.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Forward declarations
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Namespace
 *
 ****************************************************************************/

namespace Async
{

/****************************************************************************
 *
 * Forward declarations of classes inside of the declared namespace
 *
 ****************************************************************************/

class DnsLookup;


/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Class definitions
 *
 ****************************************************************************/

/**
@brief  An application class for writing GUI applications using GLib/GTK.
@author Joel
@date   2024-12-14

This class is used when writing GLib or GTK applications. It should be one of
the first things done in the \em main function of your application. Use the
Async::GlibApplication class to integrate the Async library with the GLib
main loop.

In the application you can mix Async timers and file descriptor watches with
GLib sources and GTK widgets.
*/
class GlibApplication : public Application
{
  public:
    /**
     * @brief Constructor
     *
     * The two arguments typically are the arguments given to the \em main
     * function.
     * @param argc  The number of command line arguments + 1
     * @param argv  An array containing the commandline arguments
     */
    GlibApplication(int &argc, char **argv);

    /**
     * @brief Destructor
     */
    virtual ~GlibApplication(void);

    /**
     * @brief Execute the application main loop
     *
     * When this member function is called the application core will enter the
     * main loop. It will not exit from this loop until the
     * Async::Application::quit method is called.
     */
    void exec(void) override;

    /**
     * @brief Exit the application main loop
     *
     * This function should be called to exit the application core main loop.
     */
    void quit(void) override;

    /**
     * @brief Get the GLib main loop
     * @return Returns a pointer to the GMainLoop
     *
     * This function returns the GLib main loop used by this application.
     * This can be useful when integrating with GTK or other GLib-based
     * libraries.
     */
    GMainLoop* mainLoop(void) const { return m_main_loop; }

    /**
     * @brief Get the GLib main context
     * @return Returns a pointer to the GMainContext
     */
    GMainContext* mainContext(void) const { return m_main_context; }

  protected:

  private:
    struct FdWatchData
    {
      FdWatch*  fd_watch;
      guint     source_id;
    };

    struct TimerData
    {
      Timer*  timer;
      guint   source_id;
    };

    typedef std::map<int, FdWatchData>      FdWatchMap;
    typedef std::map<Timer*, TimerData>     TimerMap;

    GMainLoop*    m_main_loop;
    GMainContext* m_main_context;
    FdWatchMap    m_rd_watch_map;
    FdWatchMap    m_wr_watch_map;
    TimerMap      m_timer_map;

    void addFdWatch(FdWatch *fd_watch) override;
    void delFdWatch(FdWatch *fd_watch) override;
    void addTimer(Timer *timer) override;
    void delTimer(Timer *timer) override;
    DnsLookupWorker *newDnsLookupWorker(const DnsLookup& lookup) override;

    static gboolean onFdActivity(gint fd, GIOCondition condition,
                                 gpointer user_data);
    static gboolean onTimerExpired(gpointer user_data);

};  /* class GlibApplication */


} /* namespace */

#endif /* ASYNC_GLIB_APPLICATION_INCLUDED */


/*
 * This file has not been truncated
 */
