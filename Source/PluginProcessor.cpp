#include "PluginProcessor.h"
#include "PluginEditor.h"

WillyBeatAudioProcessor::WillyBeatAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Install built-in presets on first run, then load all .beat files
    auto dir = getPresetsDirectory();
    if (!dir.exists() || dir.findChildFiles (juce::File::findFiles, false, "*.beat").isEmpty())
        library.installDefaultsTo (dir);
    library.loadFromDirectory (dir);

    if (! apvts.state.hasProperty ("genreTags"))
        apvts.state.setProperty ("genreTags", juce::String(), nullptr);

    apvts.addParameterListener ("patIdx", this);
    selectPattern();
}

WillyBeatAudioProcessor::~WillyBeatAudioProcessor()
{
    apvts.removeParameterListener ("patIdx", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
WillyBeatAudioProcessor::createParameterLayout()
{
    using namespace juce;

    // ── Macros group ──────────────────────────────────────────────────────
    // These are the "performance" knobs.  Native Instruments hardware
    // (Komplete Kontrol Mk2/Mk3, Maschine) maps the first VST3 parameter
    // group's children to the eight rotary encoders, in order.  To wire up
    // a new hardware knob, append another AudioParameterFloat/Int below;
    // up to the eighth child gets a hardware encoder by default.
    //
    // Names are kept short (≤ 9 chars where possible) so they fit on the
    // hardware's small displays without truncation.
    auto macros = std::make_unique<AudioProcessorParameterGroup> (
        "macros", "Macros", "|");

    macros->addChild (std::make_unique<AudioParameterFloat> (
        ParameterID { "duration", 1 }, "Duration",
        NormalisableRange<float> (10.0f, 100.0f, 1.0f), 80.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    macros->addChild (std::make_unique<AudioParameterFloat> (
        ParameterID { "dynamics", 1 }, "Dynamics",
        NormalisableRange<float> (0.0f, 50.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("vel")));

    macros->addChild (std::make_unique<AudioParameterFloat> (
        ParameterID { "swing", 1 }, "Swing",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    macros->addChild (std::make_unique<AudioParameterFloat> (
        ParameterID { "slop", 1 }, "Slop",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // density 0-200%: ≤100 filters by velocity threshold, >100 augments
    // editingCopy with extra hits pulled from same-genre source patterns.
    macros->addChild (std::make_unique<AudioParameterFloat> (
        ParameterID { "density", 1 }, "Density",
        NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // Fill region knobs (all 0-16 sixteenth notes).  Three regions of the
    // export are sourced from a fill pattern instead of the main pattern:
    //   • fillStart — leading N steps (head of fill pattern)
    //   • fillMid   — N scattered steps in the middle of the export
    //   • fillSteps — trailing N steps (tail of fill pattern)
    macros->addChild (std::make_unique<AudioParameterInt> (
        ParameterID { "fillStart", 1 }, "Start", 0, 16, 0));

    macros->addChild (std::make_unique<AudioParameterInt> (
        ParameterID { "fillMid", 1 }, "Mid", 0, 16, 0));

    macros->addChild (std::make_unique<AudioParameterInt> (
        ParameterID { "fillSteps", 1 }, "End", 0, 16, 0));

    // ── Pattern selector group (subsequent NI hardware page) ─────────────
    // Genre tags are stored as a CSV property in apvts.state (not as an APVTS
    // parameter) because the tag set is dynamic and multi-select.  Pattern
    // type is no longer user-selectable — fills are handled by the fillSteps
    // rotary, and the main pattern always picks from non-fill types.
    auto pattern = std::make_unique<AudioProcessorParameterGroup> (
        "pattern", "Pattern", "|");

    // Wide range so we can navigate the full library — the editor's IncDec
    // slider just steps one-at-a-time and selectPattern clamps to the
    // current filtered list size.  Anything above 15 was previously lost,
    // which made repeated Generate clicks appear to do nothing.
    pattern->addChild (std::make_unique<AudioParameterInt> (
        ParameterID { "patIdx", 1 }, "Pattern", 0, 999, 0));

    // Internal preview synth toggle. Default OFF so the plugin defers to the
    // downstream sampler / external instrument for actual sound.
    pattern->addChild (std::make_unique<AudioParameterBool> (
        ParameterID { "internalSound", 1 }, "Internal Sound", false));

    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::move (macros));
    layout.add (std::move (pattern));
    return layout;
}

juce::StringArray WillyBeatAudioProcessor::getSelectedGenreTags() const
{
    auto csv = apvts.state.getProperty ("genreTags", juce::String()).toString();
    juce::StringArray result;
    result.addTokens (csv, ",", "");
    for (auto& s : result) s = s.trim();
    result.removeEmptyStrings();
    return result;
}

void WillyBeatAudioProcessor::setSelectedGenreTags (const juce::StringArray& tags)
{
    juce::StringArray cleaned;
    for (const auto& t : tags)
        if (t.trim().isNotEmpty())
            cleaned.add (t.trim());
    apvts.state.setProperty ("genreTags", cleaned.joinIntoString (","), nullptr);
    selectPattern();
}

void WillyBeatAudioProcessor::parameterChanged (const juce::String&, float)
{
    selectPattern();
}

void WillyBeatAudioProcessor::selectPattern()
{
    // The chip bar above the grid IS the active pattern's tags (not a
    // library filter), so patIdx steps through every non-fill pattern in
    // load order without any genre-based culling. Fills are still hidden
    // from the slot list since they're handled by the fill rotaries.
    int idx = (int) apvts.getRawParameterValue ("patIdx")->load();

    std::vector<const DrumPattern*> slots;
    for (const auto& p : library.all())
        if (p.type == PatType::Regular || p.type == PatType::Variance)
            slots.push_back (&p);

    if (slots.empty())
        for (const auto& p : library.all())
            slots.push_back (&p);

    if (slots.empty())
    {
        activePattern = nullptr;
        return;
    }

    idx = juce::jlimit (0, (int) slots.size() - 1, idx);
    activePattern = slots[(size_t) idx];
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
    // Inherit the pattern's tags as the user's selection so density
    // augmentation, fill matching, and the chip bar all reflect the same
    // scope as the active pattern.
    apvts.state.setProperty ("genreTags", p.genres.joinIntoString (","), nullptr);

    // patIdx is now an index into the unfiltered slot list (see selectPattern).
    int patIdx = 0;
    int i      = 0;
    for (const auto& m : library.all())
    {
        if (m.type != PatType::Regular && m.type != PatType::Variance) continue;
        if (m.name == p.name) { patIdx = i; break; }
        ++i;
    }

    if (auto* iParam = apvts.getParameter ("patIdx"))
        iParam->setValueNotifyingHost (iParam->convertTo0to1 ((float) patIdx));
}

//==============================================================================

void WillyBeatAudioProcessor::prepareToPlay (double sampleRate, int)
{
    activeNotes.clear();
    absoluteSample = 0;
    lastFiredStep  = -1;
    currentStep    = 0;
    wasPlaying     = false;
    stockDrums.prepare (sampleRate);
}

void WillyBeatAudioProcessor::releaseResources() {}

bool WillyBeatAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================

void WillyBeatAudioProcessor::processBlock (juce::AudioBuffer<float>& buf,
                                             juce::MidiBuffer& midi)
{
    midi.clear();
    buf.clear();

    const bool internalOn = apvts.getRawParameterValue ("internalSound")->load() > 0.5f;

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
            stockDrums.allNotesOff();
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

    const float  gatePct      = apvts.getRawParameterValue ("duration")->load() / 100.0f;
    const int    humanize     = (int) apvts.getRawParameterValue ("dynamics")->load();
    const double swingDelay   = apvts.getRawParameterValue ("swing")->load() * (stepPPQ * 0.5 / 100.0);
    const double maxFeelSamp  = apvts.getRawParameterValue ("slop")->load() / 100.0
                                    * (stepPPQ / ppqPerSample) * 0.08;
    const double stepSamples  = stepPPQ / ppqPerSample;
    const double noteLenSamp  = stepSamples * (double) gatePct;

    // Density is applied in the editor's applyDensity pass before the
    // pattern reaches the library, so processBlock just plays whatever
    // velocities are currently in activePattern.

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

            // Internal preview voice. Voices started inside this block render
            // from their own phase=0, so timing offset within the block is
            // approximate (good enough for scrub-style monitoring).
            if (internalOn)
                stockDrums.noteOn (note, (float) vel / 127.0f);
        }
    }

    if (internalOn)
        stockDrums.render (buf, 0, blockSize);

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
    p.name = file.getFileNameWithoutExtension();
    p.type = PatType::Regular;

    // Inherit the currently selected genre tags (or default to "Imported")
    auto selTags = getSelectedGenreTags();
    if (selTags.isEmpty())
        p.genres.add ("Imported");
    else
        p.genres = selTags;

    // Take only the first 4 bars worth of notes (anything later is ignored),
    // then fold those bars onto the 1-bar display grid by mod-16. Quantize
    // by rounding each onset to the nearest 16th-note step.
    constexpr int kMaxBars = 4;
    constexpr int kCutoffSteps = kMaxBars * MAX_STEPS;

    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        auto msg = drumTrack->getEventPointer(e)->message;
        if (!msg.isNoteOn()) continue;

        int rawStep = (int) std::round (msg.getTimeStamp() / secondsPerStep);
        if (rawStep < 0 || rawStep >= kCutoffSteps) continue;
        int step = rawStep % MAX_STEPS;
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

    p.computeDensity();

    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (p, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (p);
    selectPattern();

    apvts.addParameterListener ("patIdx",  this);

    return true;
}

void WillyBeatAudioProcessor::generateVariation()
{
    if (!activePattern) return;
    auto variation = generator.makeVariance (*activePattern);

    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (variation, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (variation);
    selectPattern();

    apvts.addParameterListener ("patIdx",  this);
}

void WillyBeatAudioProcessor::generateComposite()
{
    auto tags = getSelectedGenreTags();
    juce::int64 seed = juce::Random::getSystemRandom().nextInt64();

    auto composite = library.makeComposite (tags, PatType::Regular, seed);
    if (composite.name.isEmpty()) return;

    // Each Generate yields a fresh pattern with a unique filename so users can
    // keep clicking and accumulate variations.  Name format: "{firstTag} {N}"
    // (or just "Generated {N}" with no tags), where N is the next free slot.
    juce::String prefix = tags.isEmpty() ? juce::String ("Generated")
                                         : tags[0];
    auto dir = getPresetsDirectory();
    int n = 1;
    juce::String candidate;
    for (;;)
    {
        candidate = prefix + " " + juce::String (n);
        auto safe = candidate.replaceCharacters ("/\\:*?\"<>|", "_________");
        if (! dir.getChildFile (safe + ".beat").existsAsFile()) break;
        ++n;
    }
    composite.name = candidate;

    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (composite, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (composite);
    selectPattern();

    apvts.addParameterListener ("patIdx",  this);
}

void WillyBeatAudioProcessor::autoSavePattern (DrumPattern& p)
{
    auto dir = getPresetsDirectory();
    dir.createDirectory();
    p.computeDensity();
    auto f = library.savePattern (p, dir);
    p.sourceFile = f;
    // Keep the in-memory library copy in sync so activePattern pointer stays current
    library.updatePattern (p);
}

void WillyBeatAudioProcessor::saveEditedPattern (const DrumPattern& p)
{
    apvts.removeParameterListener ("patIdx",  this);

    library.savePattern (p, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (p);
    selectPattern();

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
bool WillyBeatAudioProcessor::isMidiEffect() const { return false; }
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
