#include "StrangeEchoesProcessor.h"
#include "StrangeEchoesEditor.h"

//==============================================================================
StrangeEchoesAudioProcessor::StrangeEchoesAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    apvts.state = juce::ValueTree("savedParams");
}

StrangeEchoesAudioProcessor::~StrangeEchoesAudioProcessor()
{
}

//==============================================================================
const juce::String StrangeEchoesAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool StrangeEchoesAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool StrangeEchoesAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool StrangeEchoesAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double StrangeEchoesAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int StrangeEchoesAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int StrangeEchoesAudioProcessor::getCurrentProgram()
{
    return 0;
}

void StrangeEchoesAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String StrangeEchoesAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void StrangeEchoesAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void StrangeEchoesAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    auto effectSettings = getEffectSettings(apvts, 120.0);
    
    // Prepare delay lines
    wetSignal.setSize(2, samplesPerBlock);
    wetSignal.clear(0, 0, samplesPerBlock);
    wetSignal.clear(1, 0, samplesPerBlock);
        
    auto delayBufferSize = 2.5 * sampleRate;
    delayBuffer.setSize(2, (int)delayBufferSize);
    delayBuffer.clear(0, 0, (int)delayBufferSize);
    delayBuffer.clear(1, 0, (int)delayBufferSize);
    
    delayTimeMsSmooth.reset(sampleRate / samplesPerBlock, 0.5f);
    
    // Prepare filter chains
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = 1;
    
    filterChainL.prepare(spec);
    filterChainR.prepare(spec);
  
    updateFilterChains(effectSettings.lowPassFreq, effectSettings.highPassFreq, sampleRate);
    
    // Prepare LFO
    lfoPhase = 0.0f;
    
    
    // prepare pitch shifter
    // TODO: consider changing pitch shift preset based on block size (weird noises when buffer size < 128)
    tmpPitchShiftInput.setSize(2, samplesPerBlock);
    tmpPitchShiftOutput.setSize(2, samplesPerBlock);
    
    if (samplesPerBlock <= 128)
        pitchShifter.configure(2, sampleRate * 0.05, sampleRate * 0.02); //even cheaper
    else
        pitchShifter.presetCheaper(2, sampleRate);
    
    pitchShifter.setTransposeSemitones(effectSettings.pitchShift);
    
    freqShifter.prepare(sampleRate, samplesPerBlock);
}

void StrangeEchoesAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool StrangeEchoesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void StrangeEchoesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
        
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // extract BPM from DAW (needs fix, works in Ableton but crashes when running in standalone mode)
    float currentBpm { 120 };
    if (auto bpmFromHost = *getPlayHead()->getPosition()->getBpm())
            currentBpm = bpmFromHost;
    
    auto effectSettings = getEffectSettings(apvts, currentBpm);
    delayTimeMsSmooth.setTargetValue(effectSettings.delayTimeMs);
    
    float LFOsample = std::sin(lfoPhase * 2 * juce::MathConstants<float>::pi);
    
    lfoPhase += static_cast<float>(bufferSize / getSampleRate() * effectSettings.lfoRate);
    if (lfoPhase > 1)
        lfoPhase -= 1.0f;
    
    float delayTimeMs = juce::jlimit(minDelayTimeMs, maxDelayTimeMs, delayTimeMsSmooth.getNextValue() + LFOsample * effectSettings.lfoAmount);
    int delayTimeInSamples = static_cast<int>(delayTimeMs / 1000.0 * getSampleRate());
    
    int newReadPos = writePos - delayTimeInSamples;
    newReadPos %= delayBufferSize;
    if (newReadPos < 0)
        newReadPos += delayBufferSize;
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // Writes input from buffer -> delayBuffer
        writeToDelayBuffer(buffer, channel, channel, 1.0f, 1.0f, true);
        
        wetSignal.clear(channel, 0, bufferSize);
        
        // Reads from past values in delayBuffer -> wetSignal
        if (newReadPos==readPos)
            readFromDelayBuffer(wetSignal, channel, channel, newReadPos, 1.0f, 1.0f, true);
        else
        {
            readFromDelayBuffer(wetSignal, channel, channel, readPos, 1.0f, 0.0f, true);
            readFromDelayBuffer(wetSignal, channel, channel, newReadPos, 0.0f, 1.0f, false);
        }
    }
    
    // Update LP/HP filter parameters
    updateFilterChains(effectSettings.lowPassFreq, effectSettings.highPassFreq, getSampleRate());
    
    // Process wet signals with LP/HP filter chains
    juce::dsp::AudioBlock<float> block(wetSignal);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    filterChainL.process(leftContext);
    filterChainR.process(rightContext);
    
    // pitch shifter
    float pitchShiftAmount = effectSettings.pitchShiftAmount;
    
    pitchShifter.setTransposeSemitones(effectSettings.pitchShift);
    pitchShifter.process(wetSignal.getArrayOfReadPointers(), tmpPitchShiftInput.getNumSamples(), tmpPitchShiftOutput.getArrayOfWritePointers(), tmpPitchShiftOutput.getNumSamples());
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        wetSignal.applyGain(channel, 0, bufferSize, 1.f - pitchShiftAmount);
        wetSignal.addFromWithRamp(channel, 0, tmpPitchShiftOutput.getReadPointer(channel), tmpPitchShiftOutput.getNumSamples(), prevPitchShiftAmount, pitchShiftAmount);
    }
    
    prevPitchShiftAmount = pitchShiftAmount;
    
    // frequency shifter
    freqShifter.configure(effectSettings.freqShift, effectSettings.sideBandMix);
    freqShifter.process(wetSignal.getArrayOfWritePointers(), bufferSize);
    
    float dryWetMix = effectSettings.drywet;
    float feedback = effectSettings.feedback;
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        // Writes feedback from wetSignal -> delayBuffer
        writeToDelayBuffer(wetSignal, channel, channel, prevFeedback, feedback, false);
                
        // Scales input signal in buffer and adds wetSignal
        buffer.applyGain(channel, 0, bufferSize, 1.f - dryWetMix);
        buffer.addFromWithRamp(channel, 0, wetSignal.getReadPointer(channel), bufferSize, dryWetMix, dryWetMix);
    }
    
    prevFeedback = feedback;
    
    readPos = newReadPos + bufferSize;
    readPos %= delayBufferSize;
    
    writePos += bufferSize;
    writePos %= delayBufferSize;
}

void StrangeEchoesAudioProcessor::updateFilterChains(float lowPassFreq, float highPassFreq, double sampleRate)
{    
    auto highPassCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(highPassFreq, sampleRate, 4);
    auto lowPassCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(lowPassFreq, sampleRate, 4);
    
    auto& leftHighPass = filterChainL.get<0>();
    *leftHighPass.get<0>().coefficients = *highPassCoefficients[0];
    *leftHighPass.get<1>().coefficients = *highPassCoefficients[0];
    
    auto& rightHighPass = filterChainR.get<0>();
    *rightHighPass.get<0>().coefficients = *highPassCoefficients[0];
    *rightHighPass.get<1>().coefficients = *highPassCoefficients[0];
    
    auto& leftLowPass = filterChainL.get<1>();
    *leftLowPass.get<0>().coefficients = *lowPassCoefficients[0];
    *leftLowPass.get<1>().coefficients = *lowPassCoefficients[0];
    
    auto& rightLowPass = filterChainR.get<1>();
    *rightLowPass.get<0>().coefficients = *lowPassCoefficients[0];
    *rightLowPass.get<1>().coefficients = *lowPassCoefficients[0];
}

void FrequencyShifter::prepare(int sampleRate, int blockSize)
{
    this->tmpBufferI.setSize(1, blockSize);
    this->tmpBufferI.clear(0, 0, blockSize);
    this->tmpBufferQ.setSize(1, blockSize);
    this->tmpBufferQ.clear(0, 0, blockSize);
    
    this->tmpBufferOscI.setSize(1, blockSize);
    this->tmpBufferOscI.clear(0, 0, blockSize);
    this->tmpBufferOscQ.setSize(1, blockSize);
    this->tmpBufferOscQ.clear(0, 0, blockSize);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(blockSize);
    spec.numChannels = 1;
    
    this->firFilterL.reset();
    this->firFilterL.prepare(spec);
    this->firFilterL.coefficients = coeffs;
    
    this->firFilterR.reset();
    this->firFilterR.prepare(spec);
    this->firFilterR.coefficients = coeffs;
    
    this->firDelay.setSize(2, firDelayInSamples);
    this->firDelay.clear();
    this->firDelayWritePosition = 0;
    
    this->oscI.prepare(spec);
    this->oscI.initialise([](float x) { return std::sin(x); }, 128);
    this->oscI.setFrequency(0.0f);
    this->oscI.reset();
    
    this->oscQ.prepare(spec);
    this->oscQ.initialise([](float x) { return std::cos(x); }, 128);
    this->oscQ.setFrequency(0.0f);
    this->oscQ.reset();
}

void FrequencyShifter::configure(float freq, float sidbandMix)
{
    this->oscI.setFrequency(freq);
    this->oscQ.setFrequency(freq);
    this->sideBandMix = sidbandMix;
}

void FrequencyShifter::process(float*const* bufferData, int bufferSize)
{
    
    processOscillator(&this->tmpBufferOscI, &this->oscI);
    processOscillator(&this->tmpBufferOscQ, &this->oscQ);
    
    for (int channel = 0; channel < 2; ++channel)
    {
        float* bufferChannelData = bufferData[channel];
        float* tmpIData = this->tmpBufferI.getWritePointer(0);
        float* tmpQData = this->tmpBufferQ.getWritePointer(0);
        float* oscIData = this->tmpBufferOscI.getWritePointer(0);
        float* oscQData = this->tmpBufferOscQ.getWritePointer(0);
        float* firDelayData = this->firDelay.getWritePointer(channel);
        
        for (int i = 0; i < bufferSize; i++)
        {
            // Write to delay line
            firDelayData[this->firDelayWritePosition] = bufferChannelData[i];
    
            // Read from delay line
            int firDelayReadPosition = (this->firDelayWritePosition - this->firDelayInSamples) % this->firDelayInSamples;
            if (firDelayReadPosition < 0)
            {
               firDelayReadPosition += this->firDelayInSamples;
            }
    
            tmpIData[i] = firDelayData[firDelayReadPosition];
    
            this->firDelayWritePosition = (this->firDelayWritePosition + 1) % this->firDelayInSamples;
        }
        
        tmpBufferQ.copyFrom(0, 0, bufferChannelData, bufferSize);
        juce::dsp::AudioBlock<float> block(tmpBufferQ);
        juce::dsp::ProcessContextReplacing<float> context(block);
    
        if (channel==0)
            this->firFilterL.process(context);
        else
            this->firFilterR.process(context);
    
        for (int i = 0; i < bufferSize; i++)
        {
            float posSide = tmpIData[i] * oscIData[i] - tmpQData[i] * oscQData[i];
            float negSide = tmpIData[i] * oscIData[i] + tmpQData[i] * oscQData[i];
            
            bufferChannelData[i] = this->sideBandMix * posSide + (1.f - this->sideBandMix) * negSide;
        }
    }
}

void FrequencyShifter::processOscillator(juce::AudioBuffer<float>* bufPtr,juce::dsp::Oscillator<float>* oscPtr)
{
    bufPtr->clear();
    juce::dsp::AudioBlock<float> block(*bufPtr);
    juce::dsp::ProcessContextReplacing<float> context(block);
    oscPtr->process(context);
}

void StrangeEchoesAudioProcessor::writeToDelayBuffer(juce::AudioBuffer<float>& buffer,
                                                   int channelIn, int channelOut,
                                                   float startGain, float endGain,
                                                   bool replacing)
{
    if (delayBuffer.getNumSamples() >= writePos + buffer.getNumSamples())
    {
        if (replacing)
            delayBuffer.copyFrom(channelOut, writePos, buffer.getReadPointer (channelIn), buffer.getNumSamples());
        else
            delayBuffer.addFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), buffer.getNumSamples(), startGain, endGain);
    }
    else
    {
        auto numSamplesToEnd = delayBuffer.getNumSamples() - writePos;
        auto numLeftoverSamples = buffer.getNumSamples() - numSamplesToEnd;
        
        if (replacing)
        {
            delayBuffer.copyFrom(channelOut, writePos, buffer.getReadPointer (channelIn), numSamplesToEnd);
            delayBuffer.copyFrom(channelOut, 0, buffer.getReadPointer (channelIn, numSamplesToEnd), numLeftoverSamples);
        }
        else
        {
            delayBuffer.addFromWithRamp(channelOut, writePos, buffer.getReadPointer(channelIn), numSamplesToEnd, startGain, endGain);
            delayBuffer.addFromWithRamp(channelOut, 0, buffer.getReadPointer(channelIn, numSamplesToEnd), numLeftoverSamples, startGain, endGain);
        }
    }
}

void StrangeEchoesAudioProcessor::readFromDelayBuffer(juce::AudioBuffer<float>& buffer,
                                                    int channelIn, int channelOut,
                                                    int rPos,
                                                    float startGain, float endGain,
                                                    bool replacing)
{
    if (delayBuffer.getNumSamples() > rPos + buffer.getNumSamples())
    {
        if (replacing)
        {
            buffer.copyFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, rPos), buffer.getNumSamples(), startGain, endGain);
        }
        else
        {
            buffer.addFromWithRamp(channelOut, 0, delayBuffer.getReadPointer(channelIn, rPos),      buffer.getNumSamples(), startGain,   endGain);
        }
    }
    else
    {
        auto numSamplesToEnd = delayBuffer.getNumSamples() - rPos;
        auto numLeftoverSamples = buffer.getNumSamples() - numSamplesToEnd;
        auto midGain = juce::jmap(float (numSamplesToEnd) / buffer.getNumSamples(), startGain, endGain);
                
        if (replacing)
        {
            buffer.copyFromWithRamp(channelOut, 0,               delayBuffer.getReadPointer(channelIn, rPos), numSamplesToEnd, startGain, midGain);
            buffer.copyFromWithRamp(channelOut, numSamplesToEnd, delayBuffer.getReadPointer(channelIn, 0   ), numLeftoverSamples, midGain, endGain);
        }
        else
        {
            buffer.addFromWithRamp(channelOut, 0,               delayBuffer.getReadPointer(channelIn, rPos), numSamplesToEnd,   startGain,   midGain);
            buffer.addFromWithRamp(channelOut, numSamplesToEnd, delayBuffer.getReadPointer(channelIn, 0   ), numLeftoverSamples, midGain,           endGain);
        }
    }
}

//==============================================================================
bool StrangeEchoesAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* StrangeEchoesAudioProcessor::createEditor()
{
    return new StrangeEchoesAudioProcessorEditor (*this);
}

//==============================================================================
void StrangeEchoesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    std::unique_ptr <juce::XmlElement> xml (apvts.state.createXml());
    copyXmlToBinary(*xml, destData);
    
    //juce::ignoreUnused (destData);
}

void StrangeEchoesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    std::unique_ptr<juce::XmlElement> storedParams (getXmlFromBinary(data, sizeInBytes));
    
    if (storedParams != nullptr)
    {
        if (storedParams -> hasTagName(apvts.state.getType()))
        {
            apvts.state = juce::ValueTree::fromXml(*storedParams);
        }
    }
    
    //juce::ignoreUnused (data, sizeInBytes);
}

EffectSettings getEffectSettings(juce::AudioProcessorValueTreeState& apvts, float bpm)
{
    EffectSettings settings;
    
    settings.syncOption = static_cast<int>(apvts.getRawParameterValue("Sync Options")->load());
    
    if (settings.syncOption == 0)
        settings.delayTimeMs =  apvts.getRawParameterValue("Delay Time")->load();
    else
    {
        float baseDelayTime = 2000.0 * 120.0 / bpm;
        float typeMult = 1.0;
        
        int noteOption = static_cast<int>(apvts.getRawParameterValue("Tempo-Relative Delay Time")->load());
        int noteType = static_cast<int>(apvts.getRawParameterValue("Note Type")->load());
        
        switch (noteType)
        {
            case 1: // triplets
                typeMult = 2.0 / 3.0;
                break;
            case 2: // dotted
                typeMult = 3.0 / 2.0;
        }
        
        switch (noteOption)
        {
            case 0: // 1/1 notes
                settings.delayTimeMs = typeMult * baseDelayTime;
                break;
            case 1: // 1/2 notes
                settings.delayTimeMs = typeMult * baseDelayTime / 2;
                break;
            case 2: // 1/4 notes
                settings.delayTimeMs = typeMult * baseDelayTime / 4;
                break;
            case 3: // 1/8 notes
                settings.delayTimeMs = typeMult * baseDelayTime / 8;
                break;
            case 4: // 1/16 notes
                settings.delayTimeMs = typeMult * baseDelayTime / 16;
        }
    }
    
    settings.feedback =     apvts.getRawParameterValue("Feedback")->load();
    settings.drywet =       apvts.getRawParameterValue("Dry/Wet Mix")->load();
    settings.lfoRate =      apvts.getRawParameterValue("LFO Rate")->load();
    settings.lfoAmount =    apvts.getRawParameterValue("LFO Amount")->load();
    settings.freqShift =    apvts.getRawParameterValue("Frequency Shift")->load();
    settings.sideBandMix =  apvts.getRawParameterValue("Sideband Mix")->load();
    settings.pitchShift =   apvts.getRawParameterValue("Pitch Shift")->load();
    settings.pitchShiftAmount =   apvts.getRawParameterValue("Pitch Shift Amount")->load();
    settings.lowPassFreq =  apvts.getRawParameterValue("LowPass Freq")->load();
    settings.highPassFreq = apvts.getRawParameterValue("HighPass Freq")->load();
    
    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout StrangeEchoesAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Delay Time",
                                                           "Delay Time",
                                                           juce::NormalisableRange<float>(1.0f, 2500.0f, 1.f, 1.f),
                                                           300.0f));
    
    juce::StringArray strSyncOptions;
    strSyncOptions.add("Time");
    strSyncOptions.add("Sync");
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("Sync Options", "Sync Options", strSyncOptions, 0));
    
    juce::StringArray strNoteOptions;
    strNoteOptions.add("1/1");
    strNoteOptions.add("1/2");
    strNoteOptions.add("1/4");
    strNoteOptions.add("1/8");
    strNoteOptions.add("1/16");
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("Tempo-Relative Delay Time", "Tempo-Relative Delay Time", strNoteOptions, 2));
    
    juce::StringArray strNoteType;
    strNoteType.add("Notes");
    strNoteType.add("Triplets");
    strNoteType.add("Dotted");

    layout.add(std::make_unique<juce::AudioParameterChoice>("Note Type", "Note Type", strNoteType, 0));

        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Dry/Wet Mix",
                                                           "Dry/Wet",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f, 1.f),
                                                           0.5f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Feedback",
                                                           "Feedback",
                                                           juce::NormalisableRange<float>(0.0f, 0.99f, 0.01f, 1.f),
                                                           0.1f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Frequency Shift",
                                                           "Frequency Shift",
                                                           juce::NormalisableRange<float>(0.0f, 1000.f, 0.1f, 1.f),
                                                           0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Sideband Mix",
                                                           "Sideband Mix",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f, 1.f),
                                                           0.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Pitch Shift",
                                                           "Pitch Shift",
                                                           juce::NormalisableRange<float>(-12.f, 12.f, 1.f, 1.f),
                                                           0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Pitch Shift Amount",
                                                           "Pitch Shift Amount",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f, 1.f),
                                                           0.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowPass Freq",
                                                           "Low Pass Filter",
                                                           juce::NormalisableRange<float>(20.0f, 22000.f, 0.1f, 1.f / std::log2(1.f + std::sqrt(22000.f / 20.0f))),
                                                           22000.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighPass Freq",
                                                           "High Pass Filter",
                                                           juce::NormalisableRange<float>(20.0f, 22000.f, 0.1f, 1.f / std::log2(1.f + std::sqrt(22000.f / 20.0f))),
                                                           20.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LFO Amount",
                                                           "LFO Amount",
                                                           juce::NormalisableRange<float>(-100.0f, 100.0f, 1.f, 1.f),
                                                           0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LFO Rate",
                                                           "LFO Rate",
                                                           juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f, 1.f),
                                                           0.0f));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StrangeEchoesAudioProcessor();
}
