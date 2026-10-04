// SPDX-License-Identifier: GPL-3.0-or-later
#include <RtMidi.h>
#include <algorithm>
#include <iostream>
#include <vector>

int main()
{
    // No hardware or MIDI server is needed to query the compiled backends.
    std::vector<RtMidi::Api> apis;
    RtMidi::getCompiledApi(apis);
#if defined(__linux__)
    const auto expected = RtMidi::LINUX_ALSA;
#elif defined(_WIN32)
    const auto expected = RtMidi::WINDOWS_MM;
#elif defined(__APPLE__)
    const auto expected = RtMidi::MACOSX_CORE;
#else
    return apis.empty() ? 1 : 0;
#endif
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
    if(std::find(apis.begin(), apis.end(), expected) == apis.end()) {
        std::cerr << "Required native MIDI backend is missing\n";
        return 1;
    }
#endif
    return 0;
}
