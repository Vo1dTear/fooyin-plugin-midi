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

#include "midiinputsettings.h"

#include "midiinputdefs.h"
#ifdef MIDI_ENABLE_NUKED_SC55
#include "NukedSC55Player.h"
#endif
#ifdef MIDI_ENABLE_EXTERNAL
#include "ExternalMIDIPort.h"
#endif

#include <fooyin/gui/widgets/doubleslidereditor.h>

#include <algorithm>
#include <QMessageBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>

using namespace Qt::StringLiterals;

namespace Fooyin::MIDIInput {
MIDIInputSettings::MIDIInputSettings(QWidget* parent)
    : QDialog{parent}
    , m_engine{new QComboBox(this)}
    , m_externalPort{new QComboBox(this)}
    , m_romLocation{new QLineEdit(this)}
    , m_romSet{new QComboBox(this)}
    , m_loopCount{new QSpinBox(this)}
    , m_fadeLength{new QSpinBox(this)}
    , m_voiceCount{new QSpinBox(this)}
    , m_interpolationFilter{new QComboBox(this)}
    , m_gain{new DoubleSliderEditor(tr("Gain"), this)}
    , m_effectsEnabled{new QCheckBox(tr("Enable reverb and chorus"), this)}
    , m_reverbLevel{new DoubleSliderEditor(tr("Reverb level"), this)}
    , m_chorusLevel{new DoubleSliderEditor(tr("Chorus level"), this)}
    , m_soundfontLocation{new QLineEdit(this)}
    , m_soundfontGSLocation{new QLineEdit(this)}
{
    setWindowTitle(tr("%1 Settings").arg(u"MIDI Input"_s));
    setModal(true);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset,
        this
    );
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &MIDIInputSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &MIDIInputSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this,
                     &MIDIInputSettings::reset);

    auto* lengthGroup  = new QGroupBox(tr("Length"), this);
    auto* lengthLayout = new QGridLayout(lengthGroup);

    auto* loopLabel     = new QLabel(tr("Loop count") + u":"_s, this);

    m_loopCount->setRange(0, 16);
    m_loopCount->setSingleStep(1);
    m_loopCount->setSuffix(u" "_s + tr("times"));
 
    auto* fadeLabel = new QLabel(tr("Fade length") + u":"_s, this);

    m_fadeLength->setRange(0, 10000);
    m_fadeLength->setSingleStep(500);
    m_fadeLength->setSuffix(u" "_s + tr("ms"));

    int row{0};
    lengthLayout->addWidget(loopLabel, row, 0);
    lengthLayout->addWidget(m_loopCount, row++, 1);
    lengthLayout->addWidget(fadeLabel, row, 0);
    lengthLayout->addWidget(m_fadeLength, row++, 1);
    lengthLayout->setColumnStretch(2, 1);
    lengthLayout->setRowStretch(row++, 1);

    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    auto* soundfontPathLabel   = new QLabel(tr("Soundfont bank") + u":"_s, this);
    auto* soundfontGSPathLabel = new QLabel(tr("GS mode bank") + u":"_s, this);
    auto* soundfontHintLabel   = new QLabel(u"🛈 "_s
        + tr("MIDI files require a SoundFont bank or banks to play. A separate bank may be chosen as an alternate default for GS MIDI files."),
        this);
    soundfontHintLabel->setWordWrap(true);

    auto* browseButton = new QPushButton(tr("&Browse…"), this);
    QObject::connect(browseButton, &QPushButton::pressed, this, &MIDIInputSettings::getSoundfontPath);

    m_soundfontLocation->setMinimumWidth(200);

    auto* browseGSButton = new QPushButton(tr("&Browse…"), this);
    QObject::connect(browseGSButton, &QPushButton::pressed, this, &MIDIInputSettings::getSoundfontGSPath);

    m_soundfontGSLocation->setMinimumWidth(200);

    m_engine->addItem(u"SpessaSynth"_s, DefaultEngine);
#ifdef MIDI_ENABLE_NUKED_SC55
    m_engine->addItem(u"Nuked-SC55"_s, NukedEngine);
#endif
#ifdef MIDI_ENABLE_EXTERNAL
    m_engine->addItem(tr("External MIDI / Nuked-SC55 application"), ExternalEngine);
#endif
    auto* externalGroup = new QGroupBox(tr("External MIDI application"), this);
    auto* externalLayout = new QGridLayout(externalGroup);
    auto* refreshPorts = new QPushButton(tr("Refresh ports"), this);
    auto* openPort = new QPushButton(tr("Open MIDI port"), this);
    auto* portStatus = new QLabel(this);
    portStatus->setWordWrap(true);
    auto* externalHint = new QLabel(tr("Open the virtual output here, then select fooyin MIDI / Output as the MIDI input in Nuked-SC55. Fooyin controls master volume on GS-compatible devices such as Nuked-SC55. The external application controls audio output. Fooyin effects, gain, fades and audio conversion do not apply. One 16-channel MIDI port is supported."), this);
    externalHint->setWordWrap(true);
    externalLayout->addWidget(m_externalPort, 0, 0, 1, 2);
    externalLayout->addWidget(refreshPorts, 1, 0);
    externalLayout->addWidget(openPort, 1, 1);
    externalLayout->addWidget(portStatus, 2, 0, 1, 2);
    externalLayout->addWidget(externalHint, 3, 0, 1, 2);
#ifdef MIDI_ENABLE_EXTERNAL
    const auto refresh = [this, portStatus] {
        const auto selected = m_externalPort->currentIndex() < 0
            ? m_settings.value(ExternalPortSetting, ExternalMIDI::VirtualPort).toString()
            : m_externalPort->currentData().toString();
        m_externalPort->clear();
        m_externalPort->addItem(tr("Virtual output: fooyin MIDI / Output"), QString::fromLatin1(ExternalMIDI::VirtualPort));
        portStatus->clear();
        try {
            for(const auto& name : ExternalMIDI::ports())
                m_externalPort->addItem(QString::fromStdString(name), QString::fromStdString(name));
        } catch(const std::exception& e) { portStatus->setText(QString::fromUtf8(e.what())); }
        int index = m_externalPort->findData(selected);
        if(index < 0) {
            m_externalPort->addItem(tr("Unavailable: %1").arg(selected), selected);
            index = m_externalPort->count() - 1;
        }
        m_externalPort->setCurrentIndex(index);
    };
    connect(refreshPorts, &QPushButton::clicked, this, refresh);
    connect(openPort, &QPushButton::clicked, this, [this, portStatus] {
        try {
            ExternalMIDI::open(m_externalPort->currentData().toString().toStdString());
            portStatus->setText(tr("MIDI port is open. Connect the external application before starting playback."));
        } catch(const std::exception& e) { portStatus->setText(QString::fromUtf8(e.what())); }
    });
    // Enumerate only when this mode is selected, avoiding device access for internal playback.
    connect(m_engine, &QComboBox::currentIndexChanged, this, [this, refresh] {
        if(m_engine->currentData().toInt() == ExternalEngine && m_externalPort->count() == 0) refresh();
    });
#endif
    m_romSet->addItem(u"SC-55 (mk1)"_s, u"mk1"_s);
    m_romSet->addItem(u"SC-55mkII (mk2)"_s, u"mk2"_s);
    auto* romBrowse = new QPushButton(tr("Browse…"), this);
    connect(romBrowse, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, tr("Select SC-55 ROM directory"), m_romLocation->text());
        if(!path.isEmpty()) m_romLocation->setText(path);
    });
    auto* romHint = new QLabel(tr("Nuked-SC55 requires a complete ROM set for the selected model. Choose a folder containing one version. SoundFont banks and SpessaSynth effect settings do not apply."), this);
    romHint->setWordWrap(true);
    row = 0;
    generalLayout->addWidget(new QLabel(tr("Sound engine"), this), row, 0);
    generalLayout->addWidget(m_engine, row++, 1, 1, 2);
    auto* nukedGroup = new QGroupBox(tr("Nuked-SC55"), this);
    auto* nukedLayout = new QGridLayout(nukedGroup);
    nukedLayout->addWidget(new QLabel(tr("SC-55 model"), nukedGroup), 0, 0);
    nukedLayout->addWidget(m_romSet, 0, 1, 1, 2);
    nukedLayout->addWidget(new QLabel(tr("ROM directory"), nukedGroup), 1, 0);
    nukedLayout->addWidget(m_romLocation, 1, 1);
    nukedLayout->addWidget(romBrowse, 1, 2);
    nukedLayout->addWidget(romHint, 2, 0, 1, 3);
    nukedLayout->setColumnStretch(1, 1);
    generalLayout->addWidget(soundfontPathLabel, row, 0);
    generalLayout->addWidget(m_soundfontLocation, row, 1);
    generalLayout->addWidget(browseButton, row++, 2);
    generalLayout->addWidget(soundfontGSPathLabel, row, 0);
    generalLayout->addWidget(m_soundfontGSLocation, row, 1);
    generalLayout->addWidget(browseGSButton, row++, 2);
    generalLayout->addWidget(soundfontHintLabel, row++, 0, 1, 3);
    generalLayout->setColumnStretch(1, 1);
    generalLayout->setRowStretch(row++, 1);

    auto* synthesisGroup  = new QGroupBox(tr("Synthesis"), this);
    auto* synthesisLayout = new QGridLayout(synthesisGroup);

    auto* interpolationLabel = new QLabel(tr("Interpolation") + u":"_s, this);

    m_interpolationFilter->addItem(tr("None"), 0);
    m_interpolationFilter->addItem(tr("Linear"), 1);
    m_interpolationFilter->addItem(tr("Hermite"), 2);
    m_interpolationFilter->addItem(tr("Sinc"), 3);

    auto* voicesLabel = new QLabel(tr("Polyphony") + u":"_s, this);

    m_voiceCount->setRange(1, 2048);
    m_voiceCount->setSingleStep(10);
    m_voiceCount->setSuffix(u" "_s + tr("voices"));

    m_gain->setRange(-12.0, 12.0);
    m_gain->setSingleStep(0.1);
    m_gain->setSuffix(u" dB"_s);

    m_reverbLevel->setRange(0.0, 500.0);
    m_reverbLevel->setSingleStep(5.0);
    m_reverbLevel->setSuffix(u" %"_s);
    m_chorusLevel->setRange(0.0, 500.0);
    m_chorusLevel->setSingleStep(5.0);
    m_chorusLevel->setSuffix(u" %"_s);

    row = 0;
    synthesisLayout->addWidget(m_gain, row++, 0, 1, 5);
    synthesisLayout->addWidget(m_effectsEnabled, row++, 0, 1, 5);
    synthesisLayout->addWidget(m_reverbLevel, row++, 0, 1, 5);
    synthesisLayout->addWidget(m_chorusLevel, row++, 0, 1, 5);
    synthesisLayout->addWidget(interpolationLabel, row, 0);
    synthesisLayout->addWidget(m_interpolationFilter, row++, 1, 1, 4);
    synthesisLayout->addWidget(voicesLabel, row, 0);
    synthesisLayout->addWidget(m_voiceCount, row++, 1, 1, 4);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    row = 0;
    layout->addWidget(lengthGroup, row++, 0, 1, 4);
    layout->addWidget(generalGroup, row++, 0, 1, 4);
    layout->addWidget(nukedGroup, row++, 0, 1, 4);
    layout->addWidget(synthesisGroup, row++, 0, 1, 4);
    layout->addWidget(externalGroup, row++, 0, 1, 4);
    layout->addWidget(buttons, row++, 0, 1, 4, Qt::AlignBottom);
    layout->setColumnStretch(2, 1);

    m_loopCount->setValue(m_settings.value(LoopCountSetting, DefaultLoopCount).toInt());
    m_fadeLength->setValue(m_settings.value(FadeLengthSetting, DefaultFadeLength).toInt());
    m_interpolationFilter->setCurrentIndex(
        m_interpolationFilter->findData(m_settings.value(InterpolationSetting, DefaultInterpolation).toInt()));
    m_voiceCount->setValue(m_settings.value(VoiceCountSetting, DefaultVoiceCount).toInt());
    m_gain->setValue(m_settings.value(GainSetting, DefaultGain).toDouble());
    m_effectsEnabled->setChecked(m_settings.value(EffectsEnabledSetting, DefaultEffectsEnabled).toBool());
    m_reverbLevel->setEnabled(m_effectsEnabled->isChecked());
    m_chorusLevel->setEnabled(m_effectsEnabled->isChecked());
    m_reverbLevel->setValue(m_settings.value(ReverbLevelSetting, DefaultReverbLevel).toDouble());
    m_chorusLevel->setValue(m_settings.value(ChorusLevelSetting, DefaultChorusLevel).toDouble());
    m_soundfontLocation->setText(m_settings.value(SoundfontPathSetting).toString());
    m_soundfontGSLocation->setText(m_settings.value(SoundfontGSPathSetting).toString());
    m_romLocation->setText(m_settings.value(NukedRomPathSetting).toString());
    m_romSet->setCurrentIndex(std::max(0, m_romSet->findData(m_settings.value(NukedRomSetSetting, DefaultNukedRomSet))));
    const auto updateEngine = [=, this] {
        const bool nuked = m_engine->currentData().toInt() == NukedEngine;
        const bool external = m_engine->currentData().toInt() == ExternalEngine;
        const bool spessa = !nuked && !external;
        externalGroup->setVisible(external);
        m_gain->setEnabled(!external);
        m_fadeLength->setEnabled(!external);
        nukedGroup->setVisible(nuked);
        m_soundfontLocation->setEnabled(spessa);
        m_soundfontGSLocation->setEnabled(spessa);
        browseButton->setEnabled(spessa);
        browseGSButton->setEnabled(spessa);
        soundfontHintLabel->setVisible(spessa);
        m_voiceCount->setEnabled(spessa);
        m_interpolationFilter->setEnabled(spessa);
        m_effectsEnabled->setEnabled(spessa);
        m_reverbLevel->setEnabled(spessa && m_effectsEnabled->isChecked());
        m_chorusLevel->setEnabled(spessa && m_effectsEnabled->isChecked());
    };
    connect(m_engine, &QComboBox::currentIndexChanged, this, updateEngine);
    connect(m_effectsEnabled, &QCheckBox::toggled, this, updateEngine);
    m_engine->setCurrentIndex(std::max(0, m_engine->findData(m_settings.value(EngineSetting, DefaultEngine))));
    updateEngine();
}

void MIDIInputSettings::accept()
{
#ifdef MIDI_ENABLE_EXTERNAL
    if(m_engine->currentData().toInt() == ExternalEngine) {
        try { ExternalMIDI::open(m_externalPort->currentData().toString().toStdString()); }
        catch(const std::exception& e) {
            QMessageBox::warning(this, tr("External MIDI"), QString::fromUtf8(e.what()));
            return;
        }
        m_settings.setValue(ExternalPortSetting, m_externalPort->currentData());
    }
#endif
#ifdef MIDI_ENABLE_NUKED_SC55
    // Gain, loops and fade belong to playback, not to the booted hardware.
    // Compare before saving so unrelated edits preserve the idle device.
    const bool hardwareChanged =
        m_settings.value(EngineSetting, DefaultEngine).toInt() != m_engine->currentData().toInt() ||
        m_settings.value(NukedRomPathSetting).toString() != m_romLocation->text() ||
        m_settings.value(NukedRomSetSetting, DefaultNukedRomSet).toString() != m_romSet->currentData().toString();
    if(hardwareChanged) {
        NukedSC55Player::setPersistenceEnabled(false);
        NukedSC55Player::setPersistenceEnabled(m_engine->currentData().toInt() == NukedEngine);
    }
#endif
    m_settings.setValue(EngineSetting, m_engine->currentData());
    m_settings.setValue(NukedRomPathSetting, m_romLocation->text());
    m_settings.setValue(NukedRomSetSetting, m_romSet->currentData());
    m_settings.setValue(LoopCountSetting, m_loopCount->value());
    m_settings.setValue(FadeLengthSetting, m_fadeLength->value());
    m_settings.setValue(InterpolationSetting, m_interpolationFilter->currentData().toInt());
    m_settings.setValue(VoiceCountSetting, m_voiceCount->value());
    m_settings.setValue(GainSetting, m_gain->value());
    m_settings.setValue(EffectsEnabledSetting, m_effectsEnabled->isChecked());
    m_settings.setValue(ReverbLevelSetting, m_reverbLevel->value());
    m_settings.setValue(ChorusLevelSetting, m_chorusLevel->value());
    m_settings.setValue(SoundfontPathSetting, m_soundfontLocation->text());
    m_settings.setValue(SoundfontGSPathSetting, m_soundfontGSLocation->text());

    done(Accepted);
}

void MIDIInputSettings::reset()
{
    m_engine->setCurrentIndex(m_engine->findData(DefaultEngine));
    m_romLocation->clear();
    if(m_externalPort->count()) m_externalPort->setCurrentIndex(0);
    m_romSet->setCurrentIndex(m_romSet->findData(QString::fromLatin1(DefaultNukedRomSet)));
    m_loopCount->setValue(DefaultLoopCount);
    m_fadeLength->setValue(DefaultFadeLength);
    m_interpolationFilter->setCurrentIndex(
        m_interpolationFilter->findData(DefaultInterpolation));
    m_voiceCount->setValue(DefaultVoiceCount);
    m_gain->setValue(DefaultGain);
    m_effectsEnabled->setChecked(DefaultEffectsEnabled);
    m_reverbLevel->setValue(DefaultReverbLevel);
    m_chorusLevel->setValue(DefaultChorusLevel);
    m_soundfontLocation->clear();
    m_soundfontGSLocation->clear();
}

void MIDIInputSettings::getSoundfontPath()
{
    const QString soundfontPath = QFileDialog::getOpenFileName(this, tr("Select Soundfont bank"), QDir::homePath(), tr("Soundfont Banks (*.sf2 *.sf2pack *.sf3 *.sf4 *.dls *.sflist *.json)"));
    if(soundfontPath.isEmpty()) {
        return;
    }

    m_soundfontLocation->setText(soundfontPath);
}

void MIDIInputSettings::getSoundfontGSPath()
{
    const QString soundfontGSPath = QFileDialog::getOpenFileName(this, tr("Select GS mode Soundfont bank"), QDir::homePath(), tr("Soundfont Banks (*.sf2 *.sf2pack *.sf3 *.sf4 *.dls *.sflist *.json)"));
    if(soundfontGSPath.isEmpty()) {
        return;
    }

    m_soundfontGSLocation->setText(soundfontGSPath);
}
} // namespace Fooyin::MIDIInput
