// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <core/plugins/coreplugincontext.h>
#include <core/engine/audioloader.h>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace Fooyin::MIDIInput {
class MIDIDecoder;
class ExternalMIDIController final : public QObject {
public:
    ExternalMIDIController(const CorePluginContext&, QObject* parent);
    ~ExternalMIDIController() override;
private:
    void loadTrack(const Track& track);
    void clear();
    void tick();
    void seek(uint64_t ms);
    void anchor(uint64_t ms);
    PlayerController* player;
    std::shared_ptr<AudioLoader> loader;
    LoadedDecoder loaded;
    MIDIDecoder* decoder = nullptr;
    QTimer timer;
    QElapsedTimer clock;
    uint64_t anchorMs = 0;
    uint64_t renderedFrames = 0;
    bool suspended = true;
    double outputVolume = 1.0;
};
}
