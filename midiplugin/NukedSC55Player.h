// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Nuked-SC55 integration for fooyin-plugin-midi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details:
 * <https://www.gnu.org/licenses/gpl-3.0.html>.
 *
 * The Nuked-SC55 backend is a separate dependency by nukeykt, J.C. Moyer
 * and other contributors, licensed under GPL-2.0-or-later:
 * <https://github.com/jcmoyer/Nuked-SC55>.
 * Its original copyright and license notices remain in the upstream
 * sources; see the LICENSE file in that dependency. The SPDX identifier
 * above applies to this integration file.
 *
 * SC-55 ROM images are not included. The software licenses do not grant
 * rights to redistribute those ROM images.
 */
#pragma once

#include "MIDIPlayer.h"
#include <memory>

class NukedSC55Player final : public MIDIPlayer {
public:
    NukedSC55Player();
    ~NukedSC55Player() override;

    // Detect and validate ROMs before returning the decoder's audio format.
    bool prepare(const std::string& directory, const std::string& romset);
    unsigned sampleRate() const;
    void Seek(unsigned long sample) override;
    // Release the idle device and invalidate active leases on configuration changes.
    static void setPersistenceEnabled(bool enabled);
    static bool hasIdleDevice();

protected:
    bool startup() override;
    void shutdown() override;
    void dispatchMidi(const uint8_t* data, size_t length,
                      uint32_t sample_offset, unsigned port) override;
    void renderChunk(float* out, uint32_t sample_count) override;
    void finishSeek() override;
    bool prepareInitialPlayback() override;
    bool get_last_error(std::string& out) override;

private:
    void primeInitialSetup();
    struct Impl;
    std::unique_ptr<Impl> impl;
};
