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
#include "NukedSC55Player.h"

#include <backend/emu.h>
#include <common/rom_loader.h>
#include <algorithm>
#include <array>
#include <deque>
#include <stdexcept>
#include <mutex>

struct NukedSC55Player::Impl {
    struct Instance {
        // ROM storage follows the device across decoder lifetimes.
        std::shared_ptr<const common::LoadRomsetResult> roms;
        size_t generation = 0;
        Emulator emulator;
        std::deque<AudioFrame<float>> audio;
        std::deque<uint8_t> midi;

        static void sample(void* context, const AudioFrame<int32_t>& frame) {
            AudioFrame<float> normalized;
            Normalize(frame, normalized);
            static_cast<Instance*>(context)->audio.push_back(normalized);
        }

        void step() {
            auto& mcu = emulator.GetMCU();
            while(!midi.empty() && (mcu.uart_write_ptr + 1) % uart_buffer_size != mcu.uart_read_ptr) {
                emulator.PostMIDI(midi.front());
                midi.pop_front();
            }
            emulator.Step();
        }

        AudioFrame<float> next() {
            for(unsigned steps = 0; audio.empty(); ++steps) {
                if(steps == 1000000)
                    throw std::runtime_error("Nuked-SC55 stopped producing audio");
                step();
            }
            auto frame = audio.front();
            audio.pop_front();
            return frame;
        }
    };
    static std::unique_ptr<Instance> boot(std::shared_ptr<const common::LoadRomsetResult> roms) {
        auto instance = std::make_unique<Instance>();
        instance->roms = std::move(roms);
        auto& emulator = instance->emulator;
        if(!emulator.Init({}) || !emulator.LoadRoms(instance->roms->romset, instance->roms->romset_info))
            throw std::runtime_error("Nuked-SC55 could not start the emulator");
        emulator.Reset();
        emulator.GetPCM().enable_oversampling = true;
        emulator.PostSystemReset(EMU_SystemReset::GS_RESET);
        for(unsigned step = 0; step < 24000000; ++step) {
            emulator.Step();
        }
        // Attach callbacks only after a decoder owns the device.
        return instance;
    }

    static void resetForTrack(Instance& instance) {
        instance.midi.clear();
        instance.audio.clear();
        auto& mcu = instance.emulator.GetMCU();
        // Drop bytes not yet delivered by the previous decoder. End a possibly
        // interrupted SysEx before silencing channels and restoring GS defaults.
        mcu.uart_read_ptr = mcu.uart_write_ptr;
        instance.midi.push_back(0xf7);
        for(unsigned channel = 0; channel < 16; ++channel) {
            for(uint8_t control : {64, 120, 123}) {
                instance.midi.push_back(uint8_t(0xb0 | channel));
                instance.midi.push_back(control);
                instance.midi.push_back(0);
            }
        }
        instance.midi.insert(instance.midi.end(), syx_reset_gs, syx_reset_gs + 11);
        instance.emulator.SetSampleCallback(&Instance::sample, &instance);
        // Advance the firmware while discarding reset audio. Unlike booting,
        // this keeps the emulated device powered on between songs.
        const auto rate = PCM_GetOutputFrequency(instance.emulator.GetPCM());
        for(unsigned i = 0; i < rate / 2; ++i) instance.next();
        if(!instance.midi.empty() || mcu.uart_read_ptr != mcu.uart_write_ptr)
            throw std::runtime_error("Nuked-SC55 timed out resetting the persistent device");
        instance.audio.clear();
    }

    struct DevicePool {
        std::mutex mutex;
        bool enabled = true;
        size_t generation = 0;
        std::unique_ptr<Instance> idle;

        bool matches(const common::LoadRomsetResult& candidate) const {
            if(!idle || idle->roms->romset != candidate.romset) return false;
            for(size_t i = 0; i < ROMLOCATION_COUNT; ++i)
                if(idle->roms->romset_info.rom_data[i] != candidate.romset_info.rom_data[i]) return false;
            return true;
        }

        std::unique_ptr<Instance> take(std::shared_ptr<const common::LoadRomsetResult> source) {
            std::unique_ptr<Instance> instance;
            size_t leaseGeneration;
            {
                std::lock_guard lock(mutex);
                leaseGeneration = generation;
                if(enabled && matches(*source)) instance = std::move(idle);
                else idle.reset();
            }
            if(instance) resetForTrack(*instance);
            else instance = boot(std::move(source));
            instance->generation = leaseGeneration;
            return instance;
        }

        void release(std::unique_ptr<Instance> instance) {
            if(!instance) return;
            instance->emulator.SetSampleCallback(nullptr, nullptr);
            std::lock_guard lock(mutex);
            // Late decoders from an old configuration must not repopulate the pool.
            if(enabled && instance->generation == generation && !idle)
                idle = std::move(instance);
        }
    };

    static DevicePool& pool() {
        static DevicePool value;
        return value;
    }

    struct Event {
        std::vector<uint8_t> bytes;
        uint32_t offset;
        unsigned port;
    };
    // ROM buffers must outlive every emulator.
    std::shared_ptr<const common::LoadRomsetResult> roms;
    std::array<std::unique_ptr<Instance>, 32> instances;
    std::vector<Event> events;
    std::deque<Event> primed;
    std::string error;
    bool ready = false;
    unsigned rate = 0;

    void post(const Event& event) {
        if(event.port < instances.size() && instances[event.port]) {
            auto& queue = instances[event.port]->midi;
            queue.insert(queue.end(), event.bytes.begin(), event.bytes.end());
        }
    }
};

NukedSC55Player::NukedSC55Player() : impl{std::make_unique<Impl>()} {}
NukedSC55Player::~NukedSC55Player() { shutdown(); }

bool NukedSC55Player::prepare(const std::string& directory, const std::string& romset) {
    shutdown();
    impl->ready = false;
    impl->error.clear();
    try {
        if(directory.empty() || !std::filesystem::is_directory(directory))
            throw std::runtime_error("Select a Nuked-SC55 ROM directory in MIDI Input settings");
        auto loadedRoms = std::make_shared<common::LoadRomsetResult>();
        const auto result = common::LoadRomset(directory, romset, common::RomLoader::Hashing, {}, *loadedRoms);
        impl->roms = std::move(loadedRoms);
        if(result != common::LoadRomsetError{})
            throw std::runtime_error(std::string("Nuked-SC55: ") + common::ToCString(result));
        if(impl->roms->romset != Romset::MK1 && impl->roms->romset != Romset::MK2)
            throw std::runtime_error("Select an SC-55 (mk1) or SC-55mkII (mk2) ROM set");
        // These are the backend's fixed oversampled rates for the two supported
        // models. Avoid allocating a second device merely to query its format.
        impl->rate = impl->roms->romset == Romset::MK1 ? 64000 : 66207;
        setSampleRate(impl->rate);
        impl->ready = true;
        return true;
    } catch(const std::exception& e) {
        impl->error = e.what();
        return false;
    }
}

unsigned NukedSC55Player::sampleRate() const { return impl->rate; }

void NukedSC55Player::Seek(unsigned long sample) {
    // A restart after read-ahead must use the same initial setup preparation as
    // a newly opened track. Ordinary seek restoration bypasses that preparation.
    if(sample == 0 && Tell() != 0) {
        restartPlayback();
        return;
    }
    MIDIPlayer::Seek(sample);
}

bool NukedSC55Player::startup() {
    if(initialized) return true;
    if(!impl->ready || !impl->error.empty()) return false;
    try {
        for(unsigned port = 0; port < impl->instances.size(); ++port) {
            if(!(port_mask & (1u << port))) continue;
            auto instance = Impl::pool().take(impl->roms);
            if(PCM_GetOutputFrequency(instance->emulator.GetPCM()) != impl->rate)
                throw std::runtime_error("Nuked-SC55 output rate does not match the decoder format");
            impl->roms = instance->roms;
            instance->emulator.SetSampleCallback(&Impl::Instance::sample, instance.get());
            impl->instances[port] = std::move(instance);
        }
        initialized = true;
        return true;
    } catch(const std::exception& e) {
        impl->error = e.what();
        shutdown();
        return false;
    }
}

void NukedSC55Player::shutdown() {
    for(auto& instance : impl->instances) {
        if(impl->error.empty()) Impl::pool().release(std::move(instance));
        else instance.reset();
    }
    impl->events.clear();
    impl->primed.clear();
    initialized = false;
}

bool NukedSC55Player::prepareInitialPlayback() {
    try {
        primeInitialSetup();
        return true;
    } catch(const std::exception& e) {
        impl->error = e.what();
        shutdown();
        return false;
    }
}

void NukedSC55Player::primeInitialSetup() {
    // Only the initial, default-filter playback has a direct tick-zero mapping.
    // Seeks reconstruct their own state; format-2 and forced resets keep their order.
    if(!midi_file || midi_file->format == 2 || mode != filter_default || samples_rendered != 0) return;
    std::array<std::array<bool, 16>, 32> hasNote{};
    bool anyNote = false;
    std::array<size_t, 32> bytes{};
    for(size_t i = 0; i < midi_file->timeline_count; ++i) {
        const auto& event = midi_file->timeline[i];
        if(event.ticks != 0) break;
        unsigned port = 0;
        if(midi_file->is_multi_port && midi_file->port_channel_offset_map &&
           event.track_index < midi_file->track_count) {
            const int source = midi_file->tracks[event.track_index].port;
            if(source >= 0 && size_t(source) < midi_file->port_channel_offset_map_count)
                port = unsigned(midi_file->port_channel_offset_map[source] / 16);
        }
        if(port >= impl->instances.size() || !impl->instances[port]) continue;
        const auto kind = event.status_byte & 0xf0;
        const auto channel = event.status_byte & 0x0f;
        if(kind == 0x80 || kind == 0x90) { hasNote[port][channel] = true; anyNote = true; continue; }
        // Do not reorder setup across a global SysEx command.
        if(event.status_byte == 0xf0 || event.status_byte == 0xf7) break;
        if(kind != 0xb0 && kind != 0xc0 && kind != 0xd0 && kind != 0xe0) continue;
        if(hasNote[port][channel]) continue;
        if(kind == 0xb0 && event.data_length && event.data[0] >= 120 && anyNote) break;
        if(bytes[port] + event.data_length + 1 > 200) break;
        Impl::Event initial{{event.status_byte}, 0, port};
        initial.bytes.insert(initial.bytes.end(), event.data, event.data + event.data_length);
        bytes[port] += initial.bytes.size();
        impl->post(initial);
        impl->primed.push_back(std::move(initial));
    }
    // Drain the initial serial burst before any notes. This advances only the
    // hardware; it neither adds silence nor shortens the sequencer timeline.
    for(unsigned port = 0; port < impl->instances.size(); ++port) {
        if(!bytes[port]) continue;
        auto& instance = *impl->instances[port];
        for(unsigned i = 0; i < impl->rate / 10; ++i) instance.next();
        instance.audio.clear();
        const auto& mcu = instance.emulator.GetMCU();
        if(!instance.midi.empty() || mcu.uart_read_ptr != mcu.uart_write_ptr)
            throw std::runtime_error("Nuked-SC55 timed out preparing initial MIDI controls");
    }
}

void NukedSC55Player::dispatchMidi(const uint8_t* data, size_t length,
                                  uint32_t sample_offset, unsigned port) {
    if(!impl->primed.empty()) {
        const auto& initial = impl->primed.front();
        if(initial.port == port && initial.bytes.size() == length &&
           std::equal(initial.bytes.begin(), initial.bytes.end(), data)) {
            impl->primed.pop_front();
            return;
        }
    }
    if(length) impl->events.push_back({{data, data + length}, sample_offset, port});
}

void NukedSC55Player::renderChunk(float* out, uint32_t count) {
    std::fill_n(out, count * 2, 0.0f);
    if(!initialized) return;
    try {
        std::stable_sort(impl->events.begin(), impl->events.end(),
                         [](const auto& a, const auto& b) { return a.offset < b.offset; });
        size_t event = 0;
        for(uint32_t frame = 0; frame < count; ++frame) {
            while(event < impl->events.size() && impl->events[event].offset <= frame)
                impl->post(impl->events[event++]);
            for(auto& instance : impl->instances) {
                if(!instance) continue;
                const auto audio = instance->next();
                out[frame * 2] += audio.left;
                out[frame * 2 + 1] += audio.right;
            }
        }
        impl->events.clear();
    } catch(const std::exception& e) {
        impl->error = e.what();
        shutdown();
    }
}

void NukedSC55Player::finishSeek() {
    try {
        // Let the serial input consume reconstructed controllers/SysEx before resuming.
        for(const auto& event : impl->events) impl->post(event);
        impl->events.clear();
        for(auto& instance : impl->instances) {
            if(!instance) continue;
            auto& mcu = instance->emulator.GetMCU();
            unsigned steps = 0;
            while(!instance->midi.empty() || mcu.uart_read_ptr != mcu.uart_write_ptr) {
                instance->step();
                instance->audio.clear();
                if(++steps == 24000000) {
                    impl->error = "Nuked-SC55 timed out restoring MIDI state";
                    shutdown();
                    return;
                }
            }
            // Allow the firmware to apply the last received command.
            for(unsigned i = 0; i < impl->rate / 10; ++i) instance->next();
            instance->audio.clear();
        }
    } catch(const std::exception& e) {
        impl->error = e.what();
        shutdown();
    }
}

bool NukedSC55Player::get_last_error(std::string& out) {
    out = impl->error;
    return !out.empty();
}

void NukedSC55Player::setPersistenceEnabled(bool enabled) {
    auto& pool = Impl::pool();
    std::lock_guard lock(pool.mutex);
    pool.enabled = enabled;
    if(!enabled) {
        ++pool.generation;
        pool.idle.reset();
    }
}

bool NukedSC55Player::hasIdleDevice() {
    auto& pool = Impl::pool();
    std::lock_guard lock(pool.mutex);
    return bool(pool.idle);
}
