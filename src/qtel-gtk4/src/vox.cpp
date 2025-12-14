/**
@file    vox.cpp
@brief   Voice Operated Transmission (VOX) for GTK4
@author  Joel (GTK4 port), Stuart Longland, Tobias Blomberg / SM0SVX
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

#include "vox.h"
#include <cmath>
#include <cassert>

Vox::Vox()
  : m_enabled(false),
    m_threshold(-30),
    m_delay(1000),
    m_state(VoxState::IDLE),
    m_timer_id(0)
{
}

Vox::~Vox()
{
  if (m_timer_id != 0)
  {
    g_source_remove(m_timer_id);
    m_timer_id = 0;
  }
}

void Vox::setEnabled(bool enable)
{
  m_enabled = enable;
  if (!m_enabled)
  {
    levelChanged.emit(-60);
    setState(VoxState::IDLE);
  }
}

void Vox::setThreshold(int threshold_db)
{
  if (threshold_db < -60)
  {
    m_threshold = -60;
  }
  else if (threshold_db > 0)
  {
    m_threshold = 0;
  }
  else
  {
    m_threshold = threshold_db;
  }
}

void Vox::setDelay(int delay_ms)
{
  if (delay_ms < 0)
  {
    m_delay = 0;
  }
  else
  {
    m_delay = delay_ms;
  }
}

int Vox::writeSamples(const float *samples, int count)
{
  assert(count > 0);

  if (!m_enabled)
  {
    return count;
  }

  // Calculate DC offset
  float dc_offset = 0.0f;
  for (int i = 0; i < count; i++)
  {
    dc_offset += samples[i] / count;
  }

  // Calculate absolute average level sans offset
  float avg = 0.0f;
  for (int i = 0; i < count; i++)
  {
    float sample = (samples[i] - dc_offset) / count;
    if (sample > 0)
    {
      avg += sample;
    }
    else
    {
      avg -= sample;
    }
  }

  int db_level = -60;
  if (avg > 1.0f)
  {
    db_level = 0;
  }
  else if (avg > 0.001f)
  {
    db_level = static_cast<int>(20.0f * log10f(avg));
  }
  levelChanged.emit(db_level);

  if (db_level > m_threshold)
  {
    setState(VoxState::ACTIVE);
  }
  else if (m_state == VoxState::ACTIVE)
  {
    setState(VoxState::HANG);
  }

  return count;
}

void Vox::setState(VoxState new_state)
{
  if (new_state == m_state)
  {
    return;
  }

  m_state = new_state;

  // Stop any existing timer
  if (m_timer_id != 0)
  {
    g_source_remove(m_timer_id);
    m_timer_id = 0;
  }

  // Start hang timer if entering HANG state
  if (m_state == VoxState::HANG)
  {
    m_timer_id = g_timeout_add(m_delay, onTimeout, this);
  }

  stateChanged.emit(m_state);
}

gboolean Vox::onTimeout(gpointer user_data)
{
  Vox *self = static_cast<Vox*>(user_data);
  self->m_timer_id = 0;
  self->setState(VoxState::IDLE);
  return G_SOURCE_REMOVE;
}
