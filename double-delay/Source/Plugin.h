#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DelayEngine.h"
class DelayProcessor final:public juce::AudioProcessor {
public:
 DelayProcessor();
 juce::AudioProcessorValueTreeState state;
 std::atomic<double> displayedBpm{120};std::atomic<bool> tempoAvailable{false},limited{false};std::atomic<bool> clearRequested{false};
 noori_delay::Settings settings()const;
 void prepareToPlay(double,int)override;void releaseResources()override{};void reset()override{clearRequested=true;}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 juce::AudioProcessorEditor* createEditor()override;bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "NOORI Double Delay";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{auto s=settings();double d=0;for(const auto& t:s.taps)d=std::max(d,std::min(noori_delay::maxSeconds,noori_delay::seconds(t,s.bpm)));if(s.feedback>=.999999)return 3600.;return std::min(3600.,d*(s.feedback>0?1+std::ceil(std::log(1e-6)/std::log(s.feedback)):1));}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 void preset(int);void setValue(const juce::String&,float);
private:
 noori_delay::Engine engine;
 std::atomic<float>* feedbackValue=nullptr;std::atomic<float>* mixValue=nullptr;std::atomic<float>* bpmValue=nullptr;
 std::array<std::array<std::atomic<float>*,6>,2> tapValues{};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
