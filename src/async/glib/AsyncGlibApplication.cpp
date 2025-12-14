/**
@file    AsyncGlibApplication.cpp
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


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <sigc++/sigc++.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <iostream>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncDnsLookup.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "AsyncGlibDnsLookupWorker.h"
#include "AsyncFdWatch.h"
#include "AsyncTimer.h"
#include "AsyncGlibApplication.h"


/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/

using namespace std;
using namespace Async;


/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local class definitions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Prototypes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/

GlibApplication::GlibApplication(int &argc, char **argv)
  : m_main_loop(nullptr), m_main_context(nullptr)
{
  m_main_context = g_main_context_default();
  m_main_loop = g_main_loop_new(m_main_context, FALSE);
} /* GlibApplication::GlibApplication */


GlibApplication::~GlibApplication(void)
{
  clearTasks();

  // Clean up any remaining watches
  for (auto& item : m_rd_watch_map)
  {
    if (item.second.source_id != 0)
    {
      g_source_remove(item.second.source_id);
    }
  }
  m_rd_watch_map.clear();

  for (auto& item : m_wr_watch_map)
  {
    if (item.second.source_id != 0)
    {
      g_source_remove(item.second.source_id);
    }
  }
  m_wr_watch_map.clear();

  // Clean up any remaining timers
  for (auto& item : m_timer_map)
  {
    if (item.second.source_id != 0)
    {
      g_source_remove(item.second.source_id);
    }
  }
  m_timer_map.clear();

  if (m_main_loop != nullptr)
  {
    g_main_loop_unref(m_main_loop);
    m_main_loop = nullptr;
  }
} /* GlibApplication::~GlibApplication */


void GlibApplication::exec(void)
{
  g_main_loop_run(m_main_loop);
} /* GlibApplication::exec */


void GlibApplication::quit(void)
{
  g_main_loop_quit(m_main_loop);
} /* GlibApplication::quit */


/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/

void GlibApplication::addFdWatch(FdWatch *fd_watch)
{
  GIOCondition condition;
  FdWatchMap* watch_map = nullptr;

  switch (fd_watch->type())
  {
    case FdWatch::FD_WATCH_RD:
      condition = static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR);
      watch_map = &m_rd_watch_map;
      break;

    case FdWatch::FD_WATCH_WR:
      condition = static_cast<GIOCondition>(G_IO_OUT | G_IO_ERR);
      watch_map = &m_wr_watch_map;
      break;

    default:
      return;
  }

  FdWatchData data;
  data.fd_watch = fd_watch;
  data.source_id = g_unix_fd_add(fd_watch->fd(), condition,
                                  onFdActivity, fd_watch);

  (*watch_map)[fd_watch->fd()] = data;
} /* GlibApplication::addFdWatch */


void GlibApplication::delFdWatch(FdWatch *fd_watch)
{
  FdWatchMap* watch_map = nullptr;

  switch (fd_watch->type())
  {
    case FdWatch::FD_WATCH_RD:
      watch_map = &m_rd_watch_map;
      break;

    case FdWatch::FD_WATCH_WR:
      watch_map = &m_wr_watch_map;
      break;

    default:
      return;
  }

  auto iter = watch_map->find(fd_watch->fd());
  assert(iter != watch_map->end());

  if (iter->second.source_id != 0)
  {
    g_source_remove(iter->second.source_id);
  }
  watch_map->erase(iter);
} /* GlibApplication::delFdWatch */


void GlibApplication::addTimer(Timer *timer)
{
  TimerData data;
  data.timer = timer;

  if (timer->type() == Timer::TYPE_ONESHOT)
  {
    data.source_id = g_timeout_add(timer->timeout(), onTimerExpired, timer);
  }
  else
  {
    // For periodic timers, we'll restart them in the callback
    data.source_id = g_timeout_add(timer->timeout(), onTimerExpired, timer);
  }

  m_timer_map[timer] = data;
} /* GlibApplication::addTimer */


void GlibApplication::delTimer(Timer *timer)
{
  auto iter = m_timer_map.find(timer);
  assert(iter != m_timer_map.end());

  if (iter->second.source_id != 0)
  {
    g_source_remove(iter->second.source_id);
  }
  m_timer_map.erase(iter);
} /* GlibApplication::delTimer */


DnsLookupWorker *GlibApplication::newDnsLookupWorker(const DnsLookup& lookup)
{
  return new GlibDnsLookupWorker(lookup);
} /* GlibApplication::newDnsLookupWorker */


gboolean GlibApplication::onFdActivity(gint fd, GIOCondition condition,
                                        gpointer user_data)
{
  FdWatch* fd_watch = static_cast<FdWatch*>(user_data);
  fd_watch->activity(fd_watch);

  // Return TRUE to keep the source active
  return TRUE;
} /* GlibApplication::onFdActivity */


gboolean GlibApplication::onTimerExpired(gpointer user_data)
{
  Timer* timer = static_cast<Timer*>(user_data);
  timer->expired(timer);

  // Return TRUE for periodic timers, FALSE for one-shot
  return (timer->type() == Timer::TYPE_PERIODIC) ? TRUE : FALSE;
} /* GlibApplication::onTimerExpired */


/*
 * This file has not been truncated
 */
