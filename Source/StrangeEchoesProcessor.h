#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

struct EffectSettings
{
    float   delayTimeMs{0.0},
            feedback{0.0},
            drywet{0.0},
            lfoRate{0.0},
            lfoAmount{0.0},
            freqShift{0.0},
            lowPassFreq{0.0},
            highPassFreq{0.0};
    
    int    syncOption{0};
};

EffectSettings getEffectSettings(juce::AudioProcessorValueTreeState& apvts, float bpm);

//==============================================================================
class StrangeEchoesAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    StrangeEchoesAudioProcessor();
    ~StrangeEchoesAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
        
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrangeEchoesAudioProcessor)
    
    // Stereo delay
    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> wetSignal;
    
    int writePos{0};
    int readPos{0};
    const float minDelayTimeMs = 1.0;
    const float maxDelayTimeMs = 2500.0;
    float prevFeedback = 0.0;
    
    juce::SmoothedValue<float> delayTimeMsSmooth;
    
    // LP/HP filter chain
    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter,Filter>;
    using MonoFilterChain = juce::dsp::ProcessorChain<CutFilter,CutFilter>;
    
    MonoFilterChain filterChainL, filterChainR;
    
    // LFO
    float lfoPhase;
    
    // Frequency shifter
    juce::dsp::Oscillator<float> oscI;
    juce::dsp::Oscillator<float> oscQ;
    float oscFreqHz;
    
    juce::AudioBuffer<float> tmpBufferI;
    juce::AudioBuffer<float> tmpBufferQ;
    juce::AudioBuffer<float> tmpBufferOscI;
    juce::AudioBuffer<float> tmpBufferOscQ;
    
    size_t filterSize = 301;
    
    juce::Array<float> firCoeffArray = {0.000000, -0.000000, 0.000000, -0.000004, 0.000000, -0.000012, 0.000000, -0.000024, 0.000000, -0.000040, 0.000000, -0.000060, 0.000000, -0.000085, 0.000000, -0.000115, 0.000000, -0.000149, 0.000000, -0.000189, 0.000000, -0.000233, 0.000000, -0.000283, 0.000000, -0.000339, 0.000000, -0.000400, 0.000000, -0.000467, 0.000000, -0.000541, 0.000000, -0.000620, 0.000000, -0.000706, 0.000000, -0.000799, 0.000000, -0.000899, 0.000000, -0.001006, 0.000000, -0.001120, 0.000000, -0.001242, 0.000000, -0.001372, 0.000000, -0.001510, 0.000000, -0.001656, 0.000000, -0.001812, 0.000000, -0.001976, 0.000000, -0.002150, 0.000000, -0.002334, 0.000000, -0.002528, 0.000000, -0.002733, 0.000000, -0.002950, 0.000000, -0.003178, 0.000000, -0.003419, 0.000000, -0.003672, 0.000000, -0.003940, 0.000000, -0.004222, 0.000000, -0.004520, 0.000000, -0.004834, 0.000000, -0.005166, 0.000000, -0.005516, 0.000000, -0.005887, 0.000000, -0.006279, 0.000000, -0.006695, 0.000000, -0.007137, 0.000000, -0.007606, 0.000000, -0.008106, 0.000000, -0.008640, 0.000000, -0.009210, 0.000000, -0.009822, 0.000000, -0.010480, 0.000000, -0.011189, 0.000000, -0.011957, 0.000000, -0.012792, 0.000000, -0.013703, 0.000000, -0.014702, 0.000000, -0.015804, 0.000000, -0.017028, 0.000000, -0.018395, 0.000000, -0.019936, 0.000000, -0.021689, 0.000000, -0.023703, 0.000000, -0.026047, 0.000000, -0.028814, 0.000000, -0.032137, 0.000000, -0.036213, 0.000000, -0.041340, 0.000000, -0.048005, 0.000000, -0.057045, 0.000000, -0.070042, 0.000000, -0.090390, 0.000000, -0.126905, 0.000000, -0.211924, 0.000000, -0.636464, 0.000000, 0.636602, 0.000000, 0.212062, 0.000000, 0.127043, 0.000000, 0.090528, 0.000000, 0.070180, 0.000000, 0.057182, 0.000000, 0.048142, 0.000000, 0.041477, 0.000000, 0.036349, 0.000000, 0.032273, 0.000000, 0.028948, 0.000000, 0.026181, 0.000000, 0.023836, 0.000000, 0.021820, 0.000000, 0.020067, 0.000000, 0.018524, 0.000000, 0.017156, 0.000000, 0.015931, 0.000000, 0.014827, 0.000000, 0.013827, 0.000000, 0.012914, 0.000000, 0.012078, 0.000000, 0.011309, 0.000000, 0.010597, 0.000000, 0.009938, 0.000000, 0.009324, 0.000000, 0.008752, 0.000000, 0.008217, 0.000000, 0.007715, 0.000000, 0.007243, 0.000000, 0.006800, 0.000000, 0.006381, 0.000000, 0.005987, 0.000000, 0.005614, 0.000000, 0.005261, 0.000000, 0.004927, 0.000000, 0.004611, 0.000000, 0.004311, 0.000000, 0.004026, 0.000000, 0.003756, 0.000000, 0.003500, 0.000000, 0.003257, 0.000000, 0.003026, 0.000000, 0.002807, 0.000000, 0.002600, 0.000000, 0.002403, 0.000000, 0.002217, 0.000000, 0.002040, 0.000000, 0.001873, 0.000000, 0.001715, 0.000000, 0.001566, 0.000000, 0.001426, 0.000000, 0.001293, 0.000000, 0.001169, 0.000000, 0.001052, 0.000000, 0.000943, 0.000000, 0.000841, 0.000000, 0.000745, 0.000000, 0.000657, 0.000000, 0.000575, 0.000000, 0.000499, 0.000000, 0.000430, 0.000000, 0.000366, 0.000000, 0.000308, 0.000000, 0.000256, 0.000000, 0.000209, 0.000000, 0.000167, 0.000000, 0.000130, 0.000000, 0.000099, 0.000000, 0.000071, 0.000000, 0.000049, 0.000000, 0.000031, 0.000000, 0.000017, 0.000000, 0.000008, 0.000000, 0.000002, 0.000000};

    juce::dsp::FIR::Filter<float> firFilterL;
    juce::dsp::FIR::Filter<float> firFilterR;
    juce::dsp::FIR::Coefficients<float>::Ptr coeffs = new juce::dsp::FIR::Coefficients<float> (firCoeffArray.getRawDataPointer(), filterSize);
    
    int firDelayWritePosition;
    juce::AudioBuffer<float> firDelay;
    int firDelayInSamples = 151;
    
    
    void writeToDelayBuffer(juce::AudioBuffer<float>& buffer,
                            int channelIn, int channelOut,
                            float startGain, float endGain,
                            bool replacing);
    
    void readFromDelayBuffer(juce::AudioBuffer<float>& buffer,
                             int channelIn, int channelOut,
                             int readPos,
                             float startGain, float endGain,
                             bool replacing);
    
    void updateFilterChains(float lowPassFreq, float highPassFreq, double sampleRate);
    
    void processOscillator(juce::AudioBuffer<float>* bufPtr,
                           juce::dsp::Oscillator<float>* oscPtr);
    
    void frequencyShifter(int channel, int bufferSize);
};
