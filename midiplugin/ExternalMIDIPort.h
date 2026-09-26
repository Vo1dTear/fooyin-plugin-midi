// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ExternalMIDIPlayer.h"
#include <string>
#include <vector>

// Called on the GUI/playback-controller thread only. The virtual port stays
// open between tracks so Nuked-SC55 can subscribe once and remain connected.
namespace ExternalMIDI {
constexpr auto VirtualPort = "@virtual";
std::vector<std::string> ports();
ExternalMIDIPlayer::Sender open(const std::string& name);
void close();
}
