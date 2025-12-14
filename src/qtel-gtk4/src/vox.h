/**
@file    vox.h
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

#ifndef VOX_H
#define VOX_H

#include <glib.h>
#include <sigc++/sigc++.h>

// Note: This class would normally inherit from Async::AudioSink
// For now, we create a standalone version that can be called manually
// with audio samples. Integration with the async audio system will
// be done when connecting to the actual EchoLink backend.

/**
 * VOX State enumeration
 */
enum class VoxState {
  IDLE,
  ACTIVE,
  HANG
};

/**
 * @brief Voice Operated Transmission (VOX) controller
 *
 * This class implements VOX logic - detecting voice activity
 * and controlling PTT based on audio level.
 */
class Vox : public sigc::trackable
{
public:
  /**
   * @brief Constructor
   */
  Vox();

  /**
   * @brief Destructor
   */
  ~Vox();

  /**
   * @brief Get the current VOX state
   * @return Current state (IDLE, ACTIVE, or HANG)
   */
  VoxState state() const { return m_state; }

  /**
   * @brief Check if VOX is enabled
   * @return true if enabled
   */
  bool enabled() const { return m_enabled; }

  /**
   * @brief Get the threshold level
   * @return Threshold in dB [-60, 0]
   */
  int threshold() const { return m_threshold; }

  /**
   * @brief Get the hang delay
   * @return Delay in milliseconds
   */
  int delay() const { return m_delay; }

  /**
   * @brief Set whether VOX is enabled
   * @param enable true to enable
   */
  void setEnabled(bool enable);

  /**
   * @brief Set the threshold level
   * @param threshold_db Threshold in dB [-60, 0]
   */
  void setThreshold(int threshold_db);

  /**
   * @brief Set the hang delay
   * @param delay_ms Delay in milliseconds
   */
  void setDelay(int delay_ms);

  /**
   * @brief Process audio samples
   * @param samples Audio sample buffer
   * @param count Number of samples
   * @return Number of samples processed
   *
   * This would normally be called from Async::AudioSink::writeSamples
   */
  int writeSamples(const float *samples, int count);

  /**
   * @brief Flush samples (required by AudioSink interface)
   */
  void flushSamples() { /* Nothing to do */ }

  // Signals
  sigc::signal<void(int)> levelChanged;        ///< Level in dB [-60, 0]
  sigc::signal<void(VoxState)> stateChanged;   ///< State change notification

private:
  bool m_enabled;
  int m_threshold;
  int m_delay;
  VoxState m_state;
  guint m_timer_id;

  void setState(VoxState new_state);
  static gboolean onTimeout(gpointer user_data);

  // Non-copyable
  Vox(const Vox&) = delete;
  Vox& operator=(const Vox&) = delete;
};

#endif /* VOX_H */
