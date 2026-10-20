#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include <atomic>
#include <mutex>
#include <memory>
struct Revision { noori::Attacks attacks; std::shared_ptr<const noori::Audio> audio; };
class EditSession final : private juce::Timer, public std::enable_shared_from_this<EditSession> {
public:
 EditSession(); ~EditSession() override;
 noori::Settings settings;
 juce::String name="No audio loaded", status="Import a vocal or capture the insert input.";
 noori::Sample origin=0;
 noori::Timeline timeline;
 std::shared_ptr<const noori::Audio> original;
 noori::Attacks attacks;
 std::atomic<const noori::Audio*> audible{nullptr},originalAudible{nullptr};
 std::atomic<noori::Sample> realtimeOrigin{0};
 std::atomic<bool> compare{false}, busy{false};
 std::atomic<float> progress{0};
 std::function<void()> onChanged;
 void load(std::function<noori::Audio()> reader, juce::String nameIn, noori::Sample start, noori::Timeline map);
 void analyse(); void align(bool selectedOnly=false); void move(std::size_t,noori::Sample); void reset();
 void undo(); void redo(); bool canUndo()const; bool canRedo()const;
 void cancel(); void settle();
 const noori::Audio* displayed()const {return compare.load()?original.get():audible.load();}
 juce::MemoryBlock archive(bool includeAudio) const;
 void restore(const void*,std::size_t,bool includeAudio);
 void setSelected(std::size_t index,bool additive);
private:
 juce::ThreadPool pool{1};
 struct Result {std::shared_ptr<const noori::Audio> source;Revision revision;juce::String error; bool newSource=false;};
 std::mutex pendingMutex;
 std::unique_ptr<Result> pending;
 std::atomic<bool> cancelled{false};
 std::vector<Revision> history,retired;
 int current=-1;
 std::size_t retainedBytes=0;
 std::string restoreMarkers;
 void submit(std::function<Result()>);
 void renderMarkers(noori::Attacks);
 void timerCallback() override;
 void publish(Revision);
};
class NooriProcessor final : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
 , public juce::AudioProcessorARAExtension
#endif
{
public:
 NooriProcessor();~NooriProcessor()override;
 void prepareToPlay(double,int)override;void releaseResources()override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 const juce::String getName()const override{return "NOORI TURAB TEMPO";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}double getTailLengthSeconds()const override{return 0;}
 bool hasEditor()const override{return true;}juce::AudioProcessorEditor* createEditor()override;
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "Default";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 std::shared_ptr<EditSession> insertSession;
 std::atomic<double> bpm{80},quarter{0};std::atomic<std::int64_t> hostSample{0};
 std::atomic<int> numerator{4},denominator{4};std::atomic<bool> playing{false},hostTempoValid{false};
 std::atomic<bool> audition{false},captureArmed{false};
 void beginCapture();void finishCapture();
 void importFile(const juce::File&);void exportFile(const juce::File&,std::shared_ptr<EditSession>);
 juce::String captureStatus()const;
private:
 noori::Audio capture;
 std::atomic<noori::Sample> captureCount{0};
 noori::Sample captureOrigin=0,expectedSample=0;
 std::array<noori::TempoPoint,16384> capturedTempo{};std::size_t tempoCount=0;
 std::atomic<bool> captureStopped{false};
 double deviceRate=48000;
 const noori::Audio* activeBuffer=nullptr;const noori::Audio* transitionBuffer=nullptr;
 noori::Sample bufferOrigin=0,transitionOrigin=0;int crossfadeRemaining=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NooriProcessor)
};
