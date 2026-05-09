#include "PluginProcessor.h"
#include "PluginEditor.h"

WillyBeatAudioProcessor::WillyBeatAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
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

    return layout;
}

void WillyBeatAudioProcessor::parameterChanged (const juce::String&, float)
{
    selectPattern();
}

void WillyBeatAudioProcessor::selectPattern()
{
    auto genre  = (Genre)  (int) apvts.getRawParameterValue ("genre")->load();
    auto type   = (PatType)(int) apvts.getRawParameterValue ("patType")->load();
    int  idx    = (int)          apvts.getRawParameterValue ("patIdx")->load();

    auto matches = library.get (genre, type);
    if (matches.empty())
    {
        // Fallback to first Regular if the requested type doesn't exist
        matches = library.get (genre, PatType::Regular);
    }

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

    // Each step is one 16th note = 0.25 PPQ
    constexpr double stepPPQ = 0.25;
    const double ppqStart    = ppqPosition;
    const double ppqEnd      = ppqStart + (double) blockSize * ppqPerSample;

    const float  gatePct      = apvts.getRawParameterValue ("gate")->load() / 100.0f;
    const double stepSamples  = (stepPPQ / ppqPerSample);
    const double noteLenSamp  = stepSamples * (double) gatePct;

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

    // Fire steps landing in this block
    long firstStep = (long) std::ceil  (ppqStart / stepPPQ - 1e-9);
    long lastStep  = (long) std::floor ((ppqEnd   / stepPPQ) - 1e-9);

    for (long globalStep = firstStep; globalStep <= lastStep; ++globalStep)
    {
        if (globalStep == lastFiredStep) continue;
        lastFiredStep = globalStep;

        int  patStep    = (int) (globalStep % numSteps);
        currentStep     = patStep;

        double stepPPQPos   = globalStep * stepPPQ;
        int    sampleOffset = (int) std::round ((stepPPQPos - ppqStart) / ppqPerSample);
        sampleOffset = juce::jlimit (0, blockSize - 1, sampleOffset);

        // Emit all active voices for this step
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            uint8_t vel = activePattern->velocities[t][patStep];
            if (vel == 0) continue;

            int note = kTrackNotes[t];
            midi.addEvent (juce::MidiMessage::noteOn (10, note, vel), sampleOffset);
            activeNotes.push_back ({ note, absoluteSample + sampleOffset + (long) noteLenSamp });
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

    // Find the drum track (channel 10, or the one with the most drum notes)
    const juce::MidiMessageSequence* drumTrack = nullptr;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* track = midiFile.getTrack (t);
        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            auto msg = track->getEventPointer(e)->message;
            if (msg.isNoteOn() && msg.getChannel() == 10)
            {
                drumTrack = track;
                break;
            }
        }
        if (drumTrack) break;
    }

    if (!drumTrack) return false;

    // Get tempo: assume 120 BPM if not found
    double bpm = 120.0;
    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        auto msg = drumTrack->getEventPointer(e)->message;
        if (msg.isTempoMetaEvent())
            bpm = 60.0 / msg.getTempoSecondsPerQuarterNote();
    }

    const double secondsPerStep = (60.0 / bpm) / 4.0; // 16th note duration

    DrumPattern p;
    p.name  = file.getFileNameWithoutExtension();
    p.genre = Genre::Rock; // default; user can reassign
    p.type  = PatType::Regular;

    // Quantise every note to the nearest 16th-note step
    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        auto msg = drumTrack->getEventPointer(e)->message;
        if (!msg.isNoteOn()) continue;

        int step = (int) std::round (msg.getTimeStamp() / secondsPerStep) % MAX_STEPS;
        int note = msg.getNoteNumber();
        uint8_t vel = (uint8_t) msg.getVelocity();

        // Map GM note number to our track slots
        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            if (kTrackNotes[t] == note)
            {
                p.velocities[t][step] = std::max (p.velocities[t][step], vel);
                break;
            }
        }
    }

    library.addUserPattern (p);
    selectPattern();
    return true;
}

void WillyBeatAudioProcessor::generateVariation()
{
    if (!activePattern) return;
    auto variation = generator.makeVariance (*activePattern);
    library.addUserPattern (variation);
    selectPattern();
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
    auto* root = state.createXml().release();
    auto userPats = library.toXml();
    root->addChildElement (userPats.release());
    copyXmlToBinary (*root, destData);
    delete root;
}

void WillyBeatAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (!xml) return;

    if (auto* userPats = xml->getChildByName ("UserPatterns"))
        library.fromXml (*userPats);

    if (xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

    selectPattern();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WillyBeatAudioProcessor();
}
