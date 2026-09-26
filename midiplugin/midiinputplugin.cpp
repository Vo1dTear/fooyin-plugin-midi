/*
 * MIDI Plugin
 * Copyright 2025, Christopher Snowhill <kode54@gmail.com>
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

#include "midiinputplugin.h"

#include "midiinput.h"
#include "midiinputdefs.h"
#ifdef MIDI_ENABLE_NUKED_SC55
#include "NukedSC55Player.h"
#endif
#include "midiinputsettings.h"
#ifdef MIDI_ENABLE_EXTERNAL
#include "ExternalMIDIController.h"
#endif

using namespace Qt::StringLiterals;

namespace Fooyin::MIDIInput {
namespace {
class MIDIInputPluginSettingsProvider : public Fooyin::PluginSettingsProvider
{
public:
    QDialog* createSettings(QWidget* parent) override
    {
        return new MIDIInputSettings(parent);
    }
};
} // namespace

void MIDIInputPlugin::initialise(const Fooyin::CorePluginContext& context) {
#ifdef MIDI_ENABLE_NUKED_SC55
    const FySettings settings;
    NukedSC55Player::setPersistenceEnabled(settings.value(EngineSetting, DefaultEngine).toInt() == NukedEngine);
#endif
#ifdef MIDI_ENABLE_EXTERNAL
    delete externalController;
    externalController = new ExternalMIDIController(context, this);
#else
    Q_UNUSED(context)
#endif
}

void MIDIInputPlugin::shutdown() {
#ifdef MIDI_ENABLE_NUKED_SC55
    NukedSC55Player::setPersistenceEnabled(false);
#endif
    delete externalController;
    externalController = nullptr;
}

QString MIDIInputPlugin::inputName() const
{
    return u"MIDI Input"_s;
}

Fooyin::InputCreator MIDIInputPlugin::inputCreator() const
{
    Fooyin::InputCreator creator;
    creator.decoder = []() {
        return std::make_unique<MIDIDecoder>();
    };
    creator.reader = []() {
        return std::make_unique<MIDIReader>();
    };
    return creator;
}

std::unique_ptr<Fooyin::PluginSettingsProvider> MIDIInputPlugin::settingsProvider() const
{
    return std::make_unique<MIDIInputPluginSettingsProvider>();
}
}

#include "moc_midiinputplugin.cpp"
