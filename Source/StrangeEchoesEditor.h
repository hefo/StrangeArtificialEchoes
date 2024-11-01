#pragma once

#include "StrangeEchoesProcessor.h"


struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&,
                                    int x, int y, int width, int height,
                                    float sliderPosProportional,
                                    float rotaryStartAngle,
                                    float rotaryEndAngle,
                                    juce::Slider&) override;
};

struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix):
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,juce::Slider::TextEntryBoxPosition::NoTextBox),
    param(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }
    
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const {return 14;}
    juce::String getDisplayString() const;
    juce::String getTitleString() const;
    
    private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;
    
};


//==============================================================================
class StrangeEchoesAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit StrangeEchoesAudioProcessorEditor (StrangeEchoesAudioProcessor&);
    ~StrangeEchoesAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:    
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    
    StrangeEchoesAudioProcessor& processorRef;
    
    RotarySliderWithLabels dryWetSlider,
    delayTimeSlider,
    feedbackSlider,
    freqShiftSlider,
    sidebandSlider,
    pitchShiftSlider,
    pitchShiftAmountSlider,
    lowPassSlider,
    highPassSlider,
    lfoAmountSlider,
    lfoRateSlider;
    
    juce::Slider syncSlider, noteSelector, noteTypeSlider;

    Attachment dryWetSliderAttachment,
    delayTimeSliderAttachment,
    feedbackSliderAttachment,
    freqShiftSliderAttachment,
    sidebandSliderAttachment,
    pitchShiftSliderAttachment,
    pitchShiftAmountSliderAttachment,
    lowPassSliderAttachment,
    highPassSliderAttachment,
    lfoAmountSliderAttachment,
    lfoRateSliderAttachment,
    syncSliderAttachment,
    noteSelectorAttachment,
    noteTypeSliderAttachment;
    
    juce::LookAndFeel_V4 syncLookAndFeel;
    
    std::vector<juce::Component*> getComps();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrangeEchoesAudioProcessorEditor)
};
