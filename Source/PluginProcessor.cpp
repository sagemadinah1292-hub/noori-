#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>
namespace {
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
void EditSession::loadReference(const juce::File& file,noori::Sample projectStart) {
 if(!original||busy)return;
 // Reference files must share the project sample rate. Vocal and click file starts
 // are anchored explicitly to the host cursor at their respective import times.
 juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));
 if(!r||r->numChannels<1||r->numChannels>2||r->lengthInSamples<1||r->lengthInSamples*r->numChannels*4>128*1024*1024||std::abs(r->sampleRate-original->sampleRate)>.1){status="Use a mono/stereo click WAV at the vocal sample rate, under 128 MiB.";return;}
 auto reader=std::shared_ptr<juce::AudioFormatReader>(r.release());const auto relative=projectStart-origin;
 submit([reader,relative,file]{Result result;juce::AudioBuffer<float> b((int)reader->numChannels,(int)reader->lengthInSamples);if(!reader->read(&b,0,b.getNumSamples(),0,true,true))throw std::runtime_error("Reference read failed");noori::Audio a;a.sampleRate=reader->sampleRate;for(int c=0;c<b.getNumChannels();++c)a.channels.emplace_back(b.getReadPointer(c),b.getReadPointer(c)+b.getNumSamples());result.clicks=noori::detectClicks(a);if(result.clicks.empty())throw std::runtime_error("No metronome clicks found");for(auto& click:result.clicks)click+=relative;result.referenceTitle=file.getFileName();result.referenceOnly=true;return result;});
}
EditSession::EditSession(){startTimerHz(20);}
EditSession::~EditSession(){stopTimer();cancelled=true;pool.removeAllJobs(true,-1);}
void EditSession::submit(std::function<Result()> job) {
 if(busy.exchange(true))return;
 cancelled=false;progress=0;status="Working...";
 pool.addJob([this,job=std::move(job)] {Result result;try{result=job();}catch(const std::exception& e){result.error=juce::String::fromUTF8(e.what());}catch(...){result.error="Processing failed";}std::lock_guard lock(pendingMutex);pending=std::make_unique<Result>(std::move(result));});
}
void EditSession::timerCallback(){settle();}
void EditSession::settle() {
 std::unique_ptr<Result> result;{std::lock_guard lock(pendingMutex);result=std::move(pending);}if(!result)return;
 if(cancelled){status="Cancelled";busy=false;return;}
 if(result->error.isNotEmpty()){status=result->error;busy=false;return;}
 if(result->referenceOnly){referenceClicks=std::move(result->clicks);referenceName=result->referenceTitle;useReference=true;busy=false;status=juce::String((int)referenceClicks.size())+" reference clicks | ready";if(onChanged)onChanged();return;}
 if(result->newSource){if(original)retired.push_back({{},original});for(auto& r:history)retired.push_back(std::move(r));history.clear();current=-1;original=result->source;restoreMarkers.clear();originalAudible=original.get();realtimeOrigin=origin;}
 publish(std::move(result->revision));compare=false;busy=false;status=result->message.isNotEmpty()?result->message:juce::String((int)attacks.size())+" attacks | ready";if(onChanged)onChanged();
}
void EditSession::publish(Revision r) {
 if(current+1<(int)history.size()){for(auto i=current+1;i<(int)history.size();++i)retired.push_back(std::move(history[(std::size_t)i]));history.resize((std::size_t)(current+1));}
 attacks=r.attacks;history.push_back(std::move(r));current=(int)history.size()-1;
 audible.store(history.back().audio.get(),std::memory_order_release);
}
void EditSession::load(std::function<noori::Audio()> reader,juce::String title,noori::Sample start,noori::Timeline map) {
 if(busy)return;if(restoreMarkers.empty()){referenceClicks.clear();referenceName.clear();useReference=false;}name=title;origin=start;timeline=std::move(map);
 const auto markerState=restoreMarkers;const auto renderSettings=settings;
 submit([this,reader=std::move(reader),markerState,renderSettings] {Result r;auto a=std::make_shared<noori::Audio>(reader());a->validate();auto bytes=(std::size_t)a->length()*a->channels.size()*sizeof(float);if(bytes>128ull*1024*1024||retainedBytes+bytes>memoryLimit)throw std::runtime_error("Audio/session memory limit reached; open a new instance.");retainedBytes+=bytes;r.newSource=true;r.source=a;r.revision.audio=a;
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
void EditSession::align(bool selectedOnly) {
 if(!original||busy)return;
 auto input=original;auto map=timeline;auto s=settings;auto start=origin;auto m=attacks;auto clicks=useReference?referenceClicks:std::vector<noori::Sample>{};
 if(useReference){s.strength=1;s.humanizeMs=0;s.toleranceMs=0;s.emotionalProtection=false;s.sampleAccurate=true;}
 if(useReference&&clicks.empty()){status="Import a metronome reference first.";return;}
 auto bytes=(std::size_t)input->length()*input->channels.size()*sizeof(float);if(retainedBytes+bytes>memoryLimit){status="History memory limit reached.";return;}
 submit([this,input,map,s,start,m=std::move(m),clicks=std::move(clicks),selectedOnly,bytes]()mutable{Result r;if(m.empty())m=noori::detect(*input,s,[this](float p){progress=p*.35f;return !cancelled.load();});r.revision.attacks=noori::quantize(m,input->length(),start,map,s,selectedOnly,clicks);int limited=0;
 if(clicks.empty()&&s.strength==1&&s.humanizeMs==0&&s.toleranceMs==0&&s.sampleAccurate){double step=4./s.division;if(s.rhythm==noori::Settings::Rhythm::triplet)step*=2./3;if(s.rhythm==noori::Settings::Rhythm::dotted)step*=1.5;for(const auto& a:r.revision.attacks){if(selectedOnly&&!a.selected)continue;auto q=map.quarterAt(a.source+start);auto k=(noori::Sample)std::floor(q/step);noori::Sample desired=a.source;double distance=1e99;for(auto j=k-2;j<=k+2;++j){auto candidate=map.sampleAt(j*step+(j%2?s.swing*step:0))-start+(noori::Sample)std::llround(s.offsetMs*.001*map.sampleRate());auto d=std::abs(double(candidate-a.source));if(d<distance){desired=candidate;distance=d;}}if(a.target!=desired)++limited;}}
 r.message=limited?juce::String(limited)+" attacks limited by stretch safety | other attacks on grid | ready":clicks.empty()?"BPM alignment rendered | ready":"Exact reference clicks matched | ready";
 r.revision.audio=std::make_shared<noori::Audio>(noori::render(*input,r.revision.attacks,s,[this](float p){progress=.35f+.65f*p;return !cancelled.load();}));retainedBytes+=bytes;return r;});
}
void EditSession::move(std::size_t index,noori::Sample target){if(original&&!busy)try{renderMarkers(noori::moveAttack(attacks,index,target,original->length(),settings.maxStretch));}catch(const std::exception& e){status=juce::String::fromUTF8(e.what());}}
void EditSession::setSelected(std::size_t i,bool additive){if(busy||i>=attacks.size())return;for(std::size_t j=0;j<attacks.size();++j)attacks[j].selected=j==i|| (additive&&attacks[j].selected);}
void EditSession::reset(){if(original&&!busy){publish({{},original});compare=false;status="Reset edits | undo available";if(onChanged)onChanged();}}
bool EditSession::canUndo()const{return !busy&&current>0;}bool EditSession::canRedo()const{return !busy&&current+1<(int)history.size();}
void EditSession::undo(){if(canUndo()){--current;compare=false;status="Undo | revision "+juce::String(current);attacks=history[(std::size_t)current].attacks;audible=history[(std::size_t)current].audio.get();if(onChanged)onChanged();}}
void EditSession::redo(){if(canRedo()){++current;compare=false;status="Redo | revision "+juce::String(current);attacks=history[(std::size_t)current].attacks;audible=history[(std::size_t)current].audio.get();if(onChanged)onChanged();}}
void EditSession::cancel(){cancelled=true;}
juce::MemoryBlock EditSession::archive(bool includeAudio)const {
 juce::MemoryOutputStream out;out.writeInt(0x4e545431);writeSettings(out,settings);out.writeString(name);out.writeInt64(origin);out.writeString(!original&&!restoreMarkers.empty()?restoreMarkers:noori::serializeAttacks(attacks));out.writeDouble(timeline.sampleRate());out.writeInt((int)timeline.points().size());for(auto t:timeline.points()){out.writeInt64(t.sample);out.writeDouble(t.bpm);out.writeDouble(t.quarter);}
 out.writeBool(includeAudio&&original!=nullptr);
 if(includeAudio&&original){out.writeDouble(original->sampleRate);out.writeInt((int)original->channels.size());out.writeInt64(original->length());for(const auto& c:original->channels)for(float x:c)out.writeFloat(x);}
 out.writeBool(useReference);out.writeString(referenceName);out.writeInt((int)referenceClicks.size());for(auto click:referenceClicks)out.writeInt64(click);
 return out.getMemoryBlock();
}
void EditSession::restore(const void* data,std::size_t size,bool includeAudio) {
 if(busy)return;
 try {
  referenceClicks.clear();referenceName.clear();useReference=false;
  juce::MemoryInputStream in(data,size,false);if(in.readInt()!=0x4e545431)throw std::runtime_error("Unsupported project state");auto restoredSettings=readSettings(in);auto title=in.readString();auto start=in.readInt64();auto markers=in.readString().toStdString();auto sr=in.readDouble();int count=in.readInt();if(count<=0||count>16384)throw std::runtime_error("Invalid tempo archive");std::vector<noori::TempoPoint> points;for(int i=0;i<count;++i){noori::TempoPoint p;p.sample=in.readInt64();p.bpm=in.readDouble();p.quarter=in.readDouble();points.push_back(p);}noori::Timeline map(sr,points);
  bool hasAudio=in.readBool();settings=restoredSettings;restoreMarkers=markers;name=title;origin=start;timeline=map;
  if(hasAudio&&includeAudio) {
   auto a=std::make_shared<noori::Audio>();a->sampleRate=in.readDouble();int channels=in.readInt();auto n=in.readInt64();if(channels<1||channels>2||n<1||n>128*1024*1024/(channels*4)||in.getNumBytesRemaining()<n*channels*4)throw std::runtime_error("Invalid audio archive");a->channels.resize((std::size_t)channels);for(auto& c:a->channels){c.resize((std::size_t)n);for(auto& x:c)x=in.readFloat();}load([a]{return *a;},title,start,map);
  }
  if(in.getNumBytesRemaining()>0){useReference=in.readBool();referenceName=in.readString();int n=in.readInt();if(n<0||n>1000000||in.getNumBytesRemaining()<n*8ll)throw std::runtime_error("Invalid reference archive");referenceClicks.clear();for(int i=0;i<n;++i)referenceClicks.push_back(in.readInt64());if(!std::is_sorted(referenceClicks.begin(),referenceClicks.end()))throw std::runtime_error("Unordered reference clicks");}
 }catch(const std::exception& e){status=juce::String::fromUTF8(e.what());}
}
NooriProcessor::NooriProcessor():AudioProcessor(BusesProperties().withInput("Vocal input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),insertSession(std::make_shared<EditSession>()){}
NooriProcessor::~NooriProcessor(){captureArmed=false;}
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
 hostTempoValid=valid&&pos.getBpm().hasValue();bpm=pos.getBpm().orFallback(80);quarter=pos.getPpqPosition().orFallback(0);hostSample=pos.getTimeInSamples().orFallback(0);playing=pos.getIsPlaying();auto sig=pos.getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature{});numerator=sig.numerator;denominator=sig.denominator;
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
void NooriProcessor::beginCapture(){if(insertSession->busy)return;suspendProcessing(true);captureCount=0;tempoCount=0;captureStopped=false;audition=false;captureArmed=true;suspendProcessing(false);}
void NooriProcessor::finishCapture(){suspendProcessing(true);captureArmed=false;auto n=captureCount.load();if(n>0){auto a=std::make_shared<noori::Audio>();a->sampleRate=deviceRate;a->channels.resize(capture.channels.size());for(std::size_t c=0;c<a->channels.size();++c)a->channels[c].assign(capture.channels[c].begin(),capture.channels[c].begin()+n);std::vector<noori::TempoPoint> map(capturedTempo.begin(),capturedTempo.begin()+(std::ptrdiff_t)tempoCount);insertSession->load([a]{return *a;},"Captured vocal",captureOrigin,noori::Timeline(deviceRate,map));}suspendProcessing(false);}
juce::String NooriProcessor::captureStatus()const {return captureStopped?"Capture paused: seek, loop, tempo capacity, or 120 s limit. Press Finish.":"Capturing when Cubase plays: "+juce::String(captureCount.load()/deviceRate,1)+" s";}
void NooriProcessor::importFile(const juce::File& file){auto start=hostSample.load();auto rate=deviceRate;auto b=bpm.load();auto q=quarter.load();insertSession->load([file,rate]{juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(file));if(!r)throw std::runtime_error("Cannot read this audio file");if(r->numChannels<1||r->numChannels>2||r->lengthInSamples<1||r->lengthInSamples*(juce::int64)r->numChannels*4>128*1024*1024)throw std::runtime_error("Use a mono/stereo file smaller than 128 MiB decoded");if(std::abs(r->sampleRate-rate)>.1)throw std::runtime_error("File sample rate must match the Cubase project");juce::AudioBuffer<float> buffer((int)r->numChannels,(int)r->lengthInSamples);if(!r->read(&buffer,0,buffer.getNumSamples(),0,true,true))throw std::runtime_error("Audio read failed");noori::Audio a;a.sampleRate=r->sampleRate;for(int c=0;c<buffer.getNumChannels();++c)a.channels.emplace_back(buffer.getReadPointer(c),buffer.getReadPointer(c)+buffer.getNumSamples());return a;},file.getFileName(),start,noori::Timeline(rate,{{start,b,q}}));}
void NooriProcessor::exportFile(const juce::File& file,std::shared_ptr<EditSession> s){if(!s->audible.load()||s->busy)return;auto* a=s->audible.load();juce::WavAudioFormat format;juce::TemporaryFile temporary(file);auto stream=temporary.getFile().createOutputStream();if(!stream){s->status="Cannot create export file";return;}std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),a->sampleRate,(unsigned)a->channels.size(),24,{},0));if(!writer){s->status="Cannot create 24-bit WAV writer";return;}std::vector<const float*> ptr;for(const auto& c:a->channels)ptr.push_back(c.data());bool ok=writer->writeFromFloatArrays(ptr.data(),(int)ptr.size(),(int)a->length());writer.reset();s->status=ok&&temporary.overwriteTargetFileWithTemporary()?"Exported 24-bit WAV":"WAV write failed";}
juce::AudioProcessorEditor* NooriProcessor::createEditor(){return new NooriEditor(*this);}
void NooriProcessor::getStateInformation(juce::MemoryBlock& block){block=insertSession->archive(true);}
void NooriProcessor::setStateInformation(const void* data,int bytes){if(bytes>0)insertSession->restore(data,(std::size_t)bytes,true);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NooriProcessor;}
