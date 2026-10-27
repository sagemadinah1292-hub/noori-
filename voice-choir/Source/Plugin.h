#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChoirEngine.h"
class ChoirProcessor final:public juce::AudioProcessor {
public:
 ChoirProcessor();juce::AudioProcessorValueTreeState state;
 void prepareToPlay(double,int)override;void releaseResources()override{};void reset()override{clearRequested=true;}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 juce::AudioProcessorEditor* createEditor()override;bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "NOORI Voice Choir";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return .15;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 void setValue(const juce::String&,float);void preset(int);noori_choir::Settings settings()const;
private:
 noori_choir::Engine engine;std::atomic<bool> clearRequested{false};std::array<std::atomic<float>*,6> values{};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
