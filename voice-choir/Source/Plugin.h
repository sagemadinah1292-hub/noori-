#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChoirEngine.h"
#include "OfflineFiles.h"
#include <thread>
class ChoirProcessor final:public juce::AudioProcessor {
public:
 ChoirProcessor();~ChoirProcessor()override;juce::AudioProcessorValueTreeState state;
 void prepareToPlay(double,int)override;void releaseResources()override{};void reset()override{clearRequested=true;}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 juce::AudioProcessorEditor* createEditor()override;bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "NOORI CHOIR";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return .15;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 void setValue(const juce::String&,float);void preset(int);noori_choir::Settings settings()const;
 void importWav(juce::File);void exportTo(juce::File);
 void cancelJob(){cancelled=true;}
 bool isBusy()const{return busy.load();}
 float jobProgress()const{return progress.load();}
 juce::String jobStatus()const;juce::String sourceDescription()const;
 bool hasSource()const;juce::StringArray exportedFiles()const;
 std::atomic<bool> darkMode{true};std::atomic<int> uiScale{1};
private:
 std::thread worker;std::atomic<bool> busy{false},cancelled{false};std::atomic<float> progress{0};
 mutable juce::CriticalSection offlineLock;std::shared_ptr<const noori_choir::VocalFile> vocal;
 juce::String status="Import a vocal WAV to begin.";juce::StringArray exports;
 void runJob(std::function<void()>);
 noori_choir::Engine engine;std::atomic<bool> clearRequested{false};std::array<std::atomic<float>*,6> values{};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
