#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>
namespace {
struct ConnectedInsert {std::uint64_t id;juce::String name;std::weak_ptr<EditSession> session;};
std::mutex& registryMutex(){static std::mutex m;return m;}
std::vector<ConnectedInsert>& insertRegistry(){static std::vector<ConnectedInsert> entries;return entries;}
std::atomic<std::uint64_t> nextInsertID{1};
constexpr std::size_t memoryLimit=512ull*1024*1024;
void writeSettings(juce::OutputStream& o,const noori::Settings& s) {
 o.writeInt(s.division);o.writeInt((int)s.rhythm);
 for(double v:{s.strength,s.swing,s.offsetMs,s.humanizeMs,s.toleranceMs,s.sensitivity,s.maxStretch})o.writeDouble(v);
 for(bool b:{s.breathProtection,s.consonantProtection,s.vowelProtection,s.emotionalProtection,s.naturalNaat,s.sampleAccurate})o.writeBool(b);
}
noori::Settings readSettings(juce::InputStream& i) {
 noori::Settings s;s.division=i.readInt();auto rhythm=i.readInt();if(rhythm<0||rhythm>2)throw std::runtime_error("Invalid rhythm");s.rhythm=(noori::Settings::Rhythm)rhythm;
 s.strength=i.readDouble();s.swing=i.readDouble();s.offsetMs=i.readDouble();s.humanizeMs=i.readDouble();s.toleranceMs=i.readDouble();s.sensitivity=i.readDouble();s.maxStretch=i.readDouble();
 s.breathProtection=i.readBool();s.consonantProtection=i.readBool();s.vowelProtection=i.readBool();s.emotionalProtection=i.readBool();s.naturalNaat=i.readBool();s.sampleAccurate=i.readBool();
 if((s.division!=4&&s.division!=8&&s.division!=16&&s.division!=32)||!std::isfinite(s.strength)||s.strength<0||s.strength>1||!std::isfinite(s.maxStretch)||s.maxStretch<1||s.maxStretch>2||!std::isfinite(s.sensitivity)||s.sensitivity<0||s.sensitivity>1||!std::isfinite(s.swing)||s.swing<0||s.swing>.75||!std::isfinite(s.offsetMs)||std::abs(s.offsetMs)>500||!std::isfinite(s.humanizeMs)||s.humanizeMs<0||s.humanizeMs>50||!std::isfinite(s.toleranceMs)||s.toleranceMs<0||s.toleranceMs>100)throw std::runtime_error("Invalid settings archive");
 return s;
}
}
void EditSession::loadReference(const juce::File& file,noori::Sample projectStart,bool clickTrack) {
 if(!original||busy)return;
 // Reference files must share the project sample rate. Vocal and click file starts
 // are anchored explicitly to the host cursor at their respective import times.
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
 if(!r||r->numChannels<1||r->numChannels>2||r->lengthInSamples<1||r->lengthInSamples*r->numChannels*4>128*1024*1024||std::abs(r->sampleRate-original->sampleRate)>.1){status="Use a mono/stereo reference file at the vocal sample rate, under 128 MiB.";return;}
 auto reader=std::shared_ptr<juce::AudioFormatReader>(r.release());const auto relative=projectStart-origin;
 const auto targetLength=original->length();const auto bytes=(std::size_t)targetLength*sizeof(float);if(retainedBytes+bytes>memoryLimit){status="Reference history memory limit reached.";return;}const auto detectorSettings=settings;submit([this,reader,relative,file,targetLength,bytes,clickTrack,detectorSettings]{Result result;result.clickTrack=clickTrack;juce::AudioBuffer<float> b((int)reader->numChannels,(int)reader->lengthInSamples);if(!reader->read(&b,0,b.getNumSamples(),0,true,true))throw std::runtime_error("Reference read failed");noori::Audio a;a.sampleRate=reader->sampleRate;for(int c=0;c<b.getNumChannels();++c)a.channels.emplace_back(b.getReadPointer(c),b.getReadPointer(c)+b.getNumSamples());auto overlay=std::make_shared<noori::Audio>();overlay->sampleRate=a.sampleRate;overlay->channels.resize(1,std::vector<float>((std::size_t)targetLength));for(noori::Sample i=0;i<a.length();++i){auto dst=i+relative;if(dst<0||dst>=targetLength)continue;for(const auto& c:a.channels)if(std::abs(c[(std::size_t)i])>std::abs(overlay->channels[0][(std::size_t)dst]))overlay->channels[0][(std::size_t)dst]=c[(std::size_t)i];}result.referenceAudio=overlay;if(clickTrack)result.clicks=noori::detectClicks(a);else for(const auto& attack:noori::detect(a,detectorSettings,[this](float p){progress=p;return !cancelled.load();}))result.clicks.push_back(attack.source);if(result.clicks.empty())throw std::runtime_error(clickTrack?"No metronome clicks found":"No vocal attacks found in reference");for(auto& click:result.clicks)click+=relative;result.referenceTitle=file.getFileName();result.referenceOnly=true;retainedBytes+=bytes;return result;});
}
std::shared_ptr<const noori::Audio> EditSession::snapshot()const {return rendered?rendered:original;}
SessionState EditSession::state()const {
 SessionState s;s.logoPNG=logoPNG;s.render={attacks,rendered};s.original=original;s.referenceAudio=referenceAudio;s.settings=settings;s.timeline=timeline;s.origin=origin;s.name=name;s.referenceName=referenceName;s.referenceProjectID=referenceProjectID;s.referenceClicks=referenceClicks;s.useReference=useReference;s.exactBpm=exactBpm;s.referenceIsClick=referenceIsClick;s.compare=compare;s.lightTheme=lightTheme;s.levelView=levelView;s.overlay=overlay;s.audition=audition;s.zoom=viewZoom;s.viewStart=viewStart;s.manualBpm=manualBpm;s.preset=preset;return s;
}
void EditSession::apply(const SessionState& s){
 if(rendered)retired.push_back({{},rendered});if(original)retired.push_back({{},original});
 logoPNG=s.logoPNG;original=s.original;rendered=s.render.audio;attacks=s.render.attacks;referenceAudio=s.referenceAudio;settings=s.settings;timeline=s.timeline;origin=s.origin;name=s.name;referenceName=s.referenceName;referenceProjectID=s.referenceProjectID;referenceClicks=s.referenceClicks;useReference=s.useReference;exactBpm=s.exactBpm;referenceIsClick=s.referenceIsClick;compare=s.compare;lightTheme=s.lightTheme;levelView=s.levelView;overlay=s.overlay;audition=s.audition;viewZoom=s.zoom;viewStart=s.viewStart;manualBpm=s.manualBpm;preset=s.preset;
 originalAudible=original.get();realtimeOrigin=origin;audible=rendered.get();++changeSerial;if(onChanged)onChanged();
}
void EditSession::record(const SessionState& before,juce::String label,bool merge){auto after=state();if(before==after)return;journal->record(shared_from_this(),before,std::move(after),label,merge);++changeSerial;if(onChanged)onChanged();}
void EditSession::change(juce::String label,std::function<void()> action,bool merge){if(busy)return;journal->finishIfIdle();auto before=state();action();record(before,label,merge);}
void EditJournal::record(std::shared_ptr<EditSession> s,SessionState before,SessionState after,juce::String label,bool merge){
 if(before==after)return;const double now=juce::Time::getMillisecondCounterHiRes();
 if(merge&&cursor==entries.size()&&cursor>0&&entries.back().session.lock()==s&&entries.back().label==label&&entries.back().group==activeGroup&&now-lastEdit<700){entries.back().after=std::move(after);lastEdit=now;return;}
 entries.resize(cursor);entries.push_back({s,std::move(before),std::move(after),label,activeGroup});cursor=entries.size();lastEdit=now;
}
bool EditJournal::available(bool redo)const {for(std::size_t i=0;i<entries.size();){if(entries[i].session.expired()){entries.erase(entries.begin()+(std::ptrdiff_t)i);if(i<cursor)--cursor;}else ++i;}if(hasPending())return false;for(const auto& e:entries)if(auto s=e.session.lock())if(s->busy)return false;if(redo?cursor>=entries.size():cursor==0)return false;auto s=entries[redo?cursor:cursor-1].session.lock();return s&&!s->busy;}
juce::String EditJournal::description(bool redo)const {return available(redo)?entries[redo?cursor:cursor-1].label:juce::String();}
void EditJournal::step(bool redo){
 if(!available(redo))return;const auto group=entries[redo?cursor:cursor-1].group;
 std::size_t begin=redo?cursor:cursor-1,end=begin+1;
 if(group){while(begin>0&&entries[begin-1].group==group)--begin;while(end<entries.size()&&entries[end].group==group)++end;}
 for(std::size_t i=begin;i<end;++i){auto s=entries[i].session.lock();if(!s||s->busy)return;}
 auto apply=[&](std::size_t i){auto& e=entries[i];if(auto s=e.session.lock()){s->apply(redo?e.after:e.before);s->status=(redo?"Redo: ":"Undo: ")+e.label;restoredSession=s;}};
 if(redo){for(auto i=begin;i<end;++i)apply(i);cursor=end;}else{for(auto i=end;i>begin;)apply(--i);cursor=begin;}lastEdit=0;
}
void EditSession::selectAll(bool selected){if(busy)return;change("Attack selection",[&]{for(auto& a:attacks)a.selected=selected;});}
void EditSession::selectRange(noori::Sample first,noori::Sample last,bool additive,bool sourceCoordinates){if(busy)return;if(first>last)std::swap(first,last);change("Attack selection",[&]{for(auto& a:attacks)a.selected=(additive&&a.selected)||((sourceCoordinates?a.source:a.target)>=first&&(sourceCoordinates?a.source:a.target)<=last);});}
void EditSession::anchorTo(noori::Sample start){if(busy)return;change("Audio start",[&]{auto shift=origin-start;for(auto& click:referenceClicks)click+=shift;if(referenceAudio){auto a=std::make_shared<noori::Audio>();a->sampleRate=referenceAudio->sampleRate;a->channels.resize(1,std::vector<float>((std::size_t)referenceAudio->length()));for(noori::Sample i=0;i<referenceAudio->length();++i)if(i+shift>=0&&i+shift<referenceAudio->length())a->channels[0][(std::size_t)(i+shift)]=referenceAudio->channels[0][(std::size_t)i];referenceAudio=a;}origin=start;realtimeOrigin=start;});status="Audio start anchored to Cubase cursor";if(onChanged)onChanged();}
void EditSession::loadProjectReference(std::vector<ProjectReferenceRegion> regions,juce::String title,bool clickTrack,bool alignWhenReady,bool selectedOnly,AttackRange range,juce::String projectID){
 if(!original||busy)return;const auto rate=original->sampleRate;const auto start=origin;const auto settingsCopy=settings;const auto bytes=(std::size_t)original->length()*sizeof(float);if(retainedBytes+bytes>memoryLimit){status="Reference history memory limit reached.";return;}
 submit([this,regions=std::move(regions),title,clickTrack,rate,start,settingsCopy,alignWhenReady,selectedOnly,range,projectID,bytes]{Result r;r.projectID=projectID;r.clickTrack=clickTrack;r.range=range;r.alignWhenReady=alignWhenReady;r.selectedOnly=selectedOnly;r.referenceOnly=true;r.referenceTitle=title;auto overlay=std::make_shared<noori::Audio>();overlay->sampleRate=rate;overlay->channels.resize(1,std::vector<float>((std::size_t)original->length()));
  for(const auto& region:regions){if(!region.audio)throw std::runtime_error("Reference track is still loading. Try again shortly.");
   std::vector<noori::Sample> positions;if(clickTrack)positions=noori::detectClicks(*region.audio);else for(const auto& a:noori::detect(*region.audio,settingsCopy,[this](float p){progress=p;return !cancelled.load();}))positions.push_back(a.source);
   for(noori::Sample dst=std::max<noori::Sample>(0,(noori::Sample)std::ceil(region.playbackStart*rate)-start);dst<std::min<noori::Sample>(original->length(),(noori::Sample)std::ceil((region.playbackStart+region.sourceDuration)*rate)-start);++dst){double src=(double(dst+start)/rate-region.playbackStart+region.sourceStart)*region.audio->sampleRate;auto at=(noori::Sample)std::floor(src);if(at<0||at+1>=region.audio->length())continue;float value=0;for(const auto& c:region.audio->channels){auto v=(float)(c[(std::size_t)at]*(1-(src-at))+c[(std::size_t)at+1]*(src-at));if(std::abs(v)>std::abs(value))value=v;}auto& out=overlay->channels[0][(std::size_t)dst];if(std::abs(value)>std::abs(out))out=value;}
   for(auto sample:positions){double sourceTime=sample/region.audio->sampleRate;if(sourceTime<region.sourceStart||sourceTime>=region.sourceStart+region.sourceDuration)continue;auto at=(noori::Sample)std::llround((region.playbackStart+sourceTime-region.sourceStart)*rate)-start;r.clicks.push_back(at);}
  }
  r.referenceAudio=overlay;std::sort(r.clicks.begin(),r.clicks.end());r.clicks.erase(std::unique(r.clicks.begin(),r.clicks.end()),r.clicks.end());if(r.clicks.empty())throw std::runtime_error("No attacks found in the exposed reference regions");retainedBytes+=bytes;return r;
 });
}
EditSession::EditSession():journal(std::make_shared<EditJournal>()){startTimerHz(20);}
EditSession::~EditSession(){stopTimer();cancelled=true;pool.removeAllJobs(true,-1);}
void EditSession::submit(std::function<Result()> job) {
 if(busy.exchange(true))return;
 journal->track(shared_from_this());pendingBefore=state();cancelled=false;progress=0;status="Working...";
 pool.addJob([this,job=std::move(job)] {Result result;try{result=job();}catch(const std::exception& e){result.error=juce::String::fromUTF8(e.what());}catch(...){result.error="Processing failed";}std::lock_guard lock(pendingMutex);pending=std::make_unique<Result>(std::move(result));});
}
void EditSession::timerCallback(){settle();}
void EditSession::settle() {
 std::unique_ptr<Result> result;{std::lock_guard lock(pendingMutex);result=std::move(pending);}if(!result)return;
 if(cancelled){pendingBefore.reset();status="Cancelled";busy=false;return;}
 if(result->error.isNotEmpty()){pendingBefore.reset();status=result->error;busy=false;return;}
 if(result->referenceOnly){referenceAudio=result->referenceAudio;referenceClicks=std::move(result->clicks);referenceName=result->referenceTitle;referenceProjectID=result->projectID;referenceIsClick=result->clickTrack;useReference=true;busy=false;status=juce::String((int)referenceClicks.size())+" reference attacks | ready";if(pendingBefore){record(*pendingBefore,"Reference track");pendingBefore.reset();}if(result->alignWhenReady)align(result->selectedOnly,result->range);return;}
 if(result->newSource){if(original)retired.push_back({{},original});original=result->source;if(restoreMarkers.empty()){referenceClicks.clear();referenceName.clear();referenceProjectID.clear();referenceAudio.reset();useReference=false;}if(result->sourceName.isNotEmpty()){name=result->sourceName;origin=result->sourceOrigin;timeline=result->sourceTimeline;}restoreMarkers.clear();originalAudible=original.get();realtimeOrigin=origin;}
 publish(std::move(result->revision));compare=false;if(result->message.isNotEmpty())audition=true;busy=false;status=result->message.isNotEmpty()?result->message:juce::String((int)attacks.size())+" attacks | ready";if(pendingBefore){record(*pendingBefore,result->message.isNotEmpty()?"Align attacks":"Audio / attacks");pendingBefore.reset();}
}
void EditSession::publish(Revision r) {if(rendered)retired.push_back({{},rendered});attacks=std::move(r.attacks);rendered=std::move(r.audio);audible=rendered.get();}
void EditSession::load(std::function<noori::Audio()> reader,juce::String title,noori::Sample start,noori::Timeline map) {
 if(busy)return;
 const auto markerState=restoreMarkers;const auto renderSettings=settings;
 submit([this,reader=std::move(reader),markerState,renderSettings,title,start,map] {Result r;auto a=std::make_shared<noori::Audio>(reader());a->validate();auto bytes=(std::size_t)a->length()*a->channels.size()*sizeof(float);if(bytes>128ull*1024*1024||retainedBytes+bytes>memoryLimit)throw std::runtime_error("Audio/session memory limit reached; open a new instance.");retainedBytes+=bytes;r.newSource=true;r.source=a;r.revision.audio=a;r.sourceName=title;r.sourceOrigin=start;r.sourceTimeline=map;
 if(!markerState.empty()){r.revision.attacks=noori::deserializeAttacks(markerState,a->length());auto output=std::make_shared<noori::Audio>(noori::render(*a,r.revision.attacks,renderSettings));r.revision.audio=output;retainedBytes+=bytes;}
 return r;});
}
void EditSession::analyse() {
 if(!original||busy)return;const auto audio=original;const auto s=settings;
 submit([this,audio,s] {Result r;r.revision.attacks=noori::detect(*audio,s,[this](float p){progress=p;return !cancelled.load();});r.revision.audio=audio;return r;});
}
void EditSession::renderMarkers(noori::Attacks markers) {
 if(!original||busy)return;const auto input=original;const auto s=settings;
 const auto bytes=(std::size_t)input->length()*input->channels.size()*sizeof(float);
 if(retainedBytes+bytes>memoryLimit){status="History memory limit: open a new instance to continue.";return;}
 submit([this,input,s,markers=std::move(markers),bytes] {Result r;r.revision.attacks=markers;r.revision.audio=std::make_shared<noori::Audio>(noori::render(*input,markers,s,[this](float p){progress=p;return !cancelled.load();}));retainedBytes+=bytes;return r;});
}
void EditSession::align(bool selectedOnly,AttackRange range) {
 if(!original||busy)return;
 if(selectedOnly&&!range&&std::none_of(attacks.begin(),attacks.end(),[](const auto& a){return a.selected;})){status="Select attacks first: click markers, drag a range, or use Select all attacks.";return;}
 auto input=original;auto map=timeline;auto s=settings;auto start=origin;auto m=attacks;auto clicks=useReference?referenceClicks:std::vector<noori::Sample>{};
 const bool strictBpm=exactBpm&&!useReference;
 if(useReference||strictBpm){s.offsetMs=0;s.swing=0;s.strength=1;s.humanizeMs=0;s.toleranceMs=0;s.emotionalProtection=false;s.sampleAccurate=true;}
 if(useReference&&clicks.empty()){status="Import a metronome reference first.";return;}
 auto bytes=(std::size_t)input->length()*input->channels.size()*sizeof(float);if(retainedBytes+bytes>memoryLimit){status="History memory limit reached.";return;}
 submit([this,input,map,s,start,m=std::move(m),clicks=std::move(clicks),selectedOnly,strictBpm,range,bytes]()mutable{Result r;if(m.empty())m=noori::detect(*input,s,[this](float p){progress=p*.35f;return !cancelled.load();});if(range){for(auto& a:m)a.selected=a.source>=range->first&&a.source<range->second;selectedOnly=true;if(std::none_of(m.begin(),m.end(),[](const auto& a){return a.selected;}))throw std::runtime_error("No detected attacks inside this event region");}r.revision.attacks=noori::quantize(m,input->length(),start,map,s,selectedOnly,clicks,strictBpm);if(m.empty())throw std::runtime_error("No vocal attacks detected. Adjust sensitivity and detect again.");int limited=0;
 if(clicks.empty()&&s.strength==1&&s.humanizeMs==0&&s.toleranceMs==0&&s.sampleAccurate){double step=4./s.division;if(s.rhythm==noori::Settings::Rhythm::triplet)step*=2./3;if(s.rhythm==noori::Settings::Rhythm::dotted)step*=1.5;for(const auto& a:r.revision.attacks){if(selectedOnly&&!a.selected)continue;auto q=map.quarterAt(a.source+start);auto k=(noori::Sample)std::floor(q/step);noori::Sample desired=a.source;double distance=1e99;for(auto j=k-2;j<=k+2;++j){auto candidate=map.sampleAt(j*step+(j%2?s.swing*step:0))-start+(noori::Sample)std::llround(s.offsetMs*.001*map.sampleRate());auto d=std::abs(double(candidate-a.source));if(d<distance){desired=candidate;distance=d;}}if(a.target!=desired)++limited;}}
 r.message=strictBpm?(limited?juce::String(limited)+" clip-edge attacks kept; interior attacks exactly on BPM grid | ready":"Exact BPM grid rendered | ready"):limited?juce::String(limited)+" attacks limited by stretch safety | ready":clicks.empty()?"Natural BPM alignment rendered | ready":"Exact reference track matched | ready";
 int moved=0,processed=0;for(const auto& a:r.revision.attacks){if(selectedOnly&&!a.selected)continue;++processed;if(a.target!=a.source)++moved;}
 r.message=juce::String(moved)+" / "+juce::String(processed)+" attacks moved | "+r.message;
 r.revision.audio=std::make_shared<noori::Audio>(noori::render(*input,r.revision.attacks,s,[this](float p){progress=.35f+.65f*p;return !cancelled.load();}));retainedBytes+=bytes;return r;});
}
void EditSession::move(std::size_t index,noori::Sample target){if(original&&!busy)try{renderMarkers(noori::moveAttack(attacks,index,target,original->length(),settings.maxStretch));}catch(const std::exception& e){status=juce::String::fromUTF8(e.what());}}
void EditSession::addAttack(noori::Sample source){
 if(!original||busy)return;source=std::clamp<noori::Sample>(source,1,original->length()-1);auto m=attacks;
 auto it=std::lower_bound(m.begin(),m.end(),source,[](const auto& a,auto x){return a.source<x;});if(it!=m.end()&&it->source==source){status="An attack already exists at this sample";return;}
 auto ps=it==m.begin()?0:(it-1)->source,pt=it==m.begin()?0:(it-1)->target;auto ns=it==m.end()?original->length():it->source,nt=it==m.end()?original->length():it->target;
 auto target=pt+(noori::Sample)std::llround(double(source-ps)*(nt-pt)/(ns-ps));for(auto& a:m)a.selected=false;m.insert(it,{source,target,1,1,true});renderMarkers(std::move(m));
}
void EditSession::removeSelectedAttacks(){if(!original||busy)return;auto m=attacks;m.erase(std::remove_if(m.begin(),m.end(),[](const auto& a){return a.selected;}),m.end());if(m.size()!=attacks.size())renderMarkers(std::move(m));}
void EditSession::setSelected(std::size_t i,bool additive){if(busy||i>=attacks.size())return;change("Attack selection",[&]{for(std::size_t j=0;j<attacks.size();++j)attacks[j].selected=j==i|| (additive&&attacks[j].selected);});}
void EditSession::reset(){if(original&&!busy)change("Reset attacks",[this]{publish({{},original});compare=false;status="Reset edits | undo available";});}
bool EditSession::canUndo()const{return !busy&&journal->available(false);}bool EditSession::canRedo()const{return !busy&&journal->available(true);}
void EditSession::undo(){if(!busy)journal->step(false);}void EditSession::redo(){if(!busy)journal->step(true);}
juce::String EditSession::undoDescription()const{return journal->description(false);}juce::String EditSession::redoDescription()const{return journal->description(true);}
void EditSession::cancel(){cancelled=true;}
juce::MemoryBlock EditSession::archive(bool includeAudio)const {
 juce::MemoryOutputStream out;out.writeInt(0x4e545431);writeSettings(out,settings);out.writeString(name);out.writeInt64(origin);out.writeString(!original&&!restoreMarkers.empty()?restoreMarkers:noori::serializeAttacks(attacks));out.writeDouble(timeline.sampleRate());out.writeInt((int)timeline.points().size());for(auto t:timeline.points()){out.writeInt64(t.sample);out.writeDouble(t.bpm);out.writeDouble(t.quarter);}
 out.writeBool(includeAudio&&original!=nullptr);
 if(includeAudio&&original){out.writeDouble(original->sampleRate);out.writeInt((int)original->channels.size());out.writeInt64(original->length());for(const auto& c:original->channels)for(float x:c)out.writeFloat(x);}
 out.writeBool(useReference);out.writeString(referenceName);out.writeInt((int)referenceClicks.size());for(auto click:referenceClicks)out.writeInt64(click);out.writeBool(exactBpm);out.writeString(referenceProjectID);out.writeBool(referenceIsClick);out.writeBool(lightTheme);out.writeBool(levelView);out.writeBool(overlay);out.writeBool(audition);out.writeDouble(viewZoom);out.writeDouble(viewStart);out.writeDouble(manualBpm);out.writeInt(preset);out.writeBool(settings.percussion);out.writeInt(logoPNG?(int)logoPNG->getSize():0);if(logoPNG)out.write(logoPNG->getData(),logoPNG->getSize());out.writeBool(referenceAudio!=nullptr);if(referenceAudio){out.writeDouble(referenceAudio->sampleRate);out.writeInt64(referenceAudio->length());for(auto v:referenceAudio->channels[0])out.writeFloat(v);}
 return out.getMemoryBlock();
}
void EditSession::restore(const void* data,std::size_t size,bool includeAudio) {
 if(busy)return;
 try {
  referenceClicks.clear();referenceName.clear();referenceProjectID.clear();useReference=false;
  juce::MemoryInputStream in(data,size,false);if(in.readInt()!=0x4e545431)throw std::runtime_error("Unsupported project state");auto restoredSettings=readSettings(in);auto title=in.readString();auto start=in.readInt64();auto markers=in.readString().toStdString();auto sr=in.readDouble();int count=in.readInt();if(count<=0||count>16384)throw std::runtime_error("Invalid tempo archive");std::vector<noori::TempoPoint> points;for(int i=0;i<count;++i){noori::TempoPoint p;p.sample=in.readInt64();p.bpm=in.readDouble();p.quarter=in.readDouble();points.push_back(p);}noori::Timeline map(sr,points);
  bool hasAudio=in.readBool();settings=restoredSettings;restoreMarkers=markers;name=title;origin=start;timeline=map;
  if(hasAudio&&includeAudio) {
   auto a=std::make_shared<noori::Audio>();a->sampleRate=in.readDouble();int channels=in.readInt();auto n=in.readInt64();if(channels<1||channels>2||n<1||n>128*1024*1024/(channels*4)||in.getNumBytesRemaining()<n*channels*4)throw std::runtime_error("Invalid audio archive");a->channels.resize((std::size_t)channels);for(auto& c:a->channels){c.resize((std::size_t)n);for(auto& x:c)x=in.readFloat();}load([a]{return *a;},title,start,map);
  }
  if(in.getNumBytesRemaining()>0){useReference=in.readBool();referenceName=in.readString();int n=in.readInt();if(n<0||n>1000000||in.getNumBytesRemaining()<n*8ll)throw std::runtime_error("Invalid reference archive");referenceClicks.clear();for(int i=0;i<n;++i)referenceClicks.push_back(in.readInt64());if(!std::is_sorted(referenceClicks.begin(),referenceClicks.end()))throw std::runtime_error("Unordered reference clicks");if(in.getNumBytesRemaining()>0)exactBpm=in.readBool();if(in.getNumBytesRemaining()>0){referenceProjectID=in.readString();referenceIsClick=in.readBool();}if(in.getNumBytesRemaining()>0){lightTheme=in.readBool();levelView=in.readBool();overlay=in.readBool();audition=in.readBool();viewZoom=std::clamp(in.readDouble(),1.,24.);viewStart=std::clamp(in.readDouble(),0.,1.-1./viewZoom);manualBpm=std::clamp(in.readDouble(),1.,1000.);preset=std::clamp(in.readInt(),0,3);settings.percussion=in.readBool();if(in.getNumBytesRemaining()>0){auto n=in.readInt();if(n<0||n>4*1024*1024||n>in.getNumBytesRemaining())throw std::runtime_error("Invalid logo archive");auto logoData=std::make_shared<juce::MemoryBlock>((std::size_t)n);in.read(logoData->getData(),(std::size_t)n);logoPNG=n?logoData:nullptr;}}}
  if(in.getNumBytesRemaining()>0&&in.readBool()){auto a=std::make_shared<noori::Audio>();a->sampleRate=in.readDouble();auto n=in.readInt64();if(n<1||n>128*1024*1024/4||in.getNumBytesRemaining()<n*4)throw std::runtime_error("Invalid reference waveform archive");a->channels.resize(1,std::vector<float>((std::size_t)n));for(auto& v:a->channels[0])v=in.readFloat();a->validate();referenceAudio=a;retainedBytes+=(std::size_t)n*sizeof(float);}
 }catch(const std::exception& e){status=juce::String::fromUTF8(e.what());}
}
NooriProcessor::NooriProcessor():AudioProcessor(BusesProperties().withInput("Vocal input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),insertSession(std::make_shared<EditSession>()){registryID=nextInsertID.fetch_add(1);std::lock_guard lock(registryMutex());insertRegistry().push_back({registryID,"Noori insert "+juce::String((juce::int64)registryID),insertSession});}
NooriProcessor::~NooriProcessor(){captureArmed=false;std::lock_guard lock(registryMutex());auto& entries=insertRegistry();entries.erase(std::remove_if(entries.begin(),entries.end(),[this](const auto& e){return e.id==registryID;}),entries.end());}
void NooriProcessor::updateTrackProperties(const TrackProperties& properties){std::lock_guard lock(registryMutex());for(auto& e:insertRegistry())if(e.id==registryID&&properties.name.has_value()&&properties.name->isNotEmpty())e.name=*properties.name;}
std::vector<ProjectTrack> NooriProcessor::connectedReferenceTracks()const {std::vector<ConnectedInsert> entries;{std::lock_guard lock(registryMutex());entries=insertRegistry();}std::vector<ProjectTrack> result;for(const auto& e:entries){if(e.id==registryID)continue;auto s=e.session.lock();if(!s)continue;auto a=s->snapshot();if(!a)continue;ProjectTrack track;track.id="insert-"+juce::String((juce::int64)e.id);track.name=e.name+" (connected insert)";track.persistentID="insert-track-"+e.name;track.editable=false;track.regions.push_back({track.id,e.name,s,0,a?a->length()/a->sampleRate:0,a?s->origin/a->sampleRate:0});result.push_back(std::move(track));}return result;}

void NooriProcessor::prepareToPlay(double sr,int block){deviceRate=sr;captureArmed=false;activeBuffer=nullptr;transitionBuffer=nullptr;crossfadeRemaining=0;capture={sr,std::vector<std::vector<float>>((std::size_t)getTotalNumInputChannels(),std::vector<float>((std::size_t)(sr*120),0))};
#if JucePlugin_Enable_ARA
 prepareToPlayForARA(sr,block,getTotalNumOutputChannels(),getProcessingPrecision());
#else
 juce::ignoreUnused(block);
#endif
}
void NooriProcessor::releaseResources(){captureArmed=false;
#if JucePlugin_Enable_ARA
 releaseResourcesForARA();
#endif
}
bool NooriProcessor::isBusesLayoutSupported(const BusesLayout& l)const{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());}
void NooriProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
 juce::ScopedNoDenormals denorm;juce::AudioPlayHead::PositionInfo pos;
 bool valid=false;if(auto* head=getPlayHead())if(auto info=head->getPosition()){pos=*info;valid=true;}
 transportSequence.fetch_add(1,std::memory_order_acq_rel);
 hostTempoValid=valid&&pos.getBpm().hasValue()&&pos.getPpqPosition().hasValue()&&pos.getTimeInSamples().hasValue();bpm=pos.getBpm().orFallback(80);quarter=pos.getPpqPosition().orFallback(0);hostSample=pos.getTimeInSamples().orFallback(0);playing=pos.getIsPlaying();auto sig=pos.getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{});numerator=sig.numerator;denominator=sig.denominator;
 transportSequence.fetch_add(1,std::memory_order_release);
#if JucePlugin_Enable_ARA
 if(processBlockForARA(buffer,isRealtime(),pos))return;
#endif
 const auto block=buffer.getNumSamples();
 if(captureArmed.load(std::memory_order_acquire)&&playing) {
  auto n=captureCount.load();auto start=hostSample.load();
  if(n==0){captureOrigin=start;expectedSample=start;tempoCount=0;}
  if(start!=expectedSample||n+block>capture.length()){captureStopped=true;}
  else {
   if(tempoCount==0||std::abs(capturedTempo[tempoCount-1].bpm-bpm.load())>1e-6){if(tempoCount<capturedTempo.size()){double q=quarter.load();if(tempoCount){auto p=capturedTempo[tempoCount-1];q=p.quarter+double(start-p.sample)*p.bpm/(60*deviceRate);}capturedTempo[tempoCount++]={start,bpm.load(),q};}else captureStopped=true;}
   if(!captureStopped){for(int c=0;c<buffer.getNumChannels();++c)std::memcpy(capture.channels[(std::size_t)c].data()+n,buffer.getReadPointer(c),(std::size_t)block*sizeof(float));expectedSample=start+block;captureCount.store(n+block,std::memory_order_release);}
  }
 }
 const auto* desired=audition?(insertSession->compare.load()?insertSession->originalAudible.load():insertSession->audible.load(std::memory_order_acquire)):nullptr;
 if(desired&&(std::abs(desired->sampleRate-deviceRate)>.1||desired->channels.empty()))desired=nullptr;
 auto desiredOrigin=insertSession->realtimeOrigin.load();
 if(desired!=activeBuffer||(desired&&desiredOrigin!=bufferOrigin)){transitionBuffer=activeBuffer;transitionOrigin=bufferOrigin;activeBuffer=desired;bufferOrigin=desiredOrigin;crossfadeRemaining=256;}
 if(activeBuffer||crossfadeRemaining>0){
  auto read=[&](const noori::Audio* a,noori::Sample origin,int c,int i,float dry){if(!a)return dry;auto at=hostSample.load()-origin+i;if(!playing||at<0||at>=a->length())return 0.f;auto value=a->channels[(std::size_t)std::min(c,(int)a->channels.size()-1)][(std::size_t)at];if(buffer.getNumChannels()==1&&a->channels.size()==2)value=.5f*(a->channels[0][(std::size_t)at]+a->channels[1][(std::size_t)at]);return value;};
  for(int c=0;c<buffer.getNumChannels();++c)for(int i=0;i<block;++i){float dry=buffer.getSample(c,i);float value=read(activeBuffer,bufferOrigin,c,i,dry);if(i<crossfadeRemaining){float previous=read(transitionBuffer,transitionOrigin,c,i,dry);double mix=.5-.5*std::cos(3.141592653589793*(256-crossfadeRemaining+i+1)/256);value=float(previous*(1-mix)+value*mix);}buffer.setSample(c,i,value);}
  crossfadeRemaining=std::max(0,crossfadeRemaining-block);
 }

}
NooriProcessor::Transport NooriProcessor::transport()const {for(int attempt=0;attempt<10;++attempt){auto before=transportSequence.load(std::memory_order_acquire);if(before&1)continue;Transport t{bpm.load(),quarter.load(),hostSample.load(),hostTempoValid.load()};if(before==transportSequence.load(std::memory_order_acquire))return t;}return {80,0,0,false};}
void NooriProcessor::beginCapture(){if(insertSession->busy)return;suspendProcessing(true);captureCount=0;tempoCount=0;captureStopped=false;audition=false;captureArmed=true;suspendProcessing(false);}
void NooriProcessor::finishCapture(){suspendProcessing(true);captureArmed=false;auto n=captureCount.load();if(n>0){auto a=std::make_shared<noori::Audio>();a->sampleRate=deviceRate;a->channels.resize(capture.channels.size());for(std::size_t c=0;c<a->channels.size();++c)a->channels[c].assign(capture.channels[c].begin(),capture.channels[c].begin()+n);std::vector<noori::TempoPoint> map(capturedTempo.begin(),capturedTempo.begin()+(std::ptrdiff_t)tempoCount);insertSession->load([a]{return *a;},"Captured vocal",captureOrigin,noori::Timeline(deviceRate,map));}suspendProcessing(false);}
juce::String NooriProcessor::captureStatus()const {return captureStopped?"Capture paused: seek, loop, tempo capacity, or 120 s limit. Press Finish.":"Capturing when Cubase plays: "+juce::String(captureCount.load()/deviceRate,1)+" s";}
void NooriProcessor::importFile(const juce::File& file){auto transportState=transport();auto start=transportState.sample;auto rate=deviceRate;auto b=transportState.bpm;auto q=transportState.quarter;insertSession->load([file,rate]{juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));if(!r)throw std::runtime_error("Cannot read this audio file");if(r->numChannels<1||r->numChannels>2||r->lengthInSamples<1||r->lengthInSamples*(juce::int64)r->numChannels*4>128*1024*1024)throw std::runtime_error("Use a mono/stereo file smaller than 128 MiB decoded");if(std::abs(r->sampleRate-rate)>.1)throw std::runtime_error("File sample rate must match the Cubase project");juce::AudioBuffer<float> buffer((int)r->numChannels,(int)r->lengthInSamples);if(!r->read(&buffer,0,buffer.getNumSamples(),0,true,true))throw std::runtime_error("Audio read failed");noori::Audio a;a.sampleRate=r->sampleRate;for(int c=0;c<buffer.getNumChannels();++c)a.channels.emplace_back(buffer.getReadPointer(c),buffer.getReadPointer(c)+buffer.getNumSamples());return a;},file.getFileName(),start,noori::Timeline(rate,{{start,b,q}}));}
void NooriProcessor::exportFile(const juce::File& file,std::shared_ptr<EditSession> s){if(!s->audible.load()||s->busy)return;auto* a=s->audible.load();juce::WavAudioFormat format;juce::TemporaryFile temporary(file);auto stream=temporary.getFile().createOutputStream();if(!stream){s->status="Cannot create export file";return;}std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),a->sampleRate,(unsigned)a->channels.size(),24,{},0));if(!writer){s->status="Cannot create 24-bit WAV writer";return;}std::vector<const float*> ptr;for(const auto& c:a->channels)ptr.push_back(c.data());bool ok=writer->writeFromFloatArrays(ptr.data(),(int)ptr.size(),(int)a->length());writer.reset();s->status=ok&&temporary.overwriteTargetFileWithTemporary()?"Exported 24-bit WAV":"WAV write failed";}
juce::AudioProcessorEditor* NooriProcessor::createEditor(){return new NooriEditor(*this);}
void NooriProcessor::getStateInformation(juce::MemoryBlock& block){block=insertSession->archive(true);}
void NooriProcessor::setStateInformation(const void* data,int bytes){if(bytes>0)insertSession->restore(data,(std::size_t)bytes,true);audition=insertSession->audition;}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NooriProcessor;}
