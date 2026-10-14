#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BoriyaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "WARMTH", 1 }, "Warmth", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "SPACE", 1 }, "Space", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "FILTER", 1 }, "Filter", 40.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER", 1 }, "Master", 0.0f, 2.0f, 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "VOL1", 1 }, "Vol 1", 0.0f, 1.5f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "VOL2", 1 }, "Vol 2", 0.0f, 1.5f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "VOL3", 1 }, "Vol 3", 0.0f, 1.5f, 1.0f));

    return { params.begin(), params.end() };
}

BoriyaAudioProcessor::BoriyaAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BoriyaParams", createParameterLayout())
{
    formatManager.registerBasicFormats();
    generatedOutput.setSize(2, 44100 * 4);
    generatedOutput.clear();

    for (int i = 0; i < 2; ++i)
        voices[i].active = false;
}

BoriyaAudioProcessor::~BoriyaAudioProcessor() {}

const juce::String BoriyaAudioProcessor::getName() const { return "BORIYA AI"; }
bool BoriyaAudioProcessor::acceptsMidi() const { return true; }
bool BoriyaAudioProcessor::producesMidi() const { return false; }
bool BoriyaAudioProcessor::isMidiEffect() const { return false; }
double BoriyaAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BoriyaAudioProcessor::getNumPrograms() { return 1; }
int BoriyaAudioProcessor::getCurrentProgram() { return 0; }
void BoriyaAudioProcessor::setCurrentProgram(int) {}
const juce::String BoriyaAudioProcessor::getProgramName(int) { return "Default"; }
void BoriyaAudioProcessor::changeProgramName(int, const juce::String&) {}

void BoriyaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    double sRate = (sampleRate > 8000.0) ? sampleRate : 44100.0;
    int blockSize = std::max(16, samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = 2;

    reverb.prepare(spec);
    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    stepLength = static_cast<int>(sSec * sRate);

    for (int i = 0; i < 2; ++i)
        voices[i].active = false;
}

void BoriyaAudioProcessor::releaseResources()
{
    reverb.reset();
    filter.reset();
    for (int i = 0; i < 2; ++i)
        voices[i].active = false;
}

bool BoriyaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Stereo / Mono output allowed, input strictly disabled for Synth
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    if (!layouts.getMainInputChannelSet().isDisabled())
        return false;

    return true;
}

void BoriyaAudioProcessor::makeSlices(int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];
    lyr.slices.clear();

    int total = lyr.buffer.getNumSamples();
    if (total == 0) return;

    int cuts = 16;
    int step = total / cuts;
    for (int i = 0; i < cuts; ++i)
        lyr.slices.push_back(i * step);

    double sRate = (getSampleRate() > 8000.0) ? getSampleRate() : 44100.0;
    lyr.dna = neuralEngine.extractDNA(lyr.buffer.getReadPointer(0), total, sRate);
}

void BoriyaAudioProcessor::loadLoopIntoLayer(int layerIdx, const juce::File& file)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        lyr.buffer.setSize(2, static_cast<int>(reader->lengthInSamples));
        reader->read(&lyr.buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        lyr.isLoaded = true;
        makeSlices(layerIdx);

        triggerAiComposition();
        playState.store(false);
        for (int i = 0; i < 2; ++i)
            voices[i].active = false;
    }
}

void BoriyaAudioProcessor::triggerAiComposition()
{
    std::vector<NeuralEngine::AcousticDNA> dnaList;
    for (int i = 0; i < 3; ++i)
        if (layers[i].isLoaded && layers[i].dna.isValid)
            dnaList.push_back(layers[i].dna);

    if (dnaList.empty())
    {
        generatedOutput.clear();
        return;
    }

    juce::Random rng;
    composedNotes = neuralEngine.fuseAndCompose(dnaList, rng.nextInt());

    renderFinalTheme();
}

void BoriyaAudioProcessor::renderFinalTheme()
{
    double sRate = (getSampleRate() > 8000.0) ? getSampleRate() : 44100.0;
    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    int stepSamples = static_cast<int>(sSec * sRate);
    int totalLen = stepSamples * 64;

    juce::AudioBuffer<float> temp(2, totalLen);
    temp.clear();

    if (composedNotes.empty())
    {
        generatedOutput.setSize(2, totalLen);
        generatedOutput.clear();
        return;
    }

    int leadIdx = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (layers[i].isLoaded && layers[i].buffer.getNumSamples() > 0)
        {
            leadIdx = i;
            break;
        }
    }

    if (leadIdx == -1) return;
    const auto& lead = layers[leadIdx];

    juce::Random rng(5678);

    for (int s = 0; s < 64; ++s)
    {
        const auto& note = composedNotes[static_cast<size_t>(s)];
        if (note.active && !lead.slices.empty())
        {
            int slice = rng.nextInt(static_cast<int>(lead.slices.size()));
            int st = lead.slices[static_cast<size_t>(slice)];
            int en = (slice + 1 < static_cast<int>(lead.slices.size())) ? lead.slices[static_cast<size_t>(slice + 1)] : lead.buffer.getNumSamples();

            double ratio = std::pow(2.0, note.semitoneOffset / 12.0);
            int writePos = s * stepSamples;
            int renderSpan = std::min(stepSamples * 3, totalLen - writePos);
            double readPos = st;

            float panAngle = (note.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float gL = std::cos(panAngle);
            float gR = std::sin(panAngle);

            for (int i = 0; i < renderSpan; ++i)
            {
                int rInt = static_cast<int>(readPos);
                if (rInt < en && rInt < lead.buffer.getNumSamples() - 1 && writePos + i < totalLen)
                {
                    float env = 1.0f;
                    if (i < 120) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * i / 120.0f));
                    else if (renderSpan - i < 600) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (renderSpan - i) / 600.0f));

                    float sL = lead.buffer.getSample(0, rInt);
                    float sR = (lead.buffer.getNumChannels() > 1) ? lead.buffer.getSample(1, rInt) : sL;
                    float mono = (sL + sR) * 0.5f * note.velocity * env * 0.85f;

                    temp.addSample(0, writePos + i, mono * gL);
                    temp.addSample(1, writePos + i, mono * gR);
                    readPos += ratio;
                }
                else break;
            }
        }
    }

    generatedOutput.setSize(2, totalLen, false, true, true);
    for (int ch = 0; ch < 2; ++ch)
        generatedOutput.copyFrom(ch, 0, temp, ch, 0, totalLen);
}

void BoriyaAudioProcessor::startVoice(int layerIdx, int sliceIdx, int pitchOffset, float pan)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];
    if (!lyr.isLoaded || sliceIdx >= static_cast<int>(lyr.slices.size())) return;

    int oldV = voiceIndex;
    int newV = 1 - voiceIndex;
    voiceIndex = newV;

    voices[oldV].active = false;

    int st = lyr.slices[static_cast<size_t>(sliceIdx)];
    int en = (sliceIdx + 1 < static_cast<int>(lyr.slices.size())) ? lyr.slices[static_cast<size_t>(sliceIdx + 1)] : lyr.buffer.getNumSamples();

    auto& v = voices[newV];
    v.layerIndex = layerIdx;
    v.currentSample = st;
    v.endSample = en;
    v.speed = std::pow(2.0, pitchOffset / 12.0);

    float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    v.panL = std::cos(panAngle);
    v.panR = std::sin(panAngle);
    v.active = true;
}

void BoriyaAudioProcessor::togglePlayback()
{
    bool next = !playState.load();
    playState.store(next);

    for (int i = 0; i < 2; ++i)
        voices[i].active = false;
    clockTicks = 0;
    currentStep = 0;
}

juce::File BoriyaAudioProcessor::getExportWav()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BORIYA_Theme.wav");
    renderFinalTheme();

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> wr(wavFmt.createWriterFor(new juce::FileOutputStream(file), 44100.0, 2, 24, {}, 0));
    if (wr != nullptr && generatedOutput.getNumSamples() > 0)
        wr->writeFromAudioSampleBuffer(generatedOutput, 0, generatedOutput.getNumSamples());

    return file;
}

juce::File BoriyaAudioProcessor::getExportMidi()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BORIYA_Groove.mid");

    juce::MidiFile mf;
    const int tpq = 960;
    mf.setTicksPerQuarterNote(tpq);

    juce::MidiMessageSequence seq;
    double usPerQtr = 60000000.0 / std::max(60, targetBpm);
    seq.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(usPerQtr)), 0.0);
    seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);

    const int tps = tpq / 4;

    for (size_t i = 0; i < composedNotes.size(); ++i)
    {
        const auto& n = composedNotes[i];
        if (n.active)
        {
            int note = 60 + n.semitoneOffset;
            double sTick = static_cast<double>(i * tps);
            double eTick = sTick + (tps * 3);

            seq.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(n.velocity * 120.0f)), sTick);
            seq.addEvent(juce::MidiMessage::noteOff(1, note), eTick);
        }
    }

    seq.addEvent(juce::MidiMessage::endOfTrack(), static_cast<double>(64 * tps));
    seq.updateMatchedPairs();
    mf.addTrack(seq);

    juce::FileOutputStream fos(file);
    if (fos.openedOk()) mf.writeTo(fos);

    return file;
}

void BoriyaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!playState.load())
    {
        buffer.clear();
        for (int i = 0; i < 2; ++i) voices[i].active = false;
        return;
    }

    buffer.clear();

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())
            {
                int hBpm = static_cast<int>(*pos->getBpm());
                if (hBpm >= 60 && hBpm <= 220)
                    targetBpm = hBpm;
            }
        }
    }

    double sRate = (getSampleRate() > 8000.0) ? getSampleRate() : 44100.0;
    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    stepLength = static_cast<int>(sSec * sRate);

    const float v1 = apvts.getRawParameterValue("VOL1")->load();
    const float v2 = apvts.getRawParameterValue("VOL2")->load();
    const float v3 = apvts.getRawParameterValue("VOL3")->load();
    const float vols[3] = { v1, v2, v3 };

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        clockTicks++;
        if (clockTicks >= stepLength)
        {
            clockTicks = 0;
            currentStep = (currentStep + 1) % 64;

            if (!composedNotes.empty() && static_cast<size_t>(currentStep) < composedNotes.size())
            {
                const auto& note = composedNotes[static_cast<size_t>(currentStep)];
                if (note.active)
                {
                    int lead = -1;
                    for (int i = 0; i < 3; ++i) if (layers[i].isLoaded) { lead = i; break; }
                    if (lead != -1 && !layers[lead].slices.empty())
                    {
                        int cut = (currentStep % static_cast<int>(layers[lead].slices.size()));
                        startVoice(lead, cut, note.semitoneOffset, note.pan);
                    }
                }
            }
        }

        for (int vIdx = 0; vIdx < 2; ++vIdx)
        {
            auto& v = voices[vIdx];
            if (v.active && v.layerIndex >= 0 && v.layerIndex < 3)
            {
                auto& lyr = layers[v.layerIndex];
                int rPos = static_cast<int>(v.currentSample);

                if (rPos < v.endSample && rPos < lyr.buffer.getNumSamples() - 1)
                {
                    float s0 = lyr.buffer.getSample(0, rPos);
                    float s1 = lyr.buffer.getSample(0, rPos + 1);
                    float frac = static_cast<float>(v.currentSample - rPos);
                    float val = (s0 + frac * (s1 - s0)) * 0.75f * vols[v.layerIndex];

                    buffer.addSample(0, s, val * v.panL);
                    buffer.addSample(1, s, val * v.panR);

                    v.currentSample += v.speed;
                }
                else
                {
                    v.active = false;
                }
            }
        }
    }

    const float warmth = apvts.getRawParameterValue("WARMTH")->load();
    const float space = apvts.getRawParameterValue("SPACE")->load();
    const float cutoff = apvts.getRawParameterValue("FILTER")->load();
    const float master = apvts.getRawParameterValue("MASTER")->load();

    if (warmth > 0.01f)
    {
        float g = 1.0f + warmth * 1.5f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::tanh(d[i] * g);
        }
    }

    filter.setCutoffFrequency(cutoff);
    juce::dsp::AudioBlock<float> blk(buffer);
    juce::dsp::ProcessContextReplacing<float> pCtx(blk);
    filter.process(pCtx);

    if (space > 0.01f)
    {
        reverbParams.roomSize = space * 0.75f;
        reverbParams.damping = 0.5f;
        reverbParams.wetLevel = space * 0.3f;
        reverbParams.dryLevel = 1.0f - (space * 0.15f);
        reverb.setParameters(reverbParams);
        reverb.process(pCtx);
    }

    buffer.applyGain(master);
}

bool BoriyaAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BoriyaAudioProcessor::createEditor()
{
    return new BoriyaAudioProcessorEditor(*this);
}

void BoriyaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BoriyaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoriyaAudioProcessor();
}
