#include "PluginProcessor.h"
#include "PluginEditor.h"

BoriyaAudioProcessorEditor::BoriyaAudioProcessorEditor(BoriyaAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 520);

    rollBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc59b27));
    rollBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff091a18));
    rollBtn.onClick = [this]() { audioProcessor.triggerAiComposition(); };
    addAndMakeVisible(rollBtn);

    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff122b27));
    playBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd4af37));
    playBtn.onClick = [this]() { audioProcessor.togglePlayback(); };
    addAndMakeVisible(playBtn);

    dragWav = std::make_unique<DirectDragLabel>("WAV", [this]() {
        return audioProcessor.getExportWav();
    });
    addAndMakeVisible(*dragWav);

    dragMidi = std::make_unique<DirectDragLabel>("MIDI", [this]() {
        return audioProcessor.getExportMidi();
    });
    addAndMakeVisible(*dragMidi);

    auto setupMasterSlider = [this](juce::Slider& s) {
        s.setLookAndFeel(&berLookAndFeel);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    };

    setupMasterSlider(warmthSlider);
    setupMasterSlider(spaceSlider);
    setupMasterSlider(filterSlider);

    warmthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "WARMTH", warmthSlider);
    spaceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "SPACE", spaceSlider);
    filterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILTER", filterSlider);

    const char* vIds[3] = { "VOL1", "VOL2", "VOL3" };
    for (int i = 0; i < 3; ++i)
    {
        auto& s = layerVol[i];
        s.setLookAndFeel(&berLookAndFeel);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);

        volAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, vIds[i], s);
    }

    startTimerHz(30);
}

BoriyaAudioProcessorEditor::~BoriyaAudioProcessorEditor()
{
    stopTimer();
    warmthSlider.setLookAndFeel(nullptr);
    spaceSlider.setLookAndFeel(nullptr);
    filterSlider.setLookAndFeel(nullptr);
    for (int i = 0; i < 3; ++i)
        layerVol[i].setLookAndFeel(nullptr);
}

bool BoriyaAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::spaceKey)
    {
        audioProcessor.togglePlayback();
        return true;
    }
    return false;
}

void BoriyaAudioProcessorEditor::drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buf, juce::Rectangle<int> bounds, juce::Colour waveCol)
{
    if (buf.getNumSamples() == 0) return;

    g.setColour(waveCol);
    juce::Path p;
    int w = bounds.getWidth();
    int h = bounds.getHeight();
    int midY = bounds.getY() + h / 2;
    int numSamples = buf.getNumSamples();
    int step = std::max(1, numSamples / w);
    const float* data = buf.getReadPointer(0);

    p.startNewSubPath(bounds.getX(), midY);
    for (int i = 0; i < w; ++i)
    {
        int sIdx = std::min(numSamples - 1, i * step);
        float val = data[sIdx];
        float y = midY - (val * (h * 0.44f));
        p.lineTo(bounds.getX() + i, y);
    }

    g.strokePath(p, juce::PathStrokeType(1.4f));
}

void BoriyaAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGradient(juce::Colour(0xff061614), 0, 0,
                                    juce::Colour(0xff0a2420), 0, (float) getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(10.0f, 10.0f, getWidth() - 20.0f, getHeight() - 20.0f, 12.0f, 1.8f);

    // Header
    g.setColour(juce::Colour(0xffe8c35a));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("BORIYA", 35, 18, 200, 32, juce::Justification::left);

    g.setColour(juce::Colour(0xff86ab9f));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("OG RJ HOOD", 35, 46, 200, 18, juce::Justification::left);

    // Center Ber Glow
    float centerX = 445.0f;
    float centerY = 240.0f;
    glowPhase += 0.04f;
    float glow = 0.7f + 0.3f * std::sin(glowPhase);

    g.setColour(juce::Colour(0xffd4af37).withAlpha(0.2f * glow));
    for (int ring = 1; ring <= 3; ++ring)
        g.drawEllipse(centerX - ring * 20, centerY - ring * 20, ring * 40, ring * 40, 1.0f);

    g.setColour(juce::Colour(0xffe8c35a).withAlpha(glow));
    g.fillEllipse(centerX - 7, centerY - 7, 14, 14);

    // 3 Drop Layers
    for (int i = 0; i < 3; ++i)
    {
        int yPos = 78 + i * 78;
        juce::Rectangle<int> box(35, yPos, 330, 68);
        auto& lyr = audioProcessor.layers[i];

        g.setColour(juce::Colour(0xff091a18));
        g.fillRoundedRectangle(box.toFloat(), 6.0f);

        if (lyr.isLoaded && lyr.buffer.getNumSamples() > 0)
        {
            drawWaveform(g, lyr.buffer, box.reduced(6), juce::Colour(0xffd4af37));
        }
        else
        {
            g.setColour(juce::Colours::grey.withAlpha(0.5f));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("DROP HERE", box, juce::Justification::centred);
        }

        g.setColour(lyr.isLoaded ? juce::Colour(0xffd4af37) : juce::Colour(0xff1e423b));
        g.drawRoundedRectangle(box.toFloat(), 6.0f, 1.2f);
    }

    // Output Waveform Box
    int outY = 78 + 3 * 78 + 4;
    juce::Rectangle<int> outBox(35, outY, 395, 78);
    g.setColour(juce::Colour(0xff071513));
    g.fillRoundedRectangle(outBox.toFloat(), 6.0f);

    if (audioProcessor.generatedOutput.getNumSamples() > 0)
    {
        drawWaveform(g, audioProcessor.generatedOutput, outBox.reduced(6), juce::Colour(0xffffe082));
    }
    else
    {
        g.setColour(juce::Colour(0xffd4af37).withAlpha(0.35f));
        g.setFont(juce::Font(12.0f, juce::Font::plain));
        g.drawText("FINAL GENERATED THEME", outBox, juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(outBox.toFloat(), 6.0f, 1.5f);

    // Master Knobs Labels
    g.setColour(juce::Colour(0xffd4af37));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("WARMTH", 535, 290, 65, 14, juce::Justification::centred);
    g.drawText("SPACE", 615, 290, 65, 14, juce::Justification::centred);
    g.drawText("CUTOFF", 695, 290, 65, 14, juce::Justification::centred);
}

void BoriyaAudioProcessorEditor::resized()
{
    rollBtn.setBounds(535, 45, 225, 44);
    playBtn.setBounds(535, 96, 225, 38);

    dragWav->setBounds(535, 142, 108, 36);
    dragMidi->setBounds(652, 142, 108, 36);

    for (int i = 0; i < 3; ++i)
    {
        int yPos = 78 + i * 78 + 9;
        layerVol[i].setBounds(372, yPos, 50, 50);
    }

    warmthSlider.setBounds(535, 310, 65, 65);
    spaceSlider.setBounds(615, 310, 65, 65);
    filterSlider.setBounds(695, 310, 65, 65);
}

void BoriyaAudioProcessorEditor::timerCallback()
{
    playBtn.setButtonText(audioProcessor.isPlaying() ? "STOP" : "PLAY");
    repaint();
}

void BoriyaAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int y)
{
    if (files.isEmpty()) return;

    int layer = -1;
    if (y >= 78 && y < 156)       layer = 0;
    else if (y >= 156 && y < 234) layer = 1;
    else if (y >= 234 && y < 312) layer = 2;

    if (layer != -1)
    {
        juce::File f(files[0]);
        if (f.existsAsFile())
            audioProcessor.loadLoopIntoLayer(layer, f);
    }
}
