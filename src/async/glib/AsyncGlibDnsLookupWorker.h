/**
@file    AsyncGlibDnsLookupWorker.h
@brief   Execute DNS queries in the GLib environment
@author  Joel (GTK4 port)
@date    2024-12-14

This file contains a class for executing DNS queries in the GLib variant of
the async environment. This class should never be used directly. It is
used by Async::GlibApplication to execute DNS queries.

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


#ifndef ASYNC_GLIB_DNS_LOOKUP_WORKER_INCLUDED
#define ASYNC_GLIB_DNS_LOOKUP_WORKER_INCLUDED


/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <sigc++/sigc++.h>
#include <glib.h>
#include <gio/gio.h>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "../core/AsyncDnsLookupWorker.h"


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
 * Defines & typedefs
 *
 ****************************************************************************/

class DnsLookup;


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
@brief  DNS lookup worker for the GLib variant of the async environment
@author Joel
@date   2024-12-14

This is the DNS lookup worker for the GLib variant of the async environment.
It is an internal class that should only be used from within the async
library.
*/
class GlibDnsLookupWorker : public DnsLookupWorker
{
  public:
    /**
     * @brief   Constructor
     * @param   dns The lookup object
     */
    GlibDnsLookupWorker(const DnsLookup& dns);

    /**
     * @brief   Destructor
     */
    virtual ~GlibDnsLookupWorker(void);

    /**
     * @brief   Move assignment operator
     * @param   other The other object to move data from
     * @return  Returns this object
     */
    virtual DnsLookupWorker& operator=(DnsLookupWorker&& other_base) override;

  protected:
    /**
     * @brief   Called by the DnsLookupWorker class to start the lookup
     * @return  Return \em true on success or else \em false
     */
    virtual bool doLookup(void) override;

    /**
     * @brief   Called by the DnsLookupWorker class to abort a pending lookup
     */
    virtual void abortLookup(void) override;

  private:
    GCancellable*   m_cancellable;
    GResolver*      m_resolver;

    static void onResolverFinished(GObject* source_object,
                                   GAsyncResult* res,
                                   gpointer user_data);

};  /* class GlibDnsLookupWorker */


} /* namespace */

#endif /* ASYNC_GLIB_DNS_LOOKUP_WORKER_INCLUDED */


/*
 * This file has not been truncated
 */
