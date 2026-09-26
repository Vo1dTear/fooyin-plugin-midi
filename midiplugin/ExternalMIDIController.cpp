// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExternalMIDIController.h"
#include "ExternalMIDIPort.h"
#include "midiinput.h"
#include "midiinputdefs.h"
#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>
#include <QDebug>
#include <algorithm>

namespace Fooyin::MIDIInput {
ExternalMIDIController::ExternalMIDIController(const CorePluginContext& context, QObject* parent)
    : QObject(parent), player(context.playerController), loader(context.audioLoader) {
    outputVolume = context.settingsManager->value<Settings::Core::OutputVolume>();
    context.settingsManager->subscribe<Settings::Core::OutputVolume>(this, [this](double volume) {
        outputVolume = volume;
        if(decoder) decoder->setExternalVolume(volume);
    });
    timer.setInterval(5);
    timer.setTimerType(Qt::PreciseTimer);
    connect(&timer, &QTimer::timeout, this, [this] { tick(); });
    connect(player, &PlayerController::currentTrackChanged, this, [this](const Track& track) { loadTrack(track); });
    connect(player, &PlayerController::positionMoved, this, [this](uint64_t ms) { seek(ms); });
    connect(player, &PlayerController::positionChanged, this, [this](uint64_t ms) {
        if(!decoder || !timer.isActive()) return;
        // Position feedback is delayed/rounded. Treat it as a clock correction,
        // never as a user seek: seeking here silences notes just after startup.
        // Slew by at most 1 ms per update rather than stopping/replaying a burst.
        const auto predicted = int64_t(anchorMs) + clock.elapsed();
        const auto correction = std::clamp<int64_t>(int64_t(ms) - predicted, -1, 1);
        anchor(uint64_t(std::max<int64_t>(0, predicted + correction)));
    });
    connect(player, &PlayerController::playStateChanged, this, [this](Player::PlayState state) {
        if(state == Player::PlayState::Stopped) { clear(); return; }
        if(state == Player::PlayState::Paused) {
            timer.stop();
            if(decoder) decoder->silenceExternalOutput();
            suspended = true;
            return;
        }
        // Track selection owns session creation. Starting one here as well can
        // play an attack and then reset it when currentTrackChanged arrives.
        if(decoder && suspended) {
            if(renderedFrames) seek(player->currentPosition());
            else anchor(0);
            suspended = false;
            timer.start();
        }
    });
    // Make a configured virtual port available before the user starts Nuked-SC55.
    QTimer::singleShot(0, this, [this] {
        const FySettings settings;
        if(settings.value(EngineSetting, DefaultEngine).toInt() != ExternalEngine) return;
        try { ExternalMIDI::open(settings.value(ExternalPortSetting, ExternalMIDI::VirtualPort).toString().toStdString()); }
        catch(const std::exception& e) { qWarning() << "External MIDI:" << e.what(); }
        if(player->playState() != Player::PlayState::Stopped && !decoder) loadTrack(player->currentTrack());
    });
}

ExternalMIDIController::~ExternalMIDIController() { clear(); ExternalMIDI::close(); }

void ExternalMIDIController::clear() {
    timer.stop();
    if(loaded.decoder) loaded.decoder->stop();
    loaded = {};
    decoder = nullptr;
    suspended = true;
}

void ExternalMIDIController::anchor(uint64_t ms) { anchorMs = ms; clock.restart(); }

void ExternalMIDIController::loadTrack(const Track& track) {
    clear();
    const FySettings settings;
    if(settings.value(EngineSetting, DefaultEngine).toInt() != ExternalEngine) return;
    if(!track.isValid() || !MIDIReader{}.extensions().contains(track.extension(), Qt::CaseInsensitive)) return;
    const auto hints = player->playMode().testFlag(Playlist::RepeatTrack)
        ? AudioDecoder::RepeatTrackEnabled : AudioDecoder::NoHints;
    loaded = loader->loadDecoderForTrack(track, AudioDecoder::None, hints);
    decoder = dynamic_cast<MIDIDecoder*>(loaded.decoder.get());
    if(!decoder || !loaded.format) { clear(); return; }
    if(!decoder->enableExternalOutput()) {
        clear();
        player->pause();
        return;
    }
    decoder->setExternalVolume(outputVolume);
    decoder->start();
    const uint64_t position = 0;
    renderedFrames = position * loaded.format->sampleRate() / 1000;
    anchor(position);
    suspended = player->playState() != Player::PlayState::Playing;
    if(!suspended) timer.start();
}

void ExternalMIDIController::seek(uint64_t ms) {
    if(!decoder) return;
    decoder->silenceExternalOutput();
    decoder->seek(ms);
    renderedFrames = ms * loaded.format->sampleRate() / 1000;
    anchor(ms);
    if(player->playState() != Player::PlayState::Playing) decoder->silenceExternalOutput();
}

void ExternalMIDIController::tick() {
    if(!decoder || player->playState() != Player::PlayState::Playing) return;
    const auto rate = loaded.format->sampleRate();
    const uint64_t target = (anchorMs + uint64_t(clock.elapsed())) * rate / 1000;
    // After a stalled event loop, reconstruct state instead of bursting old notes.
    if(target > renderedFrames + rate / 4) { seek(target * 1000 / rate); return; }
    while(renderedFrames < target) {
        const auto count = std::min<uint64_t>(128, target - renderedFrames);
        auto buffer = decoder->readBuffer(loaded.format->bytesForFrames(int(count)));
        if(!buffer.isValid()) {
            const bool failed = decoder->externalOutputFailed();
            clear();
            if(failed) player->pause();
            return;
        }
        renderedFrames += count;
    }
}
}
