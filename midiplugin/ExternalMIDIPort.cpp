// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExternalMIDIPort.h"
#include <RtMidi.h>
#include <memory>
#include <regex>
#include <stdexcept>

namespace ExternalMIDI {
namespace {
#ifdef __linux__
constexpr auto Api = RtMidi::LINUX_ALSA;
#else
constexpr auto Api = RtMidi::UNSPECIFIED;
#endif
std::shared_ptr<RtMidiOut> output;
std::string selected;
std::string stableName(const std::string& name) {
#ifdef __linux__
    // ALSA client IDs change when applications restart.
    return std::regex_replace(name, std::regex(" [0-9]+:[0-9]+$"), "");
#else
    return name;
#endif
}
}

std::vector<std::string> ports() {
    RtMidiOut probe(Api, "fooyin MIDI ports");
    std::vector<std::string> result;
    for(unsigned i = 0; i < probe.getPortCount(); ++i)
        result.push_back(stableName(probe.getPortName(i)));
    return result;
}

ExternalMIDIPlayer::Sender open(const std::string& name) {
    if(!output || name != selected) {
        if(output && output.use_count() > 1)
            throw std::runtime_error("Stop external MIDI playback before changing its output port");
        auto next = std::make_shared<RtMidiOut>(Api, "fooyin MIDI");
        if(name == VirtualPort) {
            next->openVirtualPort("Output");
        } else {
            int match = -1;
            for(unsigned i = 0; i < next->getPortCount(); ++i) {
                if(stableName(next->getPortName(i)) != name) continue;
                if(match >= 0) throw std::runtime_error("Ambiguous MIDI port name; rename one of the destination ports");
                match = int(i);
            }
            if(match < 0) throw std::runtime_error("Selected MIDI output is unavailable; open the target application and refresh the ports");
            next->openPort(unsigned(match), "Output");
        }
        output = std::move(next);
        selected = name;
    }
    return [port = output](const uint8_t* data, size_t length) { port->sendMessage(data, length); };
}

void close() { output.reset(); selected.clear(); }
}
