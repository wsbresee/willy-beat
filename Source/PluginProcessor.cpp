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
    for (int t = 0; t < NUM_TRACKS; ++t) lastFiredAbsTick[t] = -1;
    currentStep = 0;
    wasPlaying  = false;
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
            currentStep = 0;
            for (int t = 0; t < NUM_TRACKS; ++t) lastFiredAbsTick[t] = -1;
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

    constexpr double stepPPQ = 0.25;   // one 16th = 0.25 PPQ, used for slop/note-length defaults
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
    // hits are currently in activePattern.

    // Pattern shape in tick / PPQ space.
    const long   totalTicks  = activePattern->totalTicks();
    if (totalTicks <= 0) { absoluteSample += blockSize; return; }
    const double patternPPQ  = (double) totalTicks / (double) PPQN;

    // 16th-grid swing: delay the second 16th of every 8th-note pair.
    // Hits that don't land on a 16th boundary are not swung.
    auto swungPPQOffset = [&] (int patTick) -> double
    {
        if (swingDelay == 0.0) return 0.0;
        constexpr int sixteenthTicks = PPQN / 4;  // 24
        if (patTick % sixteenthTicks != 0) return 0.0;
        return ((patTick / sixteenthTicks) & 1) ? swingDelay : 0.0;
    };

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

    // Update currentStep for the UI's playhead readout. The "step" here is
    // a 16th-note column so that v1-style 16-step displays keep working;
    // when we add variable subdivisions in the UI it'll be reinterpreted.
    {
        const double patPPQ = std::fmod (ppqStart, patternPPQ);
        const double posPPQ = (patPPQ < 0.0) ? patPPQ + patternPPQ : patPPQ;
        currentStep = (int) std::floor (posPPQ / stepPPQ) % juce::jmax (1, activePattern->numSteps);
    }

    // For each hit in the pattern, find every cycle whose global PPQ
    // falls inside [ppqStart, ppqEnd) and schedule the note.
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        const int note = kTrackNotes[t];
        for (const auto& hit : activePattern->hits[t])
        {
            const double hitPPQInPattern = (double) hit.tick / (double) PPQN + swungPPQOffset (hit.tick);

            // smallest cycle k with k*patternPPQ + hitPPQInPattern >= ppqStart
            long cycle = (long) std::ceil ((ppqStart - hitPPQInPattern) / patternPPQ);

            for (; ; ++cycle)
            {
                const double hitPPQ = cycle * patternPPQ + hitPPQInPattern;
                if (hitPPQ <  ppqStart - 1e-9) continue;        // shouldn't happen with ceil()
                if (hitPPQ >= ppqEnd   - 1e-9) break;

                const long absTick = cycle * totalTicks + hit.tick;
                if (absTick <= lastFiredAbsTick[t]) continue;   // already fired
                lastFiredAbsTick[t] = absTick;

                int baseSampleOffset = (int) std::round ((hitPPQ - ppqStart) / ppqPerSample);
                baseSampleOffset = juce::jlimit (0, blockSize - 1, baseSampleOffset);

                uint8_t vel = hit.velocity;
                if (humanize > 0)
                {
                    int deviation = audioRng.nextInt (2 * humanize + 1) - humanize;
                    vel = (uint8_t) juce::jlimit (1, 127, (int) hit.velocity + deviation);
                }

                int noteOffset = baseSampleOffset;
                if (maxFeelSamp > 0.0)
                {
                    double r = (audioRng.nextDouble() + audioRng.nextDouble()) * 0.5 - 0.5;
                    double velFactor = 1.0 - 0.4 * ((double) vel / 127.0);
                    noteOffset = juce::jlimit (0, blockSize - 1,
                        (int) std::round (baseSampleOffset + r * maxFeelSamp * velFactor));
                }

                midi.addEvent (juce::MidiMessage::noteOn (10, note, vel), noteOffset);
                activeNotes.push_back ({ note, absoluteSample + noteOffset + (long) noteLenSamp });

                if (internalOn)
                    stockDrums.noteOn (note, (float) vel / 127.0f);
            }
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
    if (! stream.openedOk()) return false;

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream)) return false;

    const int midiPPQ = midiFile.getTimeFormat();
    if (midiPPQ <= 0) return false;          // SMPTE-format files unsupported

    // Find the drum track. Prefer channel 10 (GM drums); fall back to the
    // first track containing any noteOn so DAW drum patts that arrive on
    // odd channels still import.
    const juce::MidiMessageSequence* drumTrack = nullptr;
    bool drumChannelStrict = false;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        auto* track = midiFile.getTrack (t);
        for (int e = 0; e < track->getNumEvents(); ++e)
        {
            const auto& msg = track->getEventPointer (e)->message;
            if (msg.isNoteOn() && msg.getChannel() == 10)
            { drumTrack = track; drumChannelStrict = true; break; }
        }
        if (drumTrack) break;
    }
    if (drumTrack == nullptr)
    {
        for (int t = 0; t < midiFile.getNumTracks(); ++t)
        {
            auto* track = midiFile.getTrack (t);
            for (int e = 0; e < track->getNumEvents(); ++e)
                if (track->getEventPointer (e)->message.isNoteOn())
                { drumTrack = track; break; }
            if (drumTrack) break;
        }
    }
    if (drumTrack == nullptr) return false;

    // Detect time signature from any track's meta events (commonly track 0).
    int tsNum = 4, tsDen = 4;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* tr = midiFile.getTrack (t);
        for (int e = 0; e < tr->getNumEvents(); ++e)
        {
            const auto& msg = tr->getEventPointer (e)->message;
            if (msg.isTimeSignatureMetaEvent())
            { msg.getTimeSignatureInfo (tsNum, tsDen); goto tsFound; }
        }
    }
    tsFound:

    DrumPattern p;
    p.name       = file.getFileNameWithoutExtension();
    p.type       = PatType::Regular;
    p.timeSigNum = juce::jlimit (1, 32, tsNum);
    p.timeSigDen = (tsDen == 1 || tsDen == 2 || tsDen == 4 || tsDen == 8 || tsDen == 16) ? tsDen : 4;

    auto selTags = getSelectedGenreTags();
    p.genres = selTags.isEmpty() ? juce::StringArray { "Imported" } : selTags;

    // Decide bar count: enough to hold the file's actual content, capped at 8.
    constexpr int kMaxBars = 8;
    const double midiTicksPerBar = (double) midiPPQ * 4.0 * (double) p.timeSigNum / (double) p.timeSigDen;
    double lastNoteTick = 0.0;
    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        const auto& msg = drumTrack->getEventPointer (e)->message;
        if (msg.isNoteOn()) lastNoteTick = juce::jmax (lastNoteTick, msg.getTimeStamp());
    }
    p.bars = juce::jlimit (1, kMaxBars, (int) std::ceil ((lastNoteTick + 1.0) / midiTicksPerBar));

    // Scale MIDI file ticks → pattern ticks (PPQN=96).
    const double tickScale = (double) PPQN / (double) midiPPQ;
    const long   patternTotal = p.totalTicks();

    for (int e = 0; e < drumTrack->getNumEvents(); ++e)
    {
        const auto& msg = drumTrack->getEventPointer (e)->message;
        if (! msg.isNoteOn()) continue;
        if (drumChannelStrict && msg.getChannel() != 10) continue;

        const long patTick = (long) std::round (msg.getTimeStamp() * tickScale);
        if (patTick < 0 || patTick >= patternTotal) continue;

        const int     note = msg.getNoteNumber();
        const uint8_t vel  = (uint8_t) msg.getVelocity();
        if (vel == 0) continue;

        for (int t = 0; t < NUM_TRACKS; ++t)
            if (kTrackNotes[t] == note)
            {
                p.hits[t].push_back ({ (int) patTick, vel });
                break;
            }
    }
    for (int t = 0; t < NUM_TRACKS; ++t)
        std::sort (p.hits[t].begin(), p.hits[t].end(),
                   [] (const DrumHit& a, const DrumHit& b) { return a.tick < b.tick; });

    p.syncLegacyFromHits();
    p.computeDensity();

    apvts.removeParameterListener ("patIdx", this);
    library.savePattern (p, getPresetsDirectory());
    reloadLibrary();
    navigateToPattern (p);
    selectPattern();
    apvts.addParameterListener ("patIdx", this);
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

bool WillyBeatAudioProcessor::navigateToPatternByName (const juce::String& name)
{
    for (const auto& pat : library.all())
    {
        if (pat.name.equalsIgnoreCase (name))
        {
            apvts.removeParameterListener ("patIdx", this);
            navigateToPattern (pat);
            selectPattern();
            apvts.addParameterListener ("patIdx", this);
            return true;
        }
    }
    return false;
}

void WillyBeatAudioProcessor::generateComposite()
{
    auto tags = getSelectedGenreTags();
    generateComposite (tags, tags);
}

void WillyBeatAudioProcessor::generateComposite (const juce::StringArray& sourcePool,
                                                  const juce::StringArray& assignedGenres)
{
    juce::int64 seed = juce::Random::getSystemRandom().nextInt64();

    auto composite = library.makeComposite (sourcePool, PatType::Regular, seed);
    if (composite.name.isEmpty()) return;

    // Saved-pattern genres reflect the user's intent (the chip bar at the
    // moment of Generate), not the random expanded pool used as sources.
    composite.genres = assignedGenres;

    // Each Generate yields a fresh pattern with a unique filename so users can
    // keep clicking and accumulate variations.  Name format: "{firstTag} {N}"
    // (or just "Generated {N}" with no tags), where N is the next free slot.
    juce::String prefix = assignedGenres.isEmpty() ? juce::String ("Generated")
                                                   : assignedGenres[0];
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
    // Grid edits still mutate the legacy velocities[][] array. Push those
    // edits into the tick-based hits[] before persistence — file writes,
    // playback, and density rebalancing all read from hits[] now.
    p.syncHitsFromLegacy();
    p.computeDensity();
    auto f = library.savePattern (p, dir);
    p.sourceFile = f;
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
