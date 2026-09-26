// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExternalMIDIPlayer.h"
#include <algorithm>
#include <exception>
#include <array>
#include <cmath>

ExternalMIDIPlayer::~ExternalMIDIPlayer() { silence(); }

void ExternalMIDIPlayer::setOutputVolume(double volume) {
    if(!std::isfinite(volume)) return;
    outputVolume = std::clamp(volume, 0.0, 1.0);
    if(!initialized || !send || !error.empty()) return;
    try { sendVolume(); }
    catch(const std::exception& e) { error = e.what(); initialized = false; }
}

void ExternalMIDIPlayer::sendVolume() {
    // Fooyin's logarithmic slider spans amplitude 0.01..1 (-40..0 dB).
    // Map its travel to GS units instead of treating amplitude as a MIDI value.
    // This is a control-range mapping, not an exact SC-55 dB calibration.
    // Reserve zero for mute; retain the song's master level and channel mix.
    const double position = outputVolume > 0.0
        ? std::clamp(1.0 + std::log10(outputVolume) / 2.0, 0.0, 1.0) : 0.0;
    const double level = outputVolume > 0.0 ? 1.0 + 126.0 * position : 0.0;
    const auto value = uint8_t(level > 0.0 && songVolume > 0.0
        ? std::max(1L, std::lround(level * songVolume)) : 0L);
    const uint8_t message[] = {0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0, 4,
                              value, uint8_t((128 - ((0x44 + value) & 127)) & 127), 0xf7};
    send(message, sizeof(message));
}

bool ExternalMIDIPlayer::enableOutput(Sender sender) {
    unsigned activePorts = 0;
    if(!midi_file) { error = "Load a MIDI file before enabling external output"; return false; }
    for(size_t index = 0; index < midi_file->track_count; ++index) {
        const auto& track = midi_file->tracks[index];
        if(!track.event_count) continue;
        const bool hasMidi = std::any_of(track.events, track.events + track.event_count, [](const auto& event) {
            return event.status_byte >= 0x80 && event.status_byte <= 0xf0;
        });
        if(!hasMidi) continue;
        unsigned port = 0;
        if(midi_file->is_multi_port && midi_file->port_channel_offset_map && track.port >= 0 &&
           size_t(track.port) < midi_file->port_channel_offset_map_count)
            port = unsigned(midi_file->port_channel_offset_map[track.port] / 16);
        if(port >= 16) { error = "Unsupported MIDI source port"; return false; }
        activePorts |= 1u << port;
        sourcePort = port;
    }
    if(activePorts && (activePorts & (activePorts - 1))) {
        error = "External MIDI output supports one 16-channel MIDI port; use an internal engine for multi-port files";
        return false;
    }
    send = std::move(sender);
    return true;
}

unsigned long ExternalMIDIPlayer::startupFrames() const {
    return std::lround(dSampleRate * StartupMilliseconds / 1000.0);
}

unsigned long ExternalMIDIPlayer::Tell() const {
    return prefixFrames + MIDIPlayer::Tell();
}

unsigned long ExternalMIDIPlayer::Play(float* out, unsigned long count) {
    if(!count) return 0;
    if(!prepared) {
        if(!PreparePlayback()) return 0;
        if(prefixFrames < startupFrames()) primeSetup();
        if(!initialized) return 0;
        prepared = true;
    }
    const auto padding = std::min(count, startupFrames() - prefixFrames);
    std::fill_n(out, padding * 2, 0.0f);
    prefixFrames += padding;
    if(padding == count) return padding;
    return padding + MIDIPlayer::Play(out + padding * 2, count - padding);
}

void ExternalMIDIPlayer::Seek(unsigned long sample) {
    if(sample == 0 && Tell() == 0) return;
    // During the prefix the sequencer has not advanced: retain its queued
    // initialization and the matching suppression list.
    if(MIDIPlayer::Tell() == 0 && sample < startupFrames()) {
        prefixFrames = sample;
        return;
    }
    primed.clear();
    primedIndex = 0;
    MIDIPlayer::Seek(sample > startupFrames() ? sample - startupFrames() : 0);
    prefixFrames = std::min(sample, startupFrames());
    // The base seek already reconstructed controller/program state.
    prepared = true;
}

void ExternalMIDIPlayer::primeSetup() {
    if(!send || !midi_file || midi_file->format == 2 || mode != filter_default) return;
    std::array<bool, 16> hasNote{};
    bool anyNote = false;
    size_t bytes = 0;
    // Only move initialization before the first note of its own channel.
    // Keep global SysEx barriers intact and bound the UART preparation burst.
    for(size_t i = 0; i < midi_file->timeline_count; ++i) {
        const auto& event = midi_file->timeline[i];
        if(event.ticks != 0) break;
        const auto kind = event.status_byte & 0xf0;
        const auto channel = event.status_byte & 0x0f;
        if(kind == 0x80 || kind == 0x90) { hasNote[channel] = true; anyNote = true; continue; }
        if(event.status_byte == 0xf0 || event.status_byte == 0xf7) break;
        if(kind != 0xb0 && kind != 0xc0 && kind != 0xd0 && kind != 0xe0) continue;
        if(hasNote[channel]) continue;
        // Channel mode messages can reset state; do not move them across notes.
        if(kind == 0xb0 && event.data_length && event.data[0] >= 120 && anyNote) break;
        if(bytes + event.data_length + 1 > 200) break;
        std::vector<uint8_t> message{event.status_byte};
        message.insert(message.end(), event.data, event.data + event.data_length);
        try { send(message.data(), message.size()); }
        catch(const std::exception& e) { error = e.what(); initialized = false; return; }
        bytes += message.size();
        primed.push_back(std::move(message));
    }
}

void ExternalMIDIPlayer::silence() {
    if(!send) return;
    try {
        for(unsigned channel = 0; channel < 16; ++channel) {
            for(uint8_t control : {64, 123, 120}) {
                const uint8_t message[] = {uint8_t(0xb0 | channel), control, 0};
                send(message, sizeof(message));
            }
        }
    } catch(const std::exception& e) { error = e.what(); }
}

bool ExternalMIDIPlayer::startup() {
    if(!error.empty()) return false;
    initialized = true;
    // The target device belongs to this playback session. Restore GS defaults
    // before the file's own initialization messages (which may select GM/XG).
    if(send) dispatchMidi(syx_reset_gs, 11, 0, sourcePort);
    return initialized;
}

void ExternalMIDIPlayer::shutdown() {
    silence();
    primed.clear();
    primedIndex = 0;
    prepared = false;
    initialized = false;
}

void ExternalMIDIPlayer::dispatchMidi(const uint8_t* data, size_t length, uint32_t, unsigned port) {
    if(!send || !length || port != sourcePort || !error.empty()) return;
    if(primedIndex < primed.size() && primed[primedIndex].size() == length &&
       std::equal(primed[primedIndex].begin(), primed[primedIndex].end(), data)) {
        ++primedIndex;
        return;
    }
    try {
        const bool gs = length == 11 && data[0] == 0xf0 && data[1] == 0x41 &&
            data[3] == 0x42 && data[4] == 0x12 && data[5] == 0x40 && data[6] == 0 &&
            data[10] == 0xf7 && ((data[5] + data[6] + data[7] + data[8] + data[9]) & 127) == 0;
        const bool gmVolume = length == 8 && data[0] == 0xf0 && data[1] == 0x7f &&
            data[3] == 4 && data[4] == 1 && data[5] < 128 && data[6] < 128 && data[7] == 0xf7;
        if((gs && data[7] == 4 && data[8] < 128) || gmVolume) {
            songVolume = gmVolume ? (data[5] + 128.0 * data[6]) / 16383.0 : data[8] / 127.0;
            sendVolume();
            return;
        }
        const bool gmReset = length == 6 && data[0] == 0xf0 && data[1] == 0x7e &&
            data[3] == 9 && data[4] >= 1 && data[4] <= 3 && data[5] == 0xf7;
        const bool xgReset = length == 9 && data[0] == 0xf0 && data[1] == 0x43 &&
            data[3] == 0x4c && data[4] == 0 && data[5] == 0 && data[6] == 0x7e &&
            data[7] == 0 && data[8] == 0xf7;
        send(data, length);
        if((gs && data[7] == 0x7f && data[8] == 0) || gmReset || xgReset) {
            songVolume = 1.0;
            sendVolume();
        }
    }
    catch(const std::exception& e) { error = e.what(); initialized = false; }
}

void ExternalMIDIPlayer::renderChunk(float* out, uint32_t count) {
    std::fill_n(out, count * 2, 0.0f);
}

bool ExternalMIDIPlayer::get_last_error(std::string& out) {
    out = error;
    return !error.empty();
}
