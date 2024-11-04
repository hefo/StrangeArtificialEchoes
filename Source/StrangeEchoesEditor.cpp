#include "StrangeEchoesProcessor.h"
#include "StrangeEchoesEditor.h"


void LookAndFeel::drawRotarySlider(juce::Graphics & g,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   float sliderPosProportional,
                                   float rotaryStartAngle,
                                   float rotaryEndAngle,
                                   juce::Slider & slider)
{
    using namespace juce;
    
    auto bounds = Rectangle<float>(x, y, width, height);
    g.setColour(Colour(197u, 124u, 49u));
    
    
    g.fillEllipse(bounds);
    
    g.setColour(Colour(0u,0u,0u));
    g.drawEllipse(bounds, 1.f);
    
    if (auto* rslw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        
        Path p;
        
        Rectangle<float> r;
        r.setLeft(center.getX()-1.5);
        r.setRight(center.getX()+1.5);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rslw->getTextHeight() * 2.0);
        
        p.addRoundedRectangle(r, 1.f);
         
        jassert(rotaryStartAngle<rotaryEndAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        
        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));
        g.fillPath(p);
        
        // draw parameter value string in middle
        g.setFont(rslw->getTextHeight());
        auto text = rslw->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);
        r.setSize(strWidth + 4, rslw->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());
        
        //g.setColour(Colour(41u,83u,77u));
        g.setColour(Colours::black.withAlpha(0.0f));
        g.fillRoundedRectangle(r, 2.f);
        //g.fillRect(r);
        g.setColour(Colours::black);
        g.drawFittedText(text, r.toNearestInt(),juce::Justification::centred,1);
        
        // draw parameter title string at top
        auto titleStr = rslw->getTitleString();
        strWidth = g.getCurrentFont().getStringWidth(titleStr);
        r.setSize(strWidth + 4, rslw->getTextHeight() + 2);
        r.setCentre(bounds.getCentreX(), bounds.getCentreY() - bounds.getHeight() / 2 - 8);
        g.setColour(Colours::black.withAlpha(0.0f));
        g.fillRect(r);
        g.setColour(Colours::whitesmoke);
        g.drawFittedText(titleStr, r.toNearestInt(),juce::Justification::centred,1);
    }
    
}


void RotarySliderWithLabels::paint(juce::Graphics & g)
{
    using namespace juce;
    
    auto startAng = degreesToRadians(180.0f + 45.0f);
    auto endAng = degreesToRadians(180.0f - 45.0f) + MathConstants<float>::twoPi;
    auto range = getRange();
    
    auto sliderBounds = getSliderBounds();
    
//    g.setColour(Colours::red);
//    g.drawRect(getLocalBounds());
//    g.setColour(Colours::yellow);
//    g.drawRect(sliderBounds);
    
    getLookAndFeel().drawRotarySlider(g,
                                      sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAng,
                                      endAng,
                                      *this);
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    
    size -= getTextHeight() * 1.2;
    
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(15);
    
    return r;
    
}

juce::String RotarySliderWithLabels::getTitleString() const
{
    juce::String str;
    
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
        str = floatParam->name;
    
    return str;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();
    
    juce::String str;
    bool addK = false;
    
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();
        
        
        if (suffix == "Hz" && val > 999.f)
        {
            val /= 1000.f;
            addK = true;
        }
        
        if (suffix == "%")
        {
            val *= 100.f;
            str = juce::String(val, 0);
        }
        else
        {
            str = juce::String(val,(addK ? 2 : 0));
        }
    }
    else
    {
        jassertfalse;
    }
    
    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";

        str << suffix;
    }
        
    return str;
}

//==============================================================================
StrangeEchoesAudioProcessorEditor::StrangeEchoesAudioProcessorEditor (StrangeEchoesAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
dryWetSlider            (*processorRef.apvts.getParameter("Dry/Wet Mix"),       "%"),
delayTimeSlider         (*processorRef.apvts.getParameter("Delay Time"),        "ms"),
feedbackSlider          (*processorRef.apvts.getParameter("Feedback"),          "%"),
freqShiftSlider         (*processorRef.apvts.getParameter("Frequency Shift"),   "Hz"),
sidebandSlider          (*processorRef.apvts.getParameter("Sideband Mix"),      "%"),
pitchShiftSlider        (*processorRef.apvts.getParameter("Pitch Shift"),       "st"),
pitchShiftAmountSlider  (*processorRef.apvts.getParameter("Pitch Shift Amount"),"%"),
//lowPassSlider           (*processorRef.apvts.getParameter("LowPass Freq"),      "Hz"),
//highPassSlider          (*processorRef.apvts.getParameter("HighPass Freq"),     "Hz"),
lfoAmountSlider         (*processorRef.apvts.getParameter("LFO Amount"),        "ms"),
lfoRateSlider           (*processorRef.apvts.getParameter("LFO Rate"),          "Hz"),

dryWetSliderAttachment          (processorRef.apvts, "Dry/Wet Mix",                 dryWetSlider),
delayTimeSliderAttachment       (processorRef.apvts, "Delay Time",                  delayTimeSlider),
feedbackSliderAttachment        (processorRef.apvts, "Feedback",                    feedbackSlider),
freqShiftSliderAttachment       (processorRef.apvts, "Frequency Shift",             freqShiftSlider),
sidebandSliderAttachment        (processorRef.apvts, "Sideband Mix",                sidebandSlider),
pitchShiftSliderAttachment      (processorRef.apvts, "Pitch Shift",                 pitchShiftSlider),
pitchShiftAmountSliderAttachment(processorRef.apvts, "Pitch Shift Amount",          pitchShiftAmountSlider),
lowPassSliderAttachment         (processorRef.apvts, "LowPass Freq",                lowPassSlider),
highPassSliderAttachment        (processorRef.apvts, "HighPass Freq",               highPassSlider),
lfoAmountSliderAttachment       (processorRef.apvts, "LFO Amount",                  lfoAmountSlider),
lfoRateSliderAttachment         (processorRef.apvts, "LFO Rate",                    lfoRateSlider),
syncSliderAttachment            (processorRef.apvts, "Sync Options",                syncSlider),
noteTypeSliderAttachment        (processorRef.apvts, "Note Type",                   noteTypeSlider),
noteSelectorAttachment          (processorRef.apvts, "Tempo-Relative Delay Time",   noteSelector)
{
    juce::ignoreUnused (processorRef);

    setSize (500, 400);

    syncLookAndFeel.setColour (juce::Slider::thumbColourId, juce::Colour(197u, 124u, 49u));
    syncLookAndFeel.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::black.withAlpha(0.0f));
    syncLookAndFeel.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
    
    syncSlider.setLookAndFeel(&syncLookAndFeel);
    noteSelector.setLookAndFeel(&syncLookAndFeel);
    noteTypeSlider.setLookAndFeel(&syncLookAndFeel);
    lowPassSlider.setLookAndFeel(&syncLookAndFeel);
    highPassSlider.setLookAndFeel(&syncLookAndFeel);
    
    lowPassSlider.setTextValueSuffix(" Hz");
    lowPassSlider.setSize(lowPassSlider.getWidth()*0.75, lowPassSlider.getHeight());
    
    auto lpbound = lowPassSlider.getBounds();
    
    lowPassSlider.setCentrePosition(lpbound.getCentreX()+50, lpbound.getCentreY());
    lowPassLabel.setText("LP: ", juce::dontSendNotification);
    lowPassLabel.attachToComponent (&lowPassSlider, true); // [4]
    
    highPassSlider.setTextValueSuffix(" Hz");
    highPassSlider.setSize(highPassSlider.getWidth()*0.75, highPassSlider.getHeight());
    
    auto hpbound = highPassSlider.getBounds();
    highPassSlider.setCentrePosition(hpbound.getCentreX()+50, hpbound.getCentreY());
    highPassLabel.setText("HP: ", juce::dontSendNotification);
    highPassLabel.attachToComponent (&highPassSlider, true); // [4]
    
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }
}

StrangeEchoesAudioProcessorEditor::~StrangeEchoesAudioProcessorEditor()
{
}

//==============================================================================
void StrangeEchoesAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    using namespace juce;
    
    g.fillAll(juce::Colour(41u,83u,77u));
    juce::Rectangle<int> r;

    auto bounds = getLocalBounds();
    auto topArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    g.setColour(juce::Colour(62u, 87u, 82u));
    r.setSize(topArea.getWidth(), topArea.getHeight());
    r.setCentre(topArea.getCentreX(), topArea.getCentreY());
    g.fillRect(r);
    
    auto eqArea = bounds.removeFromTop(bounds.getHeight()*0.33);

    g.setColour(juce::Colour(97u, 117u, 113u));
    r.setSize(eqArea.getWidth(), eqArea.getHeight());
    r.setCentre(eqArea.getCentreX(), eqArea.getCentreY());
    g.fillRect(r);
    
    auto lfoArea = bounds.removeFromTop(bounds.getHeight()*0.5);

    g.setColour(juce::Colour(62u, 87u, 82u));
    r.setSize(lfoArea.getWidth(), lfoArea.getHeight());
    r.setCentre(lfoArea.getCentreX(), lfoArea.getCentreY());
    g.fillRect(r);
    
    g.setColour(juce::Colour(97u, 117u, 113u));
    r.setSize(bounds.getWidth(), bounds.getHeight());
    r.setCentre(bounds.getCentreX(), bounds.getCentreY());
    g.fillRect(r);
    
    // Colour on Time area
    bounds = getLocalBounds();
    g.setColour(juce::Colour(43u,59u,56u));
    r.setSize(bounds.getWidth()*0.33, bounds.getHeight()*0.5511);
    r.setCentre(bounds.getCentreX() - bounds.getWidth()*0.67*0.5, bounds.getCentreY() - bounds.getHeight()*0.4489*0.5);
    g.fillRect(r);
    
    auto cr = Rectangle<float>(bounds.getCentreX(), bounds.getCentreY(), bounds.getWidth()*0.1, bounds.getWidth()*0.1);
    
    for (int i = 0; i < 24; i++)
    {
        float radi = 0.1 + 0.05*i;
        cr.setSize( bounds.getWidth() * radi, bounds.getWidth() * radi);
        cr.setCentre(bounds.getCentreX(), bounds.getCentreY());

        g.setColour(juce::Colour(69u, 77u, 76u));
        g.drawEllipse(cr, 1.f);
    }
    
    g.setFont (15.0f);
}

void StrangeEchoesAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    auto bounds = getLocalBounds();
    
    // top
    auto topArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto delayTimeArea = topArea.removeFromLeft(topArea.getWidth() * 0.33);
    auto dryWetArea = topArea.removeFromRight(topArea.getWidth() * 0.5);
    
    delayTimeSlider.setBounds(delayTimeArea);
    dryWetSlider.setBounds(dryWetArea);
    feedbackSlider.setBounds(topArea);
    
    // second
    auto midArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto syncArea = midArea.removeFromLeft(midArea.getWidth() * 0.33);
    //syncArea = syncArea.removeFromLeft(syncArea.getWidth() * 0.66);
    
    syncSlider.setBounds(syncArea.removeFromTop(syncArea.getHeight() * 0.33));
    noteSelector.setBounds(syncArea.removeFromTop(syncArea.getHeight() * 0.5));
    noteTypeSlider.setBounds(syncArea);
    
    lowPassLabel.setBounds(midArea);
    highPassLabel.setBounds(midArea);
    lowPassSlider.setBounds(midArea.removeFromTop(midArea.getHeight() * 0.5));
    highPassSlider.setBounds(midArea);
    
    
    // third
    auto lfoArea = bounds.removeFromTop(bounds.getHeight() * 0.5);
        
//    int margin = lfoArea.getWidth()*0.0438581;
//    lfoArea.removeFromLeft(margin);
//    lfoArea.removeFromRight(margin);
    
    lfoAmountSlider.setBounds(lfoArea.removeFromLeft(lfoArea.getWidth() * 0.5));
    lfoRateSlider.setBounds(lfoArea);
    
    //bottom
    auto pitchShiftArea = bounds.removeFromLeft(bounds.getWidth() * 0.5);
    
    pitchShiftAmountSlider.setBounds(pitchShiftArea.removeFromLeft(pitchShiftArea.getWidth() * 0.5));
    pitchShiftSlider.setBounds(pitchShiftArea);
    freqShiftSlider.setBounds(bounds.removeFromLeft(bounds.getWidth() * 0.5));
    sidebandSlider.setBounds(bounds);
}

std::vector<juce::Component*> StrangeEchoesAudioProcessorEditor::getComps()
{
    return
    {
        &dryWetSlider,
        &delayTimeSlider,
        &feedbackSlider,
        &freqShiftSlider,
        &sidebandSlider,
        &pitchShiftSlider,
        &pitchShiftAmountSlider,
        &lowPassSlider,
        &highPassSlider,
        &lfoAmountSlider,
        &lfoRateSlider,
        &syncSlider,
        &noteSelector,
        &noteTypeSlider,
    };
}
