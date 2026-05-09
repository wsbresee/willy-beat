#include "PluginProcessor.h"
#include "PluginEditor.h"

WillyBeatAudioProcessor::WillyBeatAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Install built-in presets on first run, then load all .beat files
    auto dir = getPresetsDirectory();
    if (!dir.exists() || dir.findChildFiles (juce::File::findFiles, false, "*.beat").isEmpty())
        library.installDefaultsTo (dir);
    library.loadFromDirectory (dir);

    apvts.addParameterListener ("genre",   this);
    apvts.addParameterListener ("patType", this);
    apvts.addParameterListener ("patIdx",  this);
    selectPattern();
}

WillyBeatAudioProcessor::~WillyBeatAudioProcessor()
{
    apvts.removeParameterListener ("genre",   this);
    apvts.removeParameterListener ("patType", this);
    apvts.removeParameterListener ("patIdx",  this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
WillyBeatAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray genres;
    for (int i = 0; i < (int) Genre::NUM_GENRES; ++i)
        genres.add (kGenreNames[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "genre", 1 }, "Genre", genres, 0));

    juce::StringArray types;
    for (int i = 0; i < (int) PatType::NUM_TYPES; ++i)
        types.add (kPatTypeNames[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "patType", 1 }, "Type", types, 0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "patIdx", 1 }, "Pattern", 0, 15, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gate", 1 }, "Gate",
        juce::NormalisableRange<float> (10.0f, 100.0f, 1.0f), 80.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "humanize", 1 }, "Humanize",
        juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("vel")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "swing", 1 }, "Swing",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feel", 1 }, "Feel",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

void WillyBeatAudioProcessor::parameterChanged (const juce::String&, float)
{
    selectPattern();
}

void WillyBeatAudioProcessor::selectPattern()
{
    auto genre  = (Genre)   (int) apvts.getRawParameterValue ("genre")->load();
    auto type   = (PatType) (int) apvts.getRawParameterValue ("patType")->load();
    int  idx    = (int)           apvts.getRawParameterValue ("patIdx")->load();

    auto matches = library.get (genre, type);
    if (matches.empty())
        matches = library.get (genre, PatType::Regular);

    if (!matches.empty())
    {
        idx = juce::jlimit (0, (int) matches.size() - 1, idx);
        activePattern = matches[(size_t) idx];
    }
    else
    {
        activePattern = nullptr;
    }
}

juce::File WillyBeatAudioProcessor::getPresetsDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("WillyBeat/Presets");
}

void WillyBeatAudioProcessor::reloadLibrary()
{
    activePattern = nullptr;                    // prevent dangling pointer on audio thread
    library.loadFromDirectory (getPresetsDirectory());
    selectPattern();
}

void WillyBeatAudioProcessor::navigateToPattern (const DrumPattern& p)
{
    // Update all three APVTS parameters in one pass (listeners removed by caller)
    auto* gParam = apvts.getParameter ("genre");
    auto* tParam = apvts.getParameter ("patType");
    auto* iParam = apvts.getParameter ("patIdx");

    if (gParam) gParam->setValueNotifyingHost (gParam->convertTo0to1 ((float) (int) p.genre));
    if (tParam) tParam->setValueNotifyingHost (tParam->convertTo0to1 ((float) (int) p.type));

    auto matches = library.get (p.genre, p.type);
    int idx = 0;
    for (int i = 0; i < (int) matches.size(); ++i)
        if (matches[i]->name == p.name) { idx = i; break; }

    if (iParam) iParam->setValueNotifyingHost (iParam->convertTo0to1 ((float) idx));
}

//==============================================================================

void WillyBeatAudioProcessor::prepareToPlay (double, int)
{
    activeNotes.clear();
    absoluteSample = 0;
    lastFiredStep  = -1;
    currentStep    = 0;
    wasPlaying     = false;
}

void WillyBeatAudioProcessor::releaseResources() {}

//==============================================================================

void WillyBeatAudioProcessor::processBlock (juce::AudioBuffer<float>& /*buf*/,
                                             juce::MidiBuffer& midi)
{
    midi.clear();

    if (!activePattern) return;

    auto* ph = getPlayHead();
    if (!ph) return;

    auto posOpt = ph->getPosition();
    if (!posOpt.hasValue()) return;
    auto& pos = *posOpt;

    if (!pos.getIsPlaying())
    {
        if (wasPlaying)
        {
            killAllNotes (midi, 0);
            currentStep   = 0;
            lastFiredStep = -1;
        }
        wasPlaying = false;
        absoluteSample += getBlockSize();
        return;
    }
    wasPlaying = true;

    const double bpm         = pos.getBpm().hasValue()         ? *pos.getBpm()         : 120.0;
    const double ppqPosition = pos.getPpqPosition().hasValue() ? *pos.getPpqPosition() : 0.0;
    const double sr          = getSampleRate();
    const int    blockSize   = getBlockSize();
    const double ppqPerSample = bpm / 60.0 / sr;

    constexpr double stepPPQ = 0.25;   // one 16th note
    const double ppqStart    = ppqPosition;
    const double ppqEnd      = ppqStart + (double) blockSize * ppqPerSample;

    const float  gatePct      = apvts.getRawParameterValue ("gate")->load() / 100.0f;
    const int    humanize     = (int) apvts.getRawParameterValue ("humanize")->load();
    // swing 0-100: at ~67 the off-beats land on triplet positions (shuffle/jazz feel)
    const double swingDelay   = apvts.getRawParameterValue ("swing")->load() * (stepPPQ * 0.5 / 100.0);
    // feel 0-100: per-note timing jitter, bell-curve, velocity-weighted
    const double maxFeelSamp  = apvts.getRawParameterValue ("feel")->load() / 100.0
                                    * (stepPPQ / ppqPerSample) * 0.08;
    const double stepSamples  = stepPPQ / ppqPerSample;
    const double noteLenSamp  = stepSamples * (double) gatePct;

    // Actual PPQ position of globalStep, including swing offset on odd steps
    auto stepToPPQ = [&] (long step) -> double
    {
        return step * stepPPQ + ((step & 1L) ? swingDelay : 0.0);
    };

    const int numSteps = activePattern->numSteps;

    // Drain note-offs
    for (auto it = activeNotes.begin(); it != activeNotes.end();)
    {
        long rel = it->offAtSample - absoluteSample;
        if (rel < blockSize)
        {
            midi.addEvent (juce::MidiMessage::noteOff (10, it->note),
                           (int) juce::jlimit (0L, (long) blockSize - 1, rel));
            it = activeNotes.erase (it);
        }
        else ++it;
    }

    // Search ±1 beyond the nominal step range so swung steps near block boundaries
    // are never missed.
    long candidateFirst = (long) std::floor (ppqStart / stepPPQ) - 1;
    long candidateLast  = (long) std::ceil  (ppqEnd   / stepPPQ) + 1;

    for (long globalStep = candidateFirst; globalStep <= candidateLast; ++globalStep)
    {
        double stepPPQPos = stepToPPQ (globalStep);
        if (stepPPQPos <  ppqStart - 1e-9) continue;
        if (stepPPQPos >= ppqEnd   - 1e-9) continue;
        if (globalStep == lastFiredStep)   continue;
        lastFiredStep = globalStep;

        int patStep = (int) (((globalStep % numSteps) + numSteps) % numSteps);
        currentStep = patStep;

        int baseSampleOffset = (int) std::round ((stepPPQPos - ppqStart) / ppqPerSample);
        baseSampleOffset = juce::jlimit (0, blockSize - 1, baseSampleOffset);

        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            uint8_t stored = activePattern->velocities[t][patStep];
            if (stored == 0) continue;

            uint8_t vel = stored;
            if (humanize > 0)
            {
                int deviation = audioRng.nextInt (2 * humanize + 1) - humanize;
                vel = (uint8_t) juce::jlimit (1, 127, (int) stored + deviation);
            }

            // Per-note timing jitter — bell curve, louder hits are more on-time
            int noteOffset = baseSampleOffset;
            if (maxFeelSamp > 0.0)
            {
                double r = (audioRng.nextDouble() + audioRng.nextDouble()) * 0.5 - 0.5;
                double velFactor = 1.0 - 0.4 * ((double) vel / 127.0);
                noteOffset = juce::jlimit (0, blockSize - 1,
                    (int) std::round (baseSampleOffset + r * maxFeelSamp * velFactor));
            }

            int note = kTrackNotes[t];
            midi.addEvent (juce::MidiMessage::noteOn (10, note, vel), noteOffset);
            activeNotes.push_back ({ note, absoluteSample + noteOffset + (long) noteLenSamp });
        }
    }

    absoluteSample += blockSize;
}

//==============================================================================

bool WillyBeatAudioProcessor::loadMidiFile (const juce::File& file)
{
    juce::FileInputStream stream (file);
    if (!stream.openedOk()) return false;

    juce::MidiFile midiFile;
    if (!midiFile.readFrom (stream)) return false;

    midiFile.convertTimestampTicksToSeconds();

    const juce::MidiMessageSequence* drumTrack = nullptr;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* track = midiFile.getTrack (t);
        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            if (track->getEventPointer(e)->message.isNoteOn() &&
                track->getEventPointer(e)->message.getChannel() == 10)
            {
                drumTrack = track;
                break;
            }
        }
        if (drumTrack) break;
    }
    if (!drumTrack) return false;

    double bpm = 120.0;
    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        auto msg = drumTrack->getEventPointer(e)->message;
        if (msg.isTempoMetaEvent())
            bpm = 60.0 / msg.getTempoSecondsPerQuarterNote();
    }

    const double secondsPerStep = (60.0 / bpm) / 4.0;

    DrumPattern p;
    p.name  = file.getFileNameWithoutExtension();
    p.genre = (Genre) (int) apvts.getRawParameterValue ("genre")->load();
    p.type  = PatType::Regular;

    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        auto msg = drumTrack->getEventPointer(e)->message;
        if (!msg.isNoteOn()) continue;

        int step = (int) std::round (msg.getTimeStamp() / secondsPerStep) % MAX_STEPS;
        int note = msg.getNoteNumber();
        uint8_t vel = (uint8_t) msg.getVelocity();

        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            if (kTrackNotes[t] == note)
            {
                p.velocities[t][step] = std::max (p.velocities[t][step], vel);
                break;
            }
        }
    }

    apvts.removeParameterListener ("genre",   this);
    apvts.removeParameterListener ("patType", this);
    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (p, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (p);
    selectPattern();

    apvts.addParameterListener ("genre",   this);
    apvts.addParameterListener ("patType", this);
    apvts.addParameterListener ("patIdx",  this);

    return true;
}

void WillyBeatAudioProcessor::generateVariation()
{
    if (!activePattern) return;
    auto variation = generator.makeVariance (*activePattern);

    apvts.removeParameterListener ("genre",   this);
    apvts.removeParameterListener ("patType", this);
    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (variation, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (variation);
    selectPattern();

    apvts.addParameterListener ("genre",   this);
    apvts.addParameterListener ("patType", this);
    apvts.addParameterListener ("patIdx",  this);
}

void WillyBeatAudioProcessor::saveEditedPattern (const DrumPattern& p)
{
    apvts.removeParameterListener ("genre",   this);
    apvts.removeParameterListener ("patType", this);
    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (p, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (p);
    selectPattern();

    apvts.addParameterListener ("genre",   this);
    apvts.addParameterListener ("patType", this);
    apvts.addParameterListener ("patIdx",  this);
}

//==============================================================================

void WillyBeatAudioProcessor::killAllNotes (juce::MidiBuffer& midi, int offset)
{
    for (auto& n : activeNotes)
        midi.addEvent (juce::MidiMessage::noteOff (10, n.note), offset);
    activeNotes.clear();
}

//==============================================================================

const juce::String WillyBeatAudioProcessor::getName() const { return JucePlugin_Name; }
bool WillyBeatAudioProcessor::acceptsMidi()  const { return true; }
bool WillyBeatAudioProcessor::producesMidi() const { return true; }
bool WillyBeatAudioProcessor::isMidiEffect() const { return true; }
double WillyBeatAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  WillyBeatAudioProcessor::getNumPrograms()              { return 1; }
int  WillyBeatAudioProcessor::getCurrentProgram()           { return 0; }
void WillyBeatAudioProcessor::setCurrentProgram (int)       {}
const juce::String WillyBeatAudioProcessor::getProgramName (int) { return {}; }
void WillyBeatAudioProcessor::changeProgramName (int, const juce::String&) {}
bool WillyBeatAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* WillyBeatAudioProcessor::createEditor()
{
    return new WillyBeatAudioProcessorEditor (*this);
}

void WillyBeatAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WillyBeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (!xml) return;
    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    selectPattern();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WillyBeatAudioProcessor();
}
