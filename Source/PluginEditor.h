#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class BoriyaBerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        g.setColour(juce::Colour(0xff091f1b));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(juce::Colour(0xffd4af37));
        g.strokePath(arc, juce::PathStrokeType(2.2f));

        auto berRadius = radius * 0.70f;
        juce::ColourGradient berGrad(juce::Colour(0xfff3d078), centre.x - berRadius * 0.4f, centre.y - berRadius * 0.4f,
                                     juce::Colour(0xff8c6218), centre.x + berRadius, centre.y + berRadius, false);
        g.setGradientFill(berGrad);
        g.fillEllipse(centre.x - berRadius, centre.y - berRadius, berRadius * 2.0f, berRadius * 2.0f);

        juce::Path p;
        p.addRectangle(-1.2f, -berRadius, 2.4f, berRadius * 0.85f);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centre.x, centre.y));
        g.setColour(juce::Colour(0xff122b27));
        g.fillPath(p);
    }
};

class DirectDragLabel : public juce::Label
{
public:
    DirectDragLabel(const juce::String& text, std::function<juce::File()> fileGetter)
        : juce::Label({}, text), getFile(fileGetter)
    {
        setJustificationType(juce::Justification::centred);
        setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a3630));
        setColour(juce::Label::textColourId, juce::Colour(0xffe8c35a));
        setColour(juce::Label::outlineColourId, juce::Colour(0xffc59b27));
    }

    void mouseDrag(const juce::MouseEvent&) override
    {
        if (getFile != nullptr)
        {
            auto file = getFile();
            if (file.existsAsFile())
            {
                juce::DragAndDropContainer* ddc = juce::DragAndDropContainer::findParentDragContainerFor(this);
                if (ddc != nullptr && !ddc->isDragAndDropActive())
                    ddc->performExternalDragDropOfFiles({ file.getFullPathName() }, false);
            }
        }
    }

private:
    std::function<juce::File()> getFile;
};

class BoriyaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
                                   public juce::Timer,
                                   public juce::FileDragAndDropTarget,
                                   public juce::KeyListener
{
public:
    explicit BoriyaAudioProcessorEditor(BoriyaAudioProcessor&);
    ~BoriyaAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    void drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buffer, juce::Rectangle<int> bounds, juce::Colour waveCol);

    BoriyaAudioProcessor& audioProcessor;
    BoriyaBerLookAndFeel berLookAndFeel;

    juce::TextButton rollBtn { "ROLL" };
    juce::TextButton playBtn { "PLAY" };
    std::unique_ptr<DirectDragLabel> dragWav;
    std::unique_ptr<DirectDragLabel> dragMidi;

    juce::Slider warmthSlider;
    juce::Slider spaceSlider;
    juce::Slider filterSlider;
    juce::Slider layerVol[3];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttach[3];

    float glowPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessorEditor)
};
