#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include <atomic>
#include <mutex>
#include <memory>
#include <optional>
using AttackRange=std::optional<std::pair<noori::Sample,noori::Sample>>;
struct ProjectReferenceRegion {
 std::shared_ptr<const noori::Audio> audio;
 double sourceStart=0,sourceDuration=0,playbackStart=0;
};
struct ProjectTrackRegion {
 juce::String id,name;
 std::shared_ptr<class EditSession> session;
 double sourceStart=0,sourceDuration=0,playbackStart=0;
};
struct ProjectTrack {juce::String id,name,persistentID;std::vector<ProjectTrackRegion> regions;bool editable=true;};
struct Revision { noori::Attacks attacks; std::shared_ptr<const noori::Audio> audio; bool operator==(const Revision&)const=default; };
struct SessionState {
 bool operator==(const SessionState&)const=default;
 std::shared_ptr<const juce::MemoryBlock> logoPNG;
 Revision render;
 std::shared_ptr<const noori::Audio> original,referenceAudio;
 noori::Settings settings;
 noori::Timeline timeline;
 noori::Sample origin=0;
 juce::String name,referenceName,referenceProjectID;
 std::vector<noori::Sample> referenceClicks;
 bool useReference=false,exactBpm=true,referenceIsClick=true,compare=false;
 bool lightTheme=true,levelView=false,overlay=false,audition=false;
 double zoom=1,viewStart=0,manualBpm=80;
 int preset=0;
};
class EditJournal;

class EditSession final : private juce::Timer, public std::enable_shared_from_this<EditSession> {
public:
 EditSession(); ~EditSession() override;
 noori::Settings settings;
 std::uint64_t changeSerial=0;
 std::shared_ptr<const juce::MemoryBlock> logoPNG;
 std::shared_ptr<EditJournal> journal;
 bool lightTheme=true,levelView=false,overlay=false,audition=false;
 double viewZoom=1,viewStart=0,manualBpm=80;int preset=0;
 std::shared_ptr<const noori::Audio> referenceAudio;
 SessionState state()const;
 void apply(const SessionState&);
 void change(juce::String,std::function<void()>,bool merge=false);
 void addAttack(noori::Sample);void removeSelectedAttacks();
 juce::String undoDescription()const;juce::String redoDescription()const;
 juce::String name="No audio loaded", status="Import a vocal or capture the insert input.";
 noori::Sample origin=0;
 noori::Timeline timeline;
 std::shared_ptr<const noori::Audio> original;
 noori::Attacks attacks;
 std::vector<noori::Sample> referenceClicks;
 juce::String referenceName,referenceProjectID;bool referenceIsClick=true;
 bool useReference=false,exactBpm=true;
 void loadProjectReference(std::vector<ProjectReferenceRegion>,juce::String,bool clickTrack=true,bool alignWhenReady=false,bool selectedOnly=false,AttackRange range={},juce::String projectID={});
 std::shared_ptr<const noori::Audio> snapshot()const;
 void selectAll(bool selected=true);void selectRange(noori::Sample,noori::Sample,bool additive=false,bool sourceCoordinates=false);
 void anchorTo(noori::Sample);
 void loadReference(const juce::File&,noori::Sample projectStart);
 std::atomic<const noori::Audio*> audible{nullptr},originalAudible{nullptr};
 std::atomic<noori::Sample> realtimeOrigin{0};
 std::atomic<bool> compare{false}, busy{false};
 std::atomic<float> progress{0};
 std::function<void()> onChanged;
 void load(std::function<noori::Audio()> reader, juce::String nameIn, noori::Sample start, noori::Timeline map);
 void analyse(); void align(bool selectedOnly=false,AttackRange range={}); void move(std::size_t,noori::Sample); void reset();
 void undo(); void redo(); bool canUndo()const; bool canRedo()const;
 void cancel(); void settle();
 const noori::Audio* displayed()const {return compare.load()?original.get():audible.load();}
 juce::MemoryBlock archive(bool includeAudio) const;
 void restore(const void*,std::size_t,bool includeAudio);
 void setSelected(std::size_t index,bool additive);
private:
 juce::ThreadPool pool{1};
 struct Result {std::shared_ptr<const noori::Audio> source;Revision revision;juce::String error,message; bool newSource=false,referenceOnly=false,alignWhenReady=false,selectedOnly=false; std::vector<noori::Sample> clicks;std::shared_ptr<const noori::Audio> referenceAudio;juce::String sourceName;noori::Sample sourceOrigin=0;noori::Timeline sourceTimeline;juce::String referenceTitle,projectID;bool clickTrack=true;AttackRange range;};
 std::mutex pendingMutex;
 std::unique_ptr<Result> pending;
 std::atomic<bool> cancelled{false};
 std::shared_ptr<const noori::Audio> rendered;
 std::vector<Revision> retired;
 std::optional<SessionState> pendingBefore;
 std::atomic<std::size_t> retainedBytes{0};
 std::string restoreMarkers;
 void submit(std::function<Result()>);
 void renderMarkers(noori::Attacks);
 void timerCallback() override;
 void publish(Revision);
 void record(const SessionState&,juce::String,bool merge=false);
};
class EditJournal {
public:
 struct Entry {std::weak_ptr<EditSession> session;SessionState before,after;juce::String label;std::uint64_t group=0;};
 std::weak_ptr<EditSession> restoredSession;
 void beginGroup(bool async=false){activeGroup=++nextGroup;asyncGroup=async;participants.clear();}void endGroup(){activeGroup=0;asyncGroup=false;participants.clear();}
 void track(std::shared_ptr<EditSession> s){if(asyncGroup)participants.push_back(s);}
 bool hasPending()const {for(const auto& w:participants)if(auto s=w.lock())if(s->busy)return true;return false;}
 void finishIfIdle(){if(asyncGroup&&!participants.empty()&&!hasPending())endGroup();}
 void record(std::shared_ptr<EditSession>,SessionState,SessionState,juce::String,bool merge=false);
 bool available(bool redo)const;
 void step(bool redo);
 juce::String description(bool redo)const;
private:
 mutable std::vector<Entry> entries;mutable std::size_t cursor=0;double lastEdit=0;std::uint64_t activeGroup=0,nextGroup=0;bool asyncGroup=false;std::vector<std::weak_ptr<EditSession>> participants;
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
 std::atomic<unsigned> transportSequence{0};
 struct Transport {double bpm,quarter;noori::Sample sample;bool valid;};
 Transport transport()const;
 std::atomic<int> numerator{4},denominator{4};std::atomic<bool> playing{false},hostTempoValid{false};
 std::atomic<bool> audition{false},captureArmed{false};
 void beginCapture();void finishCapture();
 void importFile(const juce::File&);void exportFile(const juce::File&,std::shared_ptr<EditSession>);
 juce::String captureStatus()const;
 void updateTrackProperties(const TrackProperties&)override;
 std::vector<ProjectTrack> connectedReferenceTracks()const;
private:
 std::uint64_t registryID=0;
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
