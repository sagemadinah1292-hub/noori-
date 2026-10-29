#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "WorkflowFiles.h"
#include "Licensing.h"
#include <thread>
class ChoirProcessor final:public juce::AudioProcessor,private juce::AudioProcessorValueTreeState::Listener {
public:
 explicit ChoirProcessor(noori_licensing::Config licensing=noori_licensing::buildConfig());~ChoirProcessor()override;
 juce::AudioProcessorValueTreeState state;
 noori_licensing::Manager licensing;
 void prepareToPlay(double,int)override;void releaseResources()override{};void reset()override{stop();}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 bool isBusesLayoutSupported(const BusesLayout&)const override;
 juce::AudioProcessorEditor* createEditor()override;bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "NOORI CHOIR";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return "NOORI Naat";}void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;
 void setValue(const juce::String&,float);float value(const char*)const;
 noori_choir::ChoirConfig config()const;noori_choir::FxConfig fxConfig()const;
 void importWav(juce::File);void generate(int regenerate=-1);void applyFx();void prepareStems();
 void exportTo(juce::File,int selected=-1);void clearSource();void removeStem(int);
 void cancelJob(){cancelled=true;}bool isBusy()const{return busy.load();}float jobProgress()const{return progress.load();}
 juce::String jobStatus()const;bool hasSource()const;int stemCount()const;
 std::shared_ptr<const noori_choir::ChoirSession> session()const;
 juce::StringArray exportedFiles()const;juce::StringArray dragFiles()const;
 void play(int voice=-1);void stop();bool isPlaying()const{return playbackMode.load()!=-3;}
 double playhead()const{return previewFrame.load();}int playingVoice()const{return playbackMode.load();}
 void setStemMix(int,noori_choir::StemMix);noori_choir::StemMix stemMix(int)const;
 bool stemsNeedPreparation()const{return dragDirty.load()||preparedMixRevision.load()!=mixRevision.load();}void invalidateStems(){++mixRevision;dragDirty=true;}
 void startCapture();void stopCapture();bool isCapturing()const{return capturing.load();}
 bool capturePending()const{return captureReady.load();}double capturedSeconds()const{return capturedFrames.load()/hostRate.load();}
 int revision()const{return dataRevision.load();}
 unsigned long long audioBlocks()const{return audioBlockCount.load(std::memory_order_relaxed);}
 float previewLevelDb()const{return juce::Decibels::gainToDecibels(previewPeak.load(),-100.f);}
 std::atomic<bool> sourceMuted{false};
 std::atomic<bool> darkMode{true};std::atomic<int> uiScale{0};
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 std::thread worker;std::atomic<bool> busy{false},cancelled{false};std::atomic<float> progress{0};
 mutable juce::CriticalSection dataLock;std::shared_ptr<const noori_choir::ChoirSession> current;
 juce::String status="Drop a vocal or browse files to begin.";juce::StringArray exports,dropFiles;
 void parameterChanged(const juce::String&,float)override{invalidateStems();}
 std::atomic<unsigned long long> mixRevision{0},preparedMixRevision{~0ULL};
 std::atomic<int> dataRevision{0};std::atomic<bool> dragDirty{true};
 mutable juce::CriticalSection previewLock;std::shared_ptr<const noori_choir::ChoirSession> preview;
 juce::Reverb previewReverb;juce::SmoothedValue<float> cutSmooth,brightSmooth,wetSmooth;
 std::array<double,2> fxX{},fxY{},fxLow{};
 void processPreviewFx(juce::AudioBuffer<float>&);
 std::atomic<unsigned long long> audioBlockCount{0};std::atomic<float> previewPeak{0};
 std::atomic<int> playbackMode{-3};std::atomic<bool> rewind{false};double cursor=0;
 std::atomic<double> previewFrame{0},hostRate{48000};
 struct AtomicMix {std::atomic<float> gain{0},pan{0};std::atomic<bool> mute{false},solo{false};};
 std::array<AtomicMix,16> mixes;std::atomic<float>* master=nullptr;
 juce::CriticalSection captureLock;juce::AudioBuffer<float> capture;std::atomic<bool> capturing{false},captureReady{false};
 std::atomic<int> capturedFrames{0};int captureChannels=1;juce::int64 captureOffset=0;
 std::function<void()> pendingJob;std::atomic<bool> shuttingDown{false};
 bool requireLicense();
 void runJob(std::function<void()>,bool replace=false);void publish(std::shared_ptr<noori_choir::ChoirSession>,bool resetMix=false,bool preservePlayback=false);
 void cacheSession(noori_choir::ChoirSession&);void restoreCache(juce::String,std::vector<noori_choir::StemMix>);
 void prepareDrop(const noori_choir::ChoirSession&);std::vector<noori_choir::StemMix> mixSnapshot(int)const;
};
