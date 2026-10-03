// SPDX-License-Identifier: GPL-3.0-or-later
#include "midiinput.h"
#include "midiinputdefs.h"
#include "ExternalMIDIPlayer.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace Fooyin;
using namespace Fooyin::MIDIInput;

static void require(bool condition, const char* message)
{
    if(!condition) throw std::runtime_error(message);
}

static void checkDecoder(bool repeat, bool external = true)
{
    // 1.5 seconds, with a final controller event at the musical end.
    const unsigned char midi[] = {
        'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
        'M','T','r','k',0,0,0,22,
        0,0xc0,40,0x81,0x70,0x90,60,100,
        0x83,0x60,0x80,60,0,0x85,0x50,0xb0,64,0,0,0xff,0x2f,0
    };
    QBuffer source;
    source.setData(reinterpret_cast<const char*>(midi), sizeof(midi));
    require(source.open(QIODevice::ReadOnly), "Cannot open MIDI fixture");
    Track track;
    track.setFilePath("/tmp/decoder-tail-fixture.mid");
    track.setDuration(1500);
    track.setSubsong(0);
    MIDIDecoder decoder;
    if(repeat) decoder.setPlaybackHints(AudioDecoder::RepeatTrackEnabled);
    const auto format = decoder.init({track.filepath(), &source}, track, AudioDecoder::None);
    require(format.has_value(), "Decoder init failed");
    if(!external) {
        // A pending update can replace the outgoing track during gapless
        // staging and make fooyin reopen the already audible next track.
        require(!decoder.trackHasChanged(), "Internal decoder published a transition-time duration change");
        require(!decoder.changedTrack().isValid(), "Internal decoder retained replacement metadata");
        return;
    }
    const auto duration = 1500 + (repeat ? 0 : FySettings{}.value(ReleaseTailSetting, DefaultReleaseTail).toInt())
        + ExternalMIDIPlayer::StartupMilliseconds;
    require(decoder.trackHasChanged() && decoder.changedTrack().duration() == duration,
            "Decoder did not announce the tail/startup duration correctly");
    require(!decoder.trackHasChanged(), "Consumed duration update was announced again");
    require(decoder.changedTrack().duration() == duration, "Acknowledging update discarded duration");
    decoder.start();
    uint64_t frames = 0;
    const auto expected = format->framesForDuration(duration);
    while(true) {
        // Deliberately not aligned to the 128-frame synthesis block.
        const auto buffer = decoder.readBuffer(format->bytesForFrames(997));
        if(!buffer.isValid()) break;
        frames += buffer.frameCount();
        require(!decoder.trackHasChanged(), "Reading audio republished stale metadata");
        if(repeat && frames > static_cast<uint64_t>(format->sampleRate()) * 8) return;
        require(repeat || frames <= static_cast<uint64_t>(expected), "Decoder padded beyond finite EOF");
    }
    require(!repeat, "Repeat playback stopped at MIDI EOF");
    require(frames == static_cast<uint64_t>(expected), "Decoder cropped the release tail");
    decoder.seek(duration - 500);
    require(decoder.readBuffer(1024).isValid(), "Seeking into the release tail returned premature EOF");
}

int main(int argc, char** argv)
{
    // No real MIDI port is opened; ExternalMIDIPlayer emits silent PCM without a sender.
    QTemporaryDir config;
    if(!config.isValid()) return EXIT_FAILURE;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QCoreApplication app(argc, argv);
    try {
        FySettings settings;
        require(settings.fileName().startsWith(config.path() + '/'), "Settings are not isolated");
        settings.setValue(EngineSetting, ExternalEngine);
        settings.setValue(LoopCountSetting, 0);
        settings.setValue(FadeLengthSetting, 0);
        settings.sync();
        checkDecoder(false);
        checkDecoder(true);
        settings.setValue(ReleaseTailSetting, 0);
        checkDecoder(false);
        settings.setValue(ReleaseTailSetting, 1250);
        settings.setValue(FadeLengthSetting, 4000);
        checkDecoder(false);
        checkDecoder(true);
        settings.setValue(EngineSetting, DefaultEngine);
        settings.sync();
        checkDecoder(false, false);
        checkDecoder(true, false);
        std::cout << "Decoder preserves finite release tails and uninterrupted repeat playback\n";
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
