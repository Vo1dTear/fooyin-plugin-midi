// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MIDIPlayer.h"
#include <functional>

// Without a sender this is a silent decoder: prebuffering and conversion must
// never send MIDI. Only the playback-clock controller enables the sender.
class ExternalMIDIPlayer final : public MIDIPlayer {
public:
    using Sender = std::function<void(const uint8_t*, size_t)>;
    ~ExternalMIDIPlayer() override;
    bool enableOutput(Sender sender);
    void silence();
    void setOutputVolume(double volume);
    static constexpr unsigned StartupMilliseconds = 100;
    unsigned long Play(float*, unsigned long) override;
    void Seek(unsigned long) override;
    unsigned long Tell() const override;
protected:
    bool startup() override;
    void shutdown() override;
    void dispatchMidi(const uint8_t*, size_t, uint32_t, unsigned) override;
    void renderChunk(float*, uint32_t) override;
    bool get_last_error(std::string&) override;
private:
    void primeSetup();
    void sendVolume();
    double outputVolume = 1.0;
    double songVolume = 1.0;
    unsigned long startupFrames() const;
    unsigned long prefixFrames = 0;
    bool prepared = false;
    std::vector<std::vector<uint8_t>> primed;
    size_t primedIndex = 0;
    unsigned sourcePort = 0;
    Sender send;
    std::string error;
};
