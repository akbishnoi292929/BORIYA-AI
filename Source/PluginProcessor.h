#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "NeuralEngine.h"

struct LayerTrack
{
    juce::AudioSampleBuffer buffer;
    std::vector<int> slices;
    NeuralEngine::AcousticDNA dna;
    bool isLoaded = false;
};

struct SynthVoice
{
    int layerIndex = -1;
    double currentSample = 0.0;
    int endSample = 0;
    double speed = 1.0;
    float panL = 1.0f;
    float panR = 1.0f;
    bool active = false;
};

class BoriyaAudioProcessor : public juce::AudioProcessor
{
public:
    BoriyaAudioProcessor();
    ~BoriyaAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadLoopIntoLayer(int layerIdx, const juce::File& file);
    void triggerAiComposition();
    void renderFinalTheme();
    void togglePlayback();
    bool isPlaying() const { return playState.load(); }

    juce::File getExportWav();
    juce::File getExportMidi();

    LayerTrack layers[3];
    juce::AudioSampleBuffer generatedOutput;
    std::vector<NeuralEngine::MelodicNote> composedNotes;

    int targetBpm = 130;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void makeSlices(int layerIdx);
    void startVoice(int layerIdx, int sliceIdx, int pitchOffset, float pan);

    NeuralEngine neuralEngine;
    SynthVoice voices[2];
    int voiceIndex = 0;

    std::atomic<bool> playState { false };
    int currentStep = 0;
    int clockTicks = 0;
    int stepLength = 11025;

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::StateVariableTPTFilter<float> filter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessor)
};
