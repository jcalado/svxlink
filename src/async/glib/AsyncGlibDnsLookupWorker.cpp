/**
@file    AsyncGlibDnsLookupWorker.cpp
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

/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <sigc++/sigc++.h>
#include <glib.h>
#include <gio/gio.h>

#include <iostream>
#include <cassert>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncDnsResourceRecord.h>
#include <AsyncDnsLookup.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "AsyncGlibDnsLookupWorker.h"


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

GlibDnsLookupWorker::GlibDnsLookupWorker(const DnsLookup& dns)
  : DnsLookupWorker(dns), m_cancellable(nullptr), m_resolver(nullptr)
{
} /* GlibDnsLookupWorker::GlibDnsLookupWorker */


GlibDnsLookupWorker::~GlibDnsLookupWorker(void)
{
  abortLookup();
} /* GlibDnsLookupWorker::~GlibDnsLookupWorker */


DnsLookupWorker& GlibDnsLookupWorker::operator=(DnsLookupWorker&& other_base)
{
  this->DnsLookupWorker::operator=(std::move(other_base));

  auto& other = static_cast<GlibDnsLookupWorker&>(other_base);

  other.abortLookup();
  m_cancellable = nullptr;
  m_resolver = nullptr;

  doLookup();

  return *this;
} /* GlibDnsLookupWorker::operator=(DnsLookupWorker&&) */


bool GlibDnsLookupWorker::doLookup(void)
{
  assert(dns().type() == DnsLookup::Type::A);

  m_resolver = g_resolver_get_default();
  m_cancellable = g_cancellable_new();

  g_resolver_lookup_by_name_async(
      m_resolver,
      dns().label().c_str(),
      m_cancellable,
      onResolverFinished,
      this
  );

  return true;
} /* GlibDnsLookupWorker::doLookup */


void GlibDnsLookupWorker::abortLookup(void)
{
  if (m_cancellable != nullptr)
  {
    g_cancellable_cancel(m_cancellable);
    g_object_unref(m_cancellable);
    m_cancellable = nullptr;
  }

  if (m_resolver != nullptr)
  {
    g_object_unref(m_resolver);
    m_resolver = nullptr;
  }
} /* GlibDnsLookupWorker::abortLookup */


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

void GlibDnsLookupWorker::onResolverFinished(GObject* source_object,
                                              GAsyncResult* res,
                                              gpointer user_data)
{
  GlibDnsLookupWorker* self = static_cast<GlibDnsLookupWorker*>(user_data);
  GResolver* resolver = G_RESOLVER(source_object);
  GError* error = nullptr;

  GList* addresses = g_resolver_lookup_by_name_finish(resolver, res, &error);

  // Clean up the cancellable
  if (self->m_cancellable != nullptr)
  {
    g_object_unref(self->m_cancellable);
    self->m_cancellable = nullptr;
  }

  if (self->m_resolver != nullptr)
  {
    g_object_unref(self->m_resolver);
    self->m_resolver = nullptr;
  }

  if (error != nullptr)
  {
    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      std::cerr << "*** ERROR: DNS lookup error: " << error->message
                << std::endl;
      self->setLookupFailed(true);
    }
    g_error_free(error);
    self->workerDone();
    return;
  }

  // Process the results
  for (GList* l = addresses; l != nullptr; l = l->next)
  {
    GInetAddress* addr = G_INET_ADDRESS(l->data);

    // Only process IPv4 addresses for now
    if (g_inet_address_get_family(addr) == G_SOCKET_FAMILY_IPV4)
    {
      gchar* addr_str = g_inet_address_to_string(addr);
      IpAddress ip(addr_str);
      g_free(addr_str);

      // TTL of 1 second as GResolver doesn't provide TTL information
      self->addResourceRecord(new DnsResourceRecordA(self->dns().label(), 1, ip));
    }
  }

  g_resolver_free_addresses(addresses);

  self->workerDone();
} /* GlibDnsLookupWorker::onResolverFinished */


/*
 * This file has not been truncated
 */
