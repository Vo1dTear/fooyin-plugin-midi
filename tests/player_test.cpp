// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIDIPlayer.h"
#ifdef MIDI_ENABLE_EXTERNAL
#include "ExternalMIDIPlayer.h"
#endif
#ifdef MIDI_ENABLE_NUKED_SC55
#include "NukedSC55Player.h"
#endif
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}

// 480 PPQN, 120 BPM: program at 0, note at .25 s, off at .75 s, end at 1.5 s.
std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> midi(bool immediate = false) {
    uint8_t bytes[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
        'M','T','r','k',0,0,0,22,
        0,0xc0,40, 0x81,0x70,0x90,60,100,
        0x83,0x60,0x80,60,0, 0x85,0x50,0xb0,64,0, 0,0xff,0x2f,0
    };
    if(immediate) { bytes[25] = 0x80; bytes[26] = 0; } // Delta zero, including a VLQ continuation.
    auto* file = ss_file_open_from_memory(bytes, sizeof(bytes), false);
    auto* parsed = ss_midi_load(file, "mid");
    ss_file_close(file);
    require(parsed, "Cannot parse test MIDI");
    return {parsed, ss_midi_free};
}

std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> multiMidi() {
    std::vector<uint8_t> bytes{'M','T','h','d',0,0,0,6,0,1,0,2,1,0xe0};
    for(uint8_t port : {4, 9}) {
        const uint8_t track[] = {
            'M','T','r','k',0,0,0,27,
            0,0xff,0x21,1,port, 0,0xc0,40,
            0x81,0x70,0x90,uint8_t(60 + port),100,
            0x83,0x60,0x80,uint8_t(60 + port),0,
            0x85,0x50,0xb0,64,0, 0,0xff,0x2f,0
        };
        bytes.insert(bytes.end(), std::begin(track), std::end(track));
    }
    auto* file = ss_file_open_from_memory(bytes.data(), bytes.size(), false);
    auto* parsed = ss_midi_load(file, "mid");
    ss_file_close(file);
    require(parsed, "Cannot parse multi-port MIDI");
    return {parsed, ss_midi_free};
}

struct CapturePlayer : MIDIPlayer {
    struct Event { std::vector<uint8_t> data; unsigned port; uint32_t offset; unsigned long frame; };
    std::vector<Event> events;
    unsigned long frames = 0;
    unsigned starts = 0;
    unsigned ports() const { return port_mask; }
    bool startup() override { ++starts; initialized = true; return true; }
    void shutdown() override { initialized = false; }
    void dispatchMidi(const uint8_t* data, size_t size, uint32_t offset, unsigned port) override {
        events.push_back({{data, data + size}, port, offset, frames + offset});
    }
    void renderChunk(float* out, uint32_t count) override {
        std::fill_n(out, count * 2, 0.0f);
        frames += count;
    }
    // Simulate callbacks generated before the first audio block (initialization/seek).
    void queueInitialEvents() {
        pending_events.push_back({{0xf5, 3}, 0.0});
        pending_events.push_back({{0xc0, 19}, 0.0});
    }
    void queueController() { pending_events.push_back({{0xb0, 7, 99}, 0.0}); }
};

void load(MIDIPlayer& player, SS_MIDIFile* file, unsigned loopMode = 0, double fade = 0) {
    player.setLoopCount(0);
    require(player.Load(file, 0, loopMode, fade, 0, file->duration, file->duration), "Load failed");
}

void callbacks() {
    auto file = midi();
    CapturePlayer player;
    load(player, file.get());
    float block[256];
    require(player.Play(block, 128) == 128, "First block failed");
    for(int i = 0; i < 4; ++i) player.Play(block, 128);
    player.queueInitialEvents();
    player.Play(block, 128);
    require(std::any_of(player.events.begin(), player.events.end(), [](const auto& e) {
        return e.data == std::vector<uint8_t>{0xc0, 19} && e.port == 2;
    }), "Initial callback events were discarded");
    player.queueController();
    player.Play(block, 128);
    require(std::any_of(player.events.begin(), player.events.end(), [](const auto& e) {
        return e.data == std::vector<uint8_t>{0xb0, 7, 99} && e.port == 2;
    }), "MIDI port was not retained between blocks");
    while(player.Play(block, 128)) {}
    const auto note = std::find_if(player.events.begin(), player.events.end(), [](const auto& e) {
        return e.data[0] == 0x90;
    });
    require(note != player.events.end(), "Note was not dispatched");
    require(std::abs(double(note->frame) / 44100.0 - .25) < .01, "Incorrect note timing");
    std::cout << "Note dispatched at frame " << note->frame << " offset " << note->offset << '\n';
    player.events.clear();
    player.Seek(22050);
    require(std::any_of(player.events.begin(), player.events.end(), [](const auto& e) {
        return e.data == std::vector<uint8_t>{0xc0, 40};
    }), "Seek did not restore program changes");
    require(player.Play(block, 128) == 128, "Playback did not resume after seek");
    auto multi = multiMidi();
    CapturePlayer multiPlayer;
    load(multiPlayer, multi.get());
    require(multiPlayer.ports() == 3, "Raw MIDI port IDs were not mapped to emulator ports");
    while(multiPlayer.Play(block, 128)) {}
    for(unsigned port = 0; port < 2; ++port)
        require(std::any_of(multiPlayer.events.begin(), multiPlayer.events.end(), [port](const auto& e) {
            return e.data[0] == 0x90 && e.port == port;
        }), "Missing note on remapped MIDI port");
    std::cout << "Callback initialization, port mapping, timing and seek passed\n";
}

void seekTimeline() {
    auto file = midi();
    CapturePlayer player;
    load(player, file.get());
    require(player.PreparePlayback(), "Could not prepare seek test");
    const auto advance = [&](unsigned frames) {
        float block[256];
        while(frames) {
            const auto count = std::min(frames, 128u);
            require(player.Play(block, count) == count, "Seek ended playback before the requested interval");
            frames -= count;
        }
    };

    // Seeking into a gap must preserve the wait until the next event.
    player.Seek(4410); // .1 s; first note is at .25 s.
    player.events.clear();
    advance(4410);
    const auto hasNote = [&] {
        return std::any_of(player.events.begin(), player.events.end(), [](const auto& e) {
            return e.data[0] == 0x90;
        });
    };
    require(!hasNote(), "Seek moved the first note ahead of its timestamp");
    advance(4410);
    require(hasNote(), "First note missing after seek into initial silence");

    player.Seek(44100); // 1 s; the final event is at 1.5 s.
    advance(11025);
    require(player.Tell() == 55125, "Forward seek lost the PCM position");
    float block[256];
    unsigned remaining = 0;
    while(const auto got = player.Play(block, 128)) {
        remaining += got;
        require(remaining < 22050, "Playback did not finish after seek");
    }
    require(remaining >= 10897, "Seek shortened the remaining MIDI duration");
    player.Seek(44100);
    advance(11025);
    std::cout << "Seek preserves event gaps, duration and playback after EOF\n";
}

void playbackTimeline() {
    auto file = midi();
    CapturePlayer player;
    load(player, file.get(), MIDIPlayer::loop_mode_enable);
    float block[2048];
    unsigned long expected = 0;
    for(unsigned count : {128u, 1024u, 37u, 128u}) {
        require(player.Play(block, count) == count, "Short timeline render");
        expected += count;
        require(player.Tell() == expected, "PCM timestamp lags behind rendered audio");
    }
    // The PCM clock must not jump backwards when the song loops.
    while(expected < 44100 * 4) {
        expected += player.Play(block, 128);
        require(player.Tell() == expected, "PCM timestamp wrapped at a MIDI loop");
    }
    player.Seek(22050);
    require(player.Tell() == 22050, "Seek did not set the PCM clock");

    CapturePlayer initial;
    load(initial, file.get());
    initial.Seek(0);
    require(initial.Play(block, 128) == 128, "Initial seek prevented playback");
    require(initial.starts == 1, "Initial seek booted the backend twice");
    auto immediate = midi(true);
    CapturePlayer prepared;
    load(prepared, immediate.get());
    require(prepared.PreparePlayback(), "Could not prepare playback");
    prepared.Seek(0);
    require(prepared.starts == 1, "Initial seek restarted the prepared synth");
    prepared.Play(block, 1024);
    require(prepared.starts == 1, "First audio read restarted the prepared synth");
    require(std::count_if(prepared.events.begin(), prepared.events.end(), [](const auto& event) {
        return event.data == std::vector<uint8_t>{0x90, 60, 100};
    }) == 1, "Initial seek dropped or duplicated the note at time zero");
    std::cout << "Continuous PCM timestamps, looping and initial seek passed\n";
}

#ifdef MIDI_ENABLE_NUKED_SC55
double rms(const std::vector<float>& audio, unsigned rate, double from, double to) {
    // Measure AC energy per channel: mk1 can emit a non-zero DC level even
    // during silence. DC must neither fail silence checks nor count as a note.
    const size_t begin = size_t(from * rate) * 2;
    const size_t end = std::min(audio.size(), size_t(to * rate) * 2);
    if(end <= begin || (end - begin) % 2 != 0)
        throw std::runtime_error("Invalid RMS window: " + std::to_string(from) + ".." +
                                 std::to_string(to) + ", audio frames=" + std::to_string(audio.size() / 2));
    double sum[2]{}, squares[2]{};
    for(size_t i = begin; i < end; ++i) {
        require(std::isfinite(audio[i]), "Non-finite audio sample");
        const auto channel = i % 2;
        sum[channel] += audio[i];
        squares[channel] += double(audio[i]) * audio[i];
    }
    const double frames = (end - begin) / 2;
    double energy = 0;
    for(unsigned channel = 0; channel < 2; ++channel) {
        const double mean = sum[channel] / frames;
        energy += std::max(0.0, squares[channel] / frames - mean * mean);
    }
    return std::sqrt(energy / 2);
}

std::vector<float> render(MIDIPlayer& player, unsigned count) {
    std::vector<float> audio(count * 2);
    unsigned done = 0;
    while(done < count) {
        const auto got = player.Play(audio.data() + done * 2, std::min(128u, count - done));
        if(!got) break;
        done += got;
    }
    audio.resize(done * 2);
    return audio;
}

void initialBurstTest(const char* directory, const std::string& romset) {
    std::vector<uint8_t> track;
    for(unsigned channel = 0; channel < 16; ++channel) {
        if(channel == 9) continue;
        const uint8_t setup[]{0,uint8_t(0xc0 | channel),32,
            0,uint8_t(0xb0 | channel),7,100, 0,uint8_t(0xb0 | channel),10,64};
        track.insert(track.end(), std::begin(setup), std::end(setup));
    }
    const uint8_t notes[]{0,0x99,35,120, 10,0x89,35,0,
        0x83,0x60,0xb9,64,0, 0,0xff,0x2f,0};
    track.insert(track.end(), std::begin(notes), std::end(notes));
    std::vector<uint8_t> bytes{'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
                              'M','T','r','k',0,0,0,uint8_t(track.size())};
    bytes.insert(bytes.end(), track.begin(), track.end());
    auto* input = ss_file_open_from_memory(bytes.data(), bytes.size(), false);
    std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> file(ss_midi_load(input, "mid"), ss_midi_free);
    ss_file_close(input);
    require(bool(file), "Cannot parse initial burst fixture");
    NukedSC55Player::setPersistenceEnabled(false);
    NukedSC55Player::setPersistenceEnabled(true);
    for(unsigned pass = 0; pass < 2; ++pass) {
        NukedSC55Player player;
        require(player.prepare(directory, romset), "Initial burst ROM load failed");
        load(player, file.get());
        require(player.PreparePlayback() && player.Tell() == 0, "Preparation advanced the song clock");
        const auto audio = render(player, player.sampleRate() / 4);
        const double attack = rms(audio, player.sampleRate(), .003, .015);
        std::cout << "Initial burst attack RMS (" << (pass ? "reused" : "cold") << "): " << attack << '\n';
        require(attack > .002, "Initial setup delayed or swallowed the short first note");
        // Fooyin may rewind after read-ahead or restart the same decoder.
        for(unsigned rewind = 0; rewind < 2; ++rewind) {
            player.Seek(0);
            require(player.Tell() == 0, "Rewind did not reset the PCM clock");
            const auto replay = render(player, player.sampleRate() / 4);
            require(rms(replay, player.sampleRate(), .003, .015) > .002,
                    "Rewind after read-ahead lost the initial attack");
        }
    }
    NukedSC55Player::setPersistenceEnabled(false);
}

void persistenceTest(const char* directory, const std::string& romset) {
    using Clock = std::chrono::steady_clock;
    auto file = midi();
    std::vector<float> coldAudio;
    double coldSeconds;
    NukedSC55Player::setPersistenceEnabled(false);
    {
        const auto start = Clock::now();
        NukedSC55Player cold;
        require(cold.prepare(directory, romset), "Cold ROM load failed");
        load(cold, file.get());
        require(cold.PreparePlayback(), "Cold boot failed");
        coldSeconds = std::chrono::duration<double>(Clock::now() - start).count();
        coldAudio = render(cold, cold.sampleRate());
    }
    require(!NukedSC55Player::hasIdleDevice(), "Disabled pool retained an emulator");
    NukedSC55Player::setPersistenceEnabled(true);
    {
        const std::vector<uint8_t> track{
            0,0xc0,81, 0,0xb0,7,12, 0,0xb0,64,127, 0,0xe0,0,100,
            0,0xf0,7,0x7f,0x7f,4,1,0,32,0xf7, // Universal master volume.
            0,0x90,48,127, 0x87,0x40,0x80,48,0, 0,0xff,0x2f,0
        };
        std::vector<uint8_t> bytes{'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
                                   'M','T','r','k',0,0,0,uint8_t(track.size())};
        bytes.insert(bytes.end(), track.begin(), track.end());
        auto* input = ss_file_open_from_memory(bytes.data(), bytes.size(), false);
        std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> dirty(ss_midi_load(input, "mid"), ss_midi_free);
        ss_file_close(input);
        require(bool(dirty), "Could not load dirty-state fixture");
        NukedSC55Player seed;
        require(seed.prepare(directory, romset), "Persistent seed ROM load failed");
        load(seed, dirty.get());
        seed.setGainDb(-12);
        require(seed.PreparePlayback(), "Persistent seed boot failed");
        render(seed, seed.sampleRate() / 4); // Change playing state, then destroy its owner.
    }
    require(NukedSC55Player::hasIdleDevice(), "Released device was not retained");
    {
        const auto start = Clock::now();
        NukedSC55Player warm;
        require(warm.prepare(directory, romset), "Warm ROM load failed");
        load(warm, file.get());
        require(warm.PreparePlayback(), "Warm boot failed");
        const double warmSeconds = std::chrono::duration<double>(Clock::now() - start).count();
        require(!NukedSC55Player::hasIdleDevice(), "Active device remained available to another decoder");
        // Disable persistence while a decoder still owns the borrowed device.
        NukedSC55Player::setPersistenceEnabled(false);
        require(!NukedSC55Player::hasIdleDevice(), "Disabling persistence retained a device");
        const auto reusedAudio = render(warm, warm.sampleRate());
        const double coldRms = rms(coldAudio, warm.sampleRate(), .35, .65);
        const double reusedRms = rms(reusedAudio, warm.sampleRate(), .35, .65);
        std::cout << "Initial silence RMS: cold " << rms(coldAudio, warm.sampleRate(), .05, .20)
                  << ", reused " << rms(reusedAudio, warm.sampleRate(), .05, .20) << std::endl;
        require(rms(reusedAudio, warm.sampleRate(), .05, .20) < .0001,
                "Previous notes or effects leaked into the next track");
        require(reusedRms > coldRms * .8 && reusedRms < coldRms * 1.2,
                "Reset lost the initial note or retained previous volume/instrument state");
        std::cout << "Startup including ROM loading: cold " << coldSeconds
                  << " s, reused " << warmSeconds << " s; reset attack verified\n";
    }
    // The decoder above was released after invalidation: it must not refill the pool.
    require(!NukedSC55Player::hasIdleDevice(), "An invalidated decoder repopulated the pool");
    NukedSC55Player::setPersistenceEnabled(true);
}

void romTest(const char* directory, const std::string& romset) {
    initialBurstTest(directory, romset);
    persistenceTest(directory, romset);
    auto file = midi();
    NukedSC55Player player;
    require(!player.prepare("", romset), "Empty ROM directory accepted");
    std::string error;
    require(player.GetLastError(error) && !error.empty(), "Missing ROM error not reported");
    require(player.prepare(directory, romset), "Failed to load ROMs for the selected model");
    require(player.sampleRate() == (romset == "mk1" ? 64000u : 66207u), "Unexpected sample rate for selected model");
    load(player, file.get());
    auto audio = render(player, player.sampleRate());
    std::cout << "MIDI duration: " << file->duration << "; rendered frames: " << audio.size()/2 << std::endl;
    require(audio.size() == player.sampleRate() * 2, "Short audio render");
    const auto silence = rms(audio, player.sampleRate(), .05, .20);
    const auto sound = rms(audio, player.sampleRate(), .35, .65);
    std::cout << "Native rate: " << player.sampleRate() << "; silence RMS: " << silence << "; note RMS: " << sound << '\n';
    require(sound > .001 && sound > silence * 10, "Expected note or initial silence missing");
    player.Seek(0);
    auto replay = render(player, player.sampleRate());
    require(rms(replay, player.sampleRate(), .35, .65) > .001, "Silent after rewind");
    require(rms(replay, player.sampleRate(), .05, .20) < sound / 10, "Stuck note after rewind");
    player.Seek(unsigned(player.sampleRate() * 1.0));
    auto end = render(player, player.sampleRate() / 4);
    if(player.GetLastError(error)) throw std::runtime_error(error);
    require(end.size() > size_t(player.sampleRate() * .05) * 2,
            "Forward seek produced too little audio to check for stuck notes");
    require(rms(end, player.sampleRate(), .05, .20) < sound / 10, "Stuck note after forward seek");
    require(!player.GetLastError(error), "Emulator reported a playback error");

    NukedSC55Player loop;
    require(loop.prepare(directory, romset), "Loop ROM load failed");
    load(loop, file.get(), MIDIPlayer::loop_mode_enable);
    const auto repeated = render(loop, loop.sampleRate() * 4);
    require(repeated.size() == loop.sampleRate() * 8, "Infinite loop stopped prematurely");
    require(rms(repeated, loop.sampleRate(), 1.9, 2.1) > .001, "Loop did not retrigger note");
    auto immediate = midi(true);
    NukedSC55Player prepared;
    require(prepared.prepare(directory, romset), "Startup ROM load failed");
    load(prepared, immediate.get());
    require(prepared.PreparePlayback(), "Emulator could not prepare before output starts");
    prepared.Seek(0);
    auto attack = render(prepared, prepared.sampleRate() / 4);
    require(rms(attack, prepared.sampleRate(), .05, .20) > .001,
            "Preparing/seeking at zero lost the first note");
    NukedSC55Player::setPersistenceEnabled(false);
    std::cout << "ROM rendering, initial silence, rewind, forward seek and looping passed\n";
}
#endif

#ifdef MIDI_ENABLE_EXTERNAL
void externalStartup() {
    // Simultaneous bass/drum attacks with another channel's setup between them.
    const uint8_t bytes[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
        'M','T','r','k',0,0,0,34,
        0,0xc0,34, 0,0x90,28,124,
        0,0xc9,0, 0,0xb9,7,100, 0,0x99,35,112,
        0x78,0xc9,0, // Same program later: must not be suppressed.
        0x78,0x80,28,0, 0,0x89,35,0, 0x81,0x70,0xff,0x2f,0
    };
    auto* input = ss_file_open_from_memory(bytes, sizeof(bytes), false);
    std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> file(ss_midi_load(input, "mid"), ss_midi_free);
    ss_file_close(input);
    require(bool(file), "Cannot parse interleaved setup fixture");
    using Message = std::vector<uint8_t>;
    std::vector<Message> sent;
    ExternalMIDIPlayer output;
    load(output, file.get());
    output.enableOutput([&](const uint8_t* data, size_t length) { sent.emplace_back(data, data + length); });
    std::vector<float> prefix(4410 * 2);
    require(output.Play(prefix.data(), 4410) == 4410, "Preparation prefix missing");
    require(output.Tell() == 4410, "Preparation not included in playback position");
    require(std::none_of(sent.begin(), sent.end(), [](const auto& m) { return (m[0] & 0xf0) == 0x90; }),
            "Notes sent before device preparation finished");
    require(std::count(sent.begin(), sent.end(), Message{0xc9, 0}) == 1, "Drum setup not primed");
    float block[256];
    unsigned long rendered = 4410;
    while(auto count = output.Play(block, 128)) rendered += count;
    require(output.Tell() == rendered, "External timeline lost frames");
    const auto bass = std::find(sent.begin(), sent.end(), Message{0x90, 28, 124});
    require(bass != sent.end() && std::next(bass) != sent.end() && *std::next(bass) == Message{0x99, 35, 112},
            "Setup messages separated simultaneous initial notes");
    require(std::count(sent.begin(), sent.end(), Message{0xc9, 0}) == 2, "Later program change lost or setup duplicated");
    ExternalMIDIPlayer reference;
    load(reference, file.get());
    unsigned long musicalFrames = 0;
    while(auto count = reference.MIDIPlayer::Play(block, 128)) musicalFrames += count;
    require(rendered == musicalFrames + 4410, "Preparation cropped the song tail");
    output.Seek(0);
    sent.clear();
    require(output.Play(prefix.data(), 4410) == 4410, "Rewind lost preparation prefix");
    require(std::none_of(sent.begin(), sent.end(), [](const auto& m) { return (m[0] & 0xf0) == 0x90; }),
            "Rewind sent notes during preparation");
    output.Seek(22050);
    require(output.Tell() == 22050, "Seek did not map external timeline correctly");
}

void externalVolume() {
    using Message = std::vector<uint8_t>;
    std::vector<Message> sent;
    const Message reset{0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0, 0x7f, 0, 0x41, 0xf7};
    const Message level{0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0, 4, 64, 0x7c, 0xf7};
    const Message gmReset{0xf0, 0x7e, 0x7f, 9, 1, 0xf7};
    const Message gmLevel{0xf0, 0x7f, 0x7f, 4, 1, 0, 64, 0xf7};
    std::vector<uint8_t> track;
    const auto sysex = [&](uint8_t delta, const Message& message) {
        track.insert(track.end(), {delta, 0xf0, uint8_t(message.size() - 1)});
        track.insert(track.end(), message.begin() + 1, message.end());
    };
    sysex(0, reset);
    sysex(0, level);
    sysex(120, gmReset);
    sysex(0, gmLevel);
    track.insert(track.end(), {0, 0x90, 60, 100, 120, 0x80, 60, 0,
                              0x81, 0x70, 0xff, 0x2f, 0});
    std::vector<uint8_t> bytes{'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
                               'M','T','r','k',0,0,0,uint8_t(track.size())};
    bytes.insert(bytes.end(), track.begin(), track.end());
    auto* input = ss_file_open_from_memory(bytes.data(), bytes.size(), false);
    std::unique_ptr<SS_MIDIFile, decltype(&ss_midi_free)> file(ss_midi_load(input, "mid"), ss_midi_free);
    ss_file_close(input);
    require(bool(file), "Cannot parse master volume fixture");
    ExternalMIDIPlayer output;
    load(output, file.get());
    output.setOutputVolume(.1);
    require(output.enableOutput([&](const uint8_t* data, size_t length) {
        sent.emplace_back(data, data + length);
    }), "Cannot enable volume test output");
    require(sent.empty(), "Volume sent before external playback starts");
    const auto lastVolume = [&]() {
        require(!sent.empty(), "Missing master volume");
        const auto& m = sent.back();
        require(m.size() == 11 && m[0] == 0xf0 && m[1] == 0x41 && m[7] == 4,
                "Expected GS master volume message");
        require(((m[5] + m[6] + m[7] + m[8] + m[9]) & 127) == 0, "Invalid volume checksum");
        return m[8];
    };
    require(output.PreparePlayback(), "Cannot prepare volume test");
    require(lastVolume() == 64, "Initial fooyin volume not applied after reset");
    float block[256];
    for(unsigned n = 0; n < 50; ++n) require(output.Play(block, 128) == 128, "Short volume render");
    require(lastVolume() == 32, "GS song master volume did not combine with fooyin volume");
    output.setOutputVolume(0);
    require(lastVolume() == 0, "Mute did not silence master volume");
    for(unsigned n = 0; n < 50; ++n) output.Play(block, 128);
    // The file's GM reset and master-volume message must not defeat mute.
    for(size_t i = 0; i < sent.size(); ++i) {
        if(sent[i] == gmReset) {
            require(i + 1 < sent.size() && sent[i + 1][8] == 0, "GM reset defeated mute");
        }
    }
    output.setOutputVolume(.1);
    require(lastVolume() == 32, "GM song master volume did not combine with fooyin volume");
    output.Seek(15000);
    require(lastVolume() == 32, "Seek failed to restore combined master volume");
    output.setOutputVolume(0);
    output.Seek(0);
    require(lastVolume() == 0, "Rewind defeated mute");
    output.setOutputVolume(.01);
    require(lastVolume() == 1, "Low positive volume incorrectly became mute");
    output.setOutputVolume(.1); // Midpoint of fooyin's logarithmic slider.
    require(lastVolume() == 64, "Slider midpoint must use the middle of the GS range");
    output.setOutputVolume(1);
    require(lastVolume() == 127, "Full volume did not reach GS maximum");
    output.setOutputVolume(2);
    require(lastVolume() == 127, "Output volume was not clamped");
    std::cout << "External master volume, resets, mute and seek passed\n";
}

void externalTests() {
    auto file = midi(true);
    using Message = std::vector<uint8_t>;
    std::vector<Message> sent;
    float block[256];
    {
        ExternalMIDIPlayer output;
        load(output, file.get());
        require(output.enableOutput([&](const uint8_t* bytes, size_t size) {
            sent.emplace_back(bytes, bytes + size);
        }), "Cannot enable external MIDI sender");
        output.Seek(0);
        require(sent.empty(), "Initial external seek prematurely sent resets or panic messages");
        while(output.Play(block, 128)) {
            require(std::all_of(std::begin(block), std::end(block), [](float v) { return v == 0; }),
                    "External mode emitted PCM audio");
        }
        require(std::count(sent.begin(), sent.end(), Message{0x90, 60, 100}) == 1,
                "External note was missing or duplicated");
        require(std::any_of(sent.begin(), sent.end(), [](const auto& msg) {
            return msg.front() == 0xf0 && msg.back() == 0xf7;
        }), "External SysEx was not sent");
        sent.clear();
        output.silence();
        require(sent.size() == 48, "Pause did not silence all 16 channels");
        require(sent.back() == Message{0xbf, 120, 0}, "All Sound Off missing");
        output.Seek(22050);
        require(std::find(sent.begin(), sent.end(), Message{0xc0, 40}) != sent.end(),
                "External seek did not restore instrument");
        sent.clear();
    }
    require(sent.size() == 48, "Destroying the external session left notes playing");

    ExternalMIDIPlayer prebuffer;
    load(prebuffer, file.get());
    require(prebuffer.Play(block, 128) == 128, "Silent prebuffer decoder failed");
    require(std::all_of(std::begin(block), std::end(block), [](float v) { return v == 0; }),
            "Prebuffer output is not silent");

    auto multi = multiMidi();
    ExternalMIDIPlayer multiOutput;
    load(multiOutput, multi.get());
    require(!multiOutput.enableOutput([](const uint8_t*, size_t) {}),
            "Multi-port tracks were silently collapsed into one external device");

    ExternalMIDIPlayer broken;
    load(broken, file.get());
    broken.enableOutput([](const uint8_t*, size_t) { throw std::runtime_error("Disconnected MIDI port"); });
    require(broken.Play(block, 128) == 0, "MIDI transport error did not stop playback");
    std::string error;
    require(broken.GetLastError(error) && error == "Disconnected MIDI port", "MIDI error was lost");
    std::cout << "External MIDI notes, SysEx, silence, seek, cleanup and transport errors passed\n";
}
#endif

int main(int argc, char** argv) {
    try {
        callbacks();
        seekTimeline();
        playbackTimeline();
#ifdef MIDI_ENABLE_EXTERNAL
        externalStartup();
        externalTests();
        externalVolume();
#endif
#ifdef MIDI_ENABLE_NUKED_SC55
        if(argc > 1) {
            require(argc <= 3, "Usage: midi_player_test [ROM_DIRECTORY [mk1|mk2]]");
            const std::string romset = argc > 2 ? argv[2] : "mk2";
            require(romset == "mk1" || romset == "mk2", "ROM model must be mk1 or mk2");
            std::cout << "Testing Nuked-SC55 ROM model: " << romset << '\n';
            romTest(argv[1], romset);
        }
#endif
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
