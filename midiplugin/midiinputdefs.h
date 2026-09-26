/*
 * MIDI Plugin
 * Copyright © 2025, Christopher Snowhill <kode54@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

namespace Fooyin::MIDIInput {
constexpr auto EngineSetting = "MIDIInput/Engine";
constexpr auto DefaultEngine = 0; // SpessaSynth
constexpr auto NukedEngine = 1;
constexpr auto ExternalEngine = 2;
constexpr auto ExternalPortSetting = "MIDIInput/ExternalPort";
constexpr auto NukedRomPathSetting = "MIDIInput/NukedRomPath";
constexpr auto NukedRomSetSetting = "MIDIInput/NukedRomSet";
constexpr auto DefaultNukedRomSet = "mk2";

constexpr auto SoundfontPathSetting   = "MIDIInput/SoundfontPath";
constexpr auto SoundfontGSPathSetting = "MIDIInput/SoundfontGSPath";

constexpr auto DefaultInterpolation   = 1;
constexpr auto InterpolationSetting   = "MIDIInput/Interpolation";

constexpr auto DefaultVoiceCount      = 512;
constexpr auto VoiceCountSetting      = "MIDIInput/VoiceCount";

constexpr auto DefaultGain            = 0.0;
constexpr auto GainSetting            = "MIDIInput/Gain";

constexpr auto DefaultReverbLevel     = 75.0;
constexpr auto ReverbLevelSetting     = "MIDIInput/ReverbLevel";
constexpr auto DefaultChorusLevel     = 50.0;
constexpr auto ChorusLevelSetting     = "MIDIInput/ChorusLevel";
constexpr auto DefaultEffectsEnabled  = true;
constexpr auto EffectsEnabledSetting  = "MIDIInput/EffectsEnabled";

constexpr auto DefaultLoopCount       = 2;
constexpr auto LoopCountSetting       = "MIDIInput/LoopCount";
constexpr auto DefaultFadeLength      = 4000;
constexpr auto FadeLengthSetting      = "MIDIInput/FadeLength";

} // namespace Fooyin::MIDIInput
