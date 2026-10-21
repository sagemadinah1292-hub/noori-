#include "ARAIntegration.h"
#if JucePlugin_Enable_ARA
#include <ARA_Library/Dispatch/ARAContentReader.h>
#include <cmath>
#include <map>
using namespace juce;
NooriModification::NooriModification(ARAAudioSource* source,ARA::ARAAudioModificationHostRef ref,const ARAAudioModification* clone):ARAAudioModification(source,ref,clone),session(std::make_shared<EditSession>()) {
 if(clone){const auto* m=static_cast<const NooriModification*>(clone);auto state=m->session->archive(false);session->restore(state.getData(),state.getSize(),false);}
 session->onChanged=[this]{notifyContentChanged(ARAContentUpdateScopes::samplesAreAffected(),true);for(auto* r:getPlaybackRegions())r->notifyContentChanged(ARAContentUpdateScopes::samplesAreAffected(),true);};
}
NooriModification::~NooriModification(){session->onChanged={};session->cancel();}
static void applyRegionTimeline(NooriModification* m,ARAPlaybackRegion* r) {
  if(m->session->busy)return;
  double rate=m->getAudioSource()->getSampleRate();auto oldOrigin=m->session->origin;m->session->origin=(noori::Sample)std::llround((r->getStartInPlaybackTime()-r->getStartInAudioModificationTime())*rate);for(auto& click:m->session->referenceClicks)click+=oldOrigin-m->session->origin;
  auto* sequence=r->getRegionSequence();
  if(auto* context=sequence?sequence->getMusicalContext():nullptr) {
   ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> reader(context);
   if(reader&&reader.getEventCount()>=2) {
    std::vector<noori::TempoPoint> points;
    for(int i=0;i+1<reader.getEventCount();++i){auto a=reader.getDataForEvent(i),b=reader.getDataForEvent(i+1);double bpm=(b.quarterPosition-a.quarterPosition)*60/(b.timePosition-a.timePosition);auto sample=(noori::Sample)std::llround(a.timePosition*rate);double quarter=a.quarterPosition;if(!points.empty()){const auto& prev=points.back();quarter=prev.quarter+(sample-prev.sample)*prev.bpm/(60*rate);}points.push_back({sample,bpm,quarter});}
    try {m->session->timeline=noori::Timeline(rate,points);}catch(...){m->session->status="Unsupported host tempo content";}
   }
  }
 }
std::shared_ptr<EditSession> selectedARASession(NooriProcessor& p) {
 if(auto* view=p.getEditorView()) {auto regions=view->getViewSelection().getEffectivePlaybackRegions<ARAPlaybackRegion>();if(!regions.empty())return regions.front()->getAudioModification<NooriModification>()->session;}
 return {};
}
static String regionID(ARAPlaybackRegion* region){return String::toHexString((juce::int64)(std::intptr_t)region);}
std::vector<ProjectTrack> availableProjectTracks(NooriProcessor& p){
 std::vector<ProjectTrack> tracks;auto* view=p.getEditorView();if(!view)return tracks;
 auto* document=view->getDocumentController()->getDocument<ARADocument>();
 for(auto* sequence:document->getRegionSequences<ARARegionSequence>()){
  ProjectTrack track;track.id=String::toHexString((juce::int64)(std::intptr_t)sequence);track.name=String::fromUTF8(sequence->getName());if(track.name.isEmpty())track.name="Unnamed project track";
  for(auto* r:sequence->getPlaybackRegions<ARAPlaybackRegion>()){
   auto* m=r->getAudioModification<NooriModification>();auto title=String::fromUTF8(r->getName());if(title.isEmpty())title=String::fromUTF8(m->getAudioSource()->getName());
   if(std::abs(r->getDurationInPlaybackTime()-r->getDurationInAudioModificationTime())>1./m->getAudioSource()->getSampleRate())continue;
   track.regions.push_back({regionID(r),title,m->session,r->getStartInAudioModificationTime(),r->getDurationInAudioModificationTime(),r->getStartInPlaybackTime()});
  }
  if(!track.regions.empty()){std::vector<std::string> ids;for(auto* r:sequence->getPlaybackRegions<ARAPlaybackRegion>())ids.push_back(r->getAudioModification<NooriModification>()->getPersistentID());std::sort(ids.begin(),ids.end());track.persistentID=track.name+"|";for(const auto& id:ids)track.persistentID+=String::fromUTF8(id.c_str())+"|";tracks.push_back(std::move(track));}
 }
 return tracks;
}
bool configureProjectRegion(NooriProcessor& p,const String& id){auto* view=p.getEditorView();if(!view)return false;for(auto* sequence:view->getDocumentController()->getDocument<ARADocument>()->getRegionSequences<ARARegionSequence>())for(auto* r:sequence->getPlaybackRegions<ARAPlaybackRegion>())if(regionID(r)==id){applyRegionTimeline(r->getAudioModification<NooriModification>(),r);return true;}return false;}
class NooriController;
class NooriRenderer final : public ARAPlaybackRenderer {
public:
 NooriRenderer(ARA::PlugIn::DocumentController* dc,ReadWriteLock& l):ARAPlaybackRenderer(dc),lock(l),readerThread("Noori ARA read-ahead"){readerThread.startThread();}
 void prepareToPlay(double sr,int block,int channels,AudioProcessor::ProcessingPrecision,AlwaysNonRealtime)override {
  rate=sr;temp.setSize(channels,block);readers.clear();transitions.clear();
  for(auto* r:getPlaybackRegions())transitions.emplace(r,Transition{});
  for(auto* r:getPlaybackRegions()){auto* source=r->getAudioModification()->getAudioSource();if(readers.count(source))continue;auto reader=std::make_unique<ARAAudioSourceReader>(source);auto buffered=std::make_unique<BufferingAudioReader>(reader.release(),readerThread,(int)(source->getSampleRate()*2));buffered->setReadTimeout(0);readers.emplace(source,std::move(buffered));}
 }
 void releaseResources()override{readers.clear();transitions.clear();temp.setSize(0,0);}
 bool processBlock(AudioBuffer<float>& b,AudioProcessor::Realtime realtime,const AudioPlayHead::PositionInfo& pos)noexcept override {
  b.clear();ScopedTryReadLock guard(lock);if(!guard.isLocked()||!pos.getIsPlaying())return true;
  auto blockStart=pos.getTimeInSamples().orFallback(0);
  for(auto* r:getPlaybackRegions()) {
   auto* mod=r->getAudioModification<NooriModification>();auto* source=mod->getAudioSource();
   // Host-applied region time scaling is intentionally not advertised by this factory.
   if(std::abs(r->getDurationInPlaybackTime()-r->getDurationInAudioModificationTime())>1./rate)continue;
   auto first=(int)jlimit<juce::int64>(0,b.getNumSamples(),r->getStartInPlaybackSamples(rate)-blockStart);
   auto last=(int)jlimit<juce::int64>(0,b.getNumSamples(),r->getStartInPlaybackSamples(rate)+r->getDurationInPlaybackSamples(rate)-blockStart);
   if(last<=first)continue;
   auto* audio=mod->session->compare.load()?mod->session->originalAudible.load():mod->session->audible.load(std::memory_order_acquire);
   const double sourceRate=source->getSampleRate();
   const double offset=r->getStartInAudioModificationTime()-r->getStartInPlaybackTime();
   if(audio) {
    auto stateIt=transitions.find(r);Transition* state=stateIt==transitions.end()?nullptr:&stateIt->second;
    if(state&&state->current!=audio){state->previous=state->current;state->current=audio;state->remaining=state->previous?256:0;}
    auto sample=[&](const noori::Audio* a,int c,double x){auto at=(noori::Sample)std::floor(x);double frac=x-at;const auto& samples=a->channels[(std::size_t)std::min(c,(int)a->channels.size()-1)];if(at>=0&&at+1<a->length())return float(samples[(std::size_t)at]*(1-frac)+samples[(std::size_t)(at+1)]*frac);return at==a->length()-1?samples.back():0.f;};
    for(int c=0;c<b.getNumChannels();++c) {
     auto* dst=b.getWritePointer(c);
     for(int i=first;i<last;++i){double x=((blockStart+i)/rate+offset)*sourceRate;float value=sample(audio,c,x);if(state&&i-first<state->remaining){double mix=.5-.5*std::cos(3.141592653589793*(256-state->remaining+i-first+1)/256);value=float(sample(state->previous,c,x)*(1-mix)+value*mix);}dst[i]+=value;}
    }
    if(state)state->remaining=std::max(0,state->remaining-(last-first));
   } else if(std::abs(sourceRate-rate)<.1) {
    auto it=readers.find(source);if(it==readers.end()||temp.getNumSamples()<last-first)continue;
    it->second->setReadTimeout(realtime==AudioProcessor::Realtime::no?1000:0);
    auto start=(juce::int64)std::llround(((blockStart+first)/rate+offset)*sourceRate);
    if(it->second->read(&temp,0,last-first,start,true,true))for(int c=0;c<b.getNumChannels();++c)b.addFrom(c,first,temp,std::min(c,temp.getNumChannels()-1),0,last-first);
   }
  }
  return true;
 }
 using ARAPlaybackRenderer::processBlock;
private:
 struct Transition{const noori::Audio* current=nullptr;const noori::Audio* previous=nullptr;int remaining=0;};
 std::map<ARAPlaybackRegion*,Transition> transitions;
 ReadWriteLock& lock;double rate=48000;AudioBuffer<float> temp;TimeSliceThread readerThread;
 std::map<ARAAudioSource*,std::unique_ptr<BufferingAudioReader>> readers;
};
class NooriController final : public ARADocumentControllerSpecialisation,private Timer {
public:
 NooriController(const ARA::PlugIn::PlugInEntry* e,const ARA::ARADocumentControllerHostInstance* h):ARADocumentControllerSpecialisation(e,h){startTimer(100);}
 ~NooriController()override{stopTimer();}
protected:
 void willBeginEditing(ARADocument*)override {lock.enterWrite();editing=true;}
 void didEndEditing(ARADocument*)override {editing=false;lock.exitWrite();}
 ARAAudioModification* doCreateAudioModification(ARAAudioSource* s,ARA::ARAAudioModificationHostRef ref,const ARAAudioModification* clone)override{return new NooriModification(s,ref,clone);}
 ARAPlaybackRenderer* doCreatePlaybackRenderer()override{return new NooriRenderer(getDocumentController(),lock);}
 void doUpdateAudioSourceContent(ARAAudioSource* s,ARAContentUpdateScopes scopes)override {if(!scopes.affectSamples())return;for(auto* m:s->getAudioModifications<NooriModification>()){m->needsLoad=true;m->session->cancel();}}
 void didUpdatePlaybackRegionProperties(ARAPlaybackRegion* r)override {r->getAudioModification<NooriModification>()->needsTimeline=true;}
 void didUpdateAudioSourceProperties(ARAAudioSource* s)override {for(auto* m:s->getAudioModifications<NooriModification>()){auto a=m->session->original;if(!a||a->length()!=s->getSampleCount()||a->channels.size()!=(std::size_t)s->getChannelCount()||std::abs(a->sampleRate-s->getSampleRate())>.1){m->needsLoad=true;m->session->cancel();}else m->session->name=String::fromUTF8(s->getName());}}
 void doUpdateMusicalContextContent(ARAMusicalContext*,ARAContentUpdateScopes)override {for(auto* s:getDocument()->getAudioSources<ARAAudioSource>())for(auto* m:s->getAudioModifications<NooriModification>())m->needsTimeline=true;}
 bool doStoreObjectsToStream(ARAOutputStream& out,const ARAStoreObjectsFilter* filter)override {
  const auto& mods=filter->getAudioModificationsToStore<NooriModification>();if(!out.writeInt(1)||!out.writeInt((int)mods.size()))return false;
  for(auto* m:mods){auto state=m->session->archive(false);if(!out.writeString(String::fromUTF8(m->getPersistentID().c_str()))||!out.writeInt64((juce::int64)state.getSize())||!out.write(state.getData(),state.getSize()))return false;}
  return true;
 }
 bool doRestoreObjectsFromStream(ARAInputStream& in,const ARARestoreObjectsFilter* filter)override {
  if(in.readInt()!=1)return false;int n=in.readInt();if(n<0||n>100000)return false;
  for(int i=0;i<n;++i){auto id=in.readString();auto bytes=in.readInt64();if(bytes<0||bytes>8*1024*1024)return false;juce::MemoryBlock state((std::size_t)bytes);if(in.read(state.getData(),(int)bytes)!=bytes)return false;if(auto* m=filter->getAudioModificationToRestoreStateWithID<NooriModification>(id.toRawUTF8())){m->session->restore(state.getData(),state.getSize(),false);m->needsLoad=true;}}
  return !in.failed();
 }
private:
 ReadWriteLock lock;bool editing=false;

 void timerCallback()override {
  if(editing)return;
  for(auto* source:getDocument()->getAudioSources<ARAAudioSource>()) {
   if(!source->isSampleAccessEnabled())continue;
   for(auto* m:source->getAudioModifications<NooriModification>()) {
    if(m->session->busy)continue;
    if(m->needsTimeline&&!m->getPlaybackRegions().empty()){applyRegionTimeline(m,m->getPlaybackRegions().front());m->needsTimeline=false;}
    if(!m->needsLoad)continue;
    auto n=source->getSampleCount();auto nc=source->getChannelCount();double sr=source->getSampleRate();
    if(n<1||nc<1||nc>2||n*nc*4>128*1024*1024){m->session->status="ARA source exceeds mono/stereo 128 MiB limit";m->needsLoad=false;continue;}
    auto reader=std::shared_ptr<ARAAudioSourceReader>(new ARAAudioSourceReader(source),[](ARAAudioSourceReader* ptr){if(MessageManager::existsAndIsCurrentThread())delete ptr;else MessageManager::callAsync([ptr]{delete ptr;});});
    if(!m->getPlaybackRegions().empty())applyRegionTimeline(m,m->getPlaybackRegions().front());
    m->needsLoad=false;
    m->session->load([reader,n,nc,sr]{noori::Audio a{sr,std::vector<std::vector<float>>((std::size_t)nc,std::vector<float>((std::size_t)n))};AudioBuffer<float> block(nc,65536);for(juce::int64 at=0;at<n;){int size=(int)std::min<juce::int64>(65536,n-at);if(!reader->read(&block,0,size,at,true,true))throw std::runtime_error("ARA source access revoked or read failed");for(int c=0;c<nc;++c)std::memcpy(a.channels[(std::size_t)c].data()+at,block.getReadPointer(c),(std::size_t)size*sizeof(float));at+=size;}return a;},String::fromUTF8(source->getName()),m->session->origin,m->session->timeline);
   }
  }
 }
};
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory(){return ARADocumentControllerSpecialisation::createARAFactory<NooriController>();}
#endif
