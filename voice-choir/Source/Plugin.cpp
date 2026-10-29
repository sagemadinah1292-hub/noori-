#include "Plugin.h"
#include "Editor.h"
#include "RenderMemory.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace juce;
using namespace noori_choir;
namespace {
std::uint64_t currentRenderBudget(std::uint64_t resident){
#if JUCE_WINDOWS
 MEMORYSTATUSEX info{};info.dwLength=sizeof(info);if(GlobalMemoryStatusEx(&info))return renderBudgetForAvailable(info.ullAvailPhys,resident);
#endif
 return renderBudgetForAvailable(std::uint64_t(SystemStats::getMemorySizeInMegabytes())*renderMiB/2,resident);
}
std::uint64_t retainedStemBytes(const ChoirSession& s){std::uint64_t bytes=0;for(const auto& stem:s.stems)for(const auto* file:{stem.dry.get(),stem.wet.get()})if(file)bytes+=std::uint64_t(file->audio.getNumSamples())*file->audio.getNumChannels()*sizeof(float);return bytes;}
}
AudioProcessorValueTreeState::ParameterLayout ChoirProcessor::layout(){
 AudioProcessorValueTreeState::ParameterLayout p;
 auto integer=[&](const char* id,const char* name,int low,int high,int def){p.add(std::make_unique<AudioParameterInt>(ParameterID{id,1},name,low,high,def));};
 auto number=[&](const char* id,const char* name,float low,float high,float def){p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(low,high,.01f),def));};
 auto toggle=[&](const char* id,const char* name,bool def){p.add(std::make_unique<AudioParameterBool>(ParameterID{id,1},name,def));};
 integer("voices","Number of singers",2,16,8);integer("character","Voice character DSP",0,7,3);integer("microphone","Microphone character",0,4,0);
 integer("key","Key",0,11,7);integer("scale","Scale",0,4,0);integer("customMask","Custom scale pitch classes",1,4095,2741);number("detune","Performance variation cents",0,20,6);
 for(auto id:{"octaveUp","octaveDown","thirdAbove","thirdBelow","fifthAbove","fifthBelow"})toggle(id,id,true);
 toggle("autoKey","Auto detect key and scale",true);toggle("stereo","Stereo output",true);number("masterGain","Preview master gain dB",-36,6,-6);
 number("lowCut","Lo Cut Hz",0,500,40);number("brightness","Brightness dB",-12,12,0);number("reverb","Reverb",0,1,.12f);
 toggle("printFx","Print Post FX",true);toggle("floatWav","32 bit float export",false);toggle("bwf","BWF source timestamp",true);
 return p;
}
ChoirProcessor::ChoirProcessor(noori_licensing::Config licenseConfig):AudioProcessor(BusesProperties().withInput("Vocal",AudioChannelSet::stereo(),true).withOutput("Choir Preview",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NooriVoiceChoir",layout()),licensing(std::move(licenseConfig)){master=state.getRawParameterValue("masterGain");for(auto id:{"lowCut","brightness","reverb","printFx","floatWav","bwf"})state.addParameterListener(id,this);}
ChoirProcessor::~ChoirProcessor(){licensing.shutdown();for(auto id:{"lowCut","brightness","reverb","printFx","floatWav","bwf"})state.removeParameterListener(id,this);shuttingDown=true;capturing=false;cancelled=true;{const ScopedLock lock(dataLock);pendingJob={};}if(worker.joinable())worker.join();}
float ChoirProcessor::value(const char* id)const{auto* v=state.getRawParameterValue(id);return v?v->load():0.f;}
void ChoirProcessor::setValue(const String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
ChoirConfig ChoirProcessor::config()const{ChoirConfig c;c.singers=int(value("voices"));c.character=int(value("character"));c.microphone=int(value("microphone"));c.key=int(value("key"));c.scale=int(value("scale"));c.customMask=int(value("customMask"));c.variation=value("detune");c.autoKey=value("autoKey")>.5f;c.octaveUp=value("octaveUp")>.5f;c.octaveDown=value("octaveDown")>.5f;c.thirdAbove=value("thirdAbove")>.5f;c.thirdBelow=value("thirdBelow")>.5f;c.fifthAbove=value("fifthAbove")>.5f;c.fifthBelow=value("fifthBelow")>.5f;c.stereo=value("stereo")>.5f;return c;}
FxConfig ChoirProcessor::fxConfig()const{return {value("lowCut"),value("brightness"),value("reverb"),value("printFx")>.5f};}
void ChoirProcessor::prepareToPlay(double rate,int){capturing=false;captureReady=false;hostRate=rate;setLatencySamples(0);stop();previewReverb.setSampleRate(rate);previewReverb.reset();fxX.fill(0);fxY.fill(0);fxLow.fill(0);cutSmooth.reset(rate,.025);brightSmooth.reset(rate,.025);wetSmooth.reset(rate,.025);cutSmooth.setCurrentAndTargetValue(value("lowCut"));brightSmooth.setCurrentAndTargetValue(value("brightness"));wetSmooth.setCurrentAndTargetValue(value("reverb"));}
bool ChoirProcessor::isBusesLayoutSupported(const BusesLayout& l)const{auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();return (in==AudioChannelSet::mono()||in==AudioChannelSet::stereo())&&(out==in||out==AudioChannelSet::stereo());}
void ChoirProcessor::processBlock(AudioBuffer<float>& buffer,MidiBuffer&){
 ScopedNoDenormals guard;const int frames=buffer.getNumSamples();if(frames<=0||buffer.getNumChannels()==0)return;audioBlockCount.fetch_add(1,std::memory_order_relaxed);
 if(!licensing.canUse()){capturing=false;playbackMode=-3;if(getTotalNumInputChannels()==1&&buffer.getNumChannels()>1)buffer.copyFrom(1,0,buffer,0,0,frames);return;}
 if(capturing.load()){
  const ScopedTryLock lock(captureLock);
  if(lock.isLocked()&&capturing.load()){
   const int offset=capturedFrames.load();if(offset==0){captureOffset=0;if(auto* ph=getPlayHead())if(auto pos=ph->getPosition())if(auto time=pos->getTimeInSamples())captureOffset=*time;}
   const int n=std::min(frames,capture.getNumSamples()-offset);for(int ch=0;ch<captureChannels;++ch)capture.copyFrom(ch,offset,buffer,ch,0,n);capturedFrames=offset+n;
   if(offset+n>=capture.getNumSamples()){capturing=false;captureReady=true;}
  }
 }
 if(getTotalNumInputChannels()==1&&buffer.getNumChannels()>1)buffer.copyFrom(1,0,buffer,0,0,frames);
 if(playbackMode.load()==-3)return;
 const ScopedTryLock lock(previewLock);if(!lock.isLocked()||!preview||!preview->source)return;
 if(rewind.exchange(false)){cursor=0;previewReverb.reset();fxX.fill(0);fxY.fill(0);fxLow.fill(0);}
 const int mode=playbackMode.load();if(mode==-3)return;
 buffer.clear();const int length=preview->source->audio.getNumSamples();const double increment=preview->source->rate/hostRate.load();
 const bool sourceMode=mode==-2;bool anySolo=false;for(int v=0;v<int(preview->stems.size());++v)anySolo=anySolo||mixes[v].solo.load();
 const float masterGain=std::pow(10.f,master->load()/20.f);const float normalise=sourceMode||mode>=0?1.f:1.f/float(std::max(1,int(preview->stems.size())));
 std::array<float,16> gainCache{},leftCache{},rightCache{};
 for(int v=0;v<int(preview->stems.size());++v){gainCache[v]=std::pow(10.f,mixes[v].gain.load()/20.f);const float pan=mixes[v].pan.load();leftCache[v]=std::sqrt(std::clamp(1-pan,0.f,1.f));rightCache[v]=std::sqrt(std::clamp(1+pan,0.f,1.f));}
 for(int i=0;i<frames&&cursor<length;++i,cursor+=increment){const int a=int(cursor),b=std::min(a+1,length-1);const float frac=float(cursor-a);
  const int count=sourceMode?1:int(preview->stems.size());for(int v=0;v<count;++v){if(!sourceMode&&((mode>=0&&mode!=v)||mixes[v].mute.load()||(mode==-1&&anySolo&&!mixes[v].solo.load())))continue;
   const auto& audio=sourceMode?preview->source->audio:preview->stems[v].dry->audio;
   const float gain=sourceMode?(sourceMuted?0.f:1.f):gainCache[v];
   for(int ch=0;ch<buffer.getNumChannels();++ch){const int inputCh=std::min(ch,audio.getNumChannels()-1);float x=audio.getSample(inputCh,a)+frac*(audio.getSample(inputCh,b)-audio.getSample(inputCh,a));
    if(buffer.getNumChannels()==1&&audio.getNumChannels()==2){float y=audio.getSample(1,a)+frac*(audio.getSample(1,b)-audio.getSample(1,a));x=.5f*(x+y);}
    float balance=sourceMode||buffer.getNumChannels()==1?1.f:(ch==0?leftCache[v]:rightCache[v]);buffer.addSample(ch,i,x*gain*balance*masterGain*normalise);
   }
  }
 }
 if(!sourceMode)processPreviewFx(buffer);
 previewPeak=buffer.getMagnitude(0,frames);previewFrame=cursor;if(cursor>=length)playbackMode=-3;
}
void ChoirProcessor::processPreviewFx(AudioBuffer<float>& buffer){
 cutSmooth.setTargetValue(value("lowCut"));brightSmooth.setTargetValue(value("brightness"));wetSmooth.setTargetValue(value("reverb"));
 const auto rate=hostRate.load();const double lp=std::exp(-2*pi*2500/rate);
 // Only bounded, preallocated DSP runs here; full stem renders stay on the worker.
 for(int offset=0;offset<buffer.getNumSamples();offset+=32){const int count=std::min(32,buffer.getNumSamples()-offset);
  float wet=0;
  for(int i=offset;i<offset+count;++i){const float cut=cutSmooth.getNextValue();const double hp=std::exp(-2*pi*cut/rate),gain=std::pow(10.,brightSmooth.getNextValue()/20.);wet=wetSmooth.getNextValue();
   for(int ch=0;ch<std::min(2,buffer.getNumChannels());++ch){const double x=buffer.getSample(ch,i),high=cut>0?hp*(fxY[ch]+x-fxX[ch]):x;fxX[ch]=x;fxY[ch]=high;fxLow[ch]=lp*fxLow[ch]+(1-lp)*high;buffer.setSample(ch,i,float(fxLow[ch]+(high-fxLow[ch])*gain));}
  }
  Reverb::Parameters params;params.roomSize=.55f;params.damping=.5f;params.wetLevel=std::clamp(wet,0.f,1.f)*.35f;params.dryLevel=1;params.width=1;previewReverb.setParameters(params);
  if(buffer.getNumChannels()==1)previewReverb.processMono(buffer.getWritePointer(0,offset),count);else previewReverb.processStereo(buffer.getWritePointer(0,offset),buffer.getWritePointer(1,offset),count);
 }
}
void ChoirProcessor::play(int voice){
 if(!requireLicense())return;const auto s=session();String error;
 if(isCapturing())error="Stop capture before starting preview.";
 else if(!s||!s->source)error="Import a vocal before starting preview.";
 else if(voice==-1&&s->stems.empty())error="Create vocals before using Play All.";
 else if(voice< -2||voice>=int(s->stems.size()))error="That preview voice is unavailable.";
 if(error.isNotEmpty()){const ScopedLock lock(dataLock);status=error;return;}
 rewind=true;playbackMode=voice;const ScopedLock lock(dataLock);status="Preview playing through the Cubase track output.";
}
void ChoirProcessor::stop(){previewPeak=0;playbackMode=-3;rewind=true;previewFrame=0;}
std::shared_ptr<const ChoirSession> ChoirProcessor::session()const{const ScopedLock lock(dataLock);return current;}
bool ChoirProcessor::hasSource()const{auto s=session();return s&&s->source;}
int ChoirProcessor::stemCount()const{auto s=session();return s?int(s->stems.size()):0;}
String ChoirProcessor::jobStatus()const{const ScopedLock lock(dataLock);return status;}
StringArray ChoirProcessor::exportedFiles()const{const ScopedLock lock(dataLock);return exports;}
StringArray ChoirProcessor::dragFiles()const{const ScopedLock lock(dataLock);return !licensing.canUse()||stemsNeedPreparation()?StringArray{}:dropFiles;}
StemMix ChoirProcessor::stemMix(int v)const{if(v<0||v>=16)return {};return {mixes[v].gain.load(),mixes[v].pan.load(),mixes[v].mute.load(),mixes[v].solo.load()};}
void ChoirProcessor::setStemMix(int v,StemMix m){if(v<0||v>=16)return;mixes[v].gain=std::clamp(m.gain,-60.f,12.f);mixes[v].pan=std::clamp(m.pan,-1.f,1.f);mixes[v].mute=m.mute;mixes[v].solo=m.solo;invalidateStems();}
std::vector<StemMix> ChoirProcessor::mixSnapshot(int n)const{std::vector<StemMix> result;for(int i=0;i<n;++i)result.push_back(stemMix(i));return result;}
bool ChoirProcessor::requireLicense(){if(licensing.canUse())return true;const auto information=licensing.snapshot();const ScopedLock lock(dataLock);status=information.message;return false;}
void ChoirProcessor::runJob(std::function<void()> fn,bool replace){
 {const ScopedLock lock(dataLock);if(busy){if(replace){pendingJob=std::move(fn);cancelled=true;}return;}busy=true;cancelled=false;progress=0;status="Working...";}
 if(worker.joinable())worker.join();
 try{worker=std::thread([this,task=std::move(fn)]()mutable{
  for(;;){
   try{task();}catch(const std::bad_alloc&){const ScopedLock lock(dataLock);status="Not enough available RAM to finish. Reduce singers, select Mono, or free RAM and retry.";}catch(const std::exception& e){const ScopedLock lock(dataLock);status=String::fromUTF8(e.what());}catch(...){const ScopedLock lock(dataLock);status="Audio operation failed.";}
   const ScopedLock lock(dataLock);
   if(pendingJob&&!shuttingDown){task=std::move(pendingJob);pendingJob={};cancelled=false;progress=0;status="Restoring saved project audio...";}
   else {busy=false;break;}
  }
 });}catch(...){const ScopedLock lock(dataLock);busy=false;status="Cannot start audio worker.";}
}
void ChoirProcessor::publish(std::shared_ptr<ChoirSession> s,bool resetMix,bool preservePlayback){
 if(!preservePlayback)stop();if(resetMix)sourceMuted=false;if(resetMix)for(int i=0;i<16;++i){auto m=StemMix{};if(s->config.stereo&&s->stems.size()>1)m.pan=float(-.75+1.5*i/std::max(1,int(s->stems.size())-1));setStemMix(i,m);}
 {const ScopedLock lock(dataLock);current=s;exports.clear();dropFiles.clear();dragDirty=true;}
 {const ScopedLock lock(previewLock);preview=std::move(s);}
 if(!preservePlayback)++dataRevision;
}
void ChoirProcessor::cacheSession(ChoirSession& s){
 if(s.cacheId.isEmpty())s.cacheId=Uuid().toString();auto dir=cacheRoot().getChildFile(s.cacheId);
 if(dir.createDirectory().failed())throw std::runtime_error("Cannot create persistent audio cache.");
 try{writeWave(dir.getChildFile("Source.wav"),*s.source,32,true,cancelled);XmlElement manifest("NooriCache");manifest.setAttribute("sourceName",s.source->name);manifest.setAttribute("originalPath",s.source->originalPath);
  for(int i=0;i<int(s.stems.size());++i){const auto& stem=s.stems[i];const auto name="Voice-"+String(i)+".wav";writeWave(dir.getChildFile(name),*stem.dry,32,true,cancelled);auto* e=manifest.createNewChildElement("Stem");e->setAttribute("name",stem.name);e->setAttribute("part",int(stem.part));e->setAttribute("singer",stem.singer);e->setAttribute("file",name);}
  if(!manifest.writeTo(dir.getChildFile("Session.xml")))throw std::runtime_error("Cannot save audio cache manifest.");
 }catch(...){dir.deleteRecursively();throw;}
}
void ChoirProcessor::prepareDrop(const ChoirSession& s){if(!licensing.canUse())return;
 auto folder=cacheRoot().getChildFile("DragFiles");if(folder.createDirectory().failed())throw std::runtime_error("Cannot prepare drag files.");
 const auto revision=mixRevision.load();auto files=exportSession(s,folder,mixSnapshot(int(s.stems.size())),value("floatWav")>.5f?32:24,value("printFx")>.5f,value("bwf")>.5f,-1,cancelled,progress);
 const ScopedLock lock(dataLock);dropFiles=std::move(files);preparedMixRevision=revision;dragDirty=false;
}
void ChoirProcessor::importWav(File file){if(isCapturing())return;stop();runJob([this,file]{auto s=std::make_shared<ChoirSession>();s->source=std::make_shared<VocalFile>(readWav(file,cancelled));s->config=config();s->fx=fxConfig();cacheSession(*s);publish(s,true);progress=1;const ScopedLock lock(dataLock);status="Vocal loaded. Choose choir parts and Create Vocals.";});}
String ChoirProcessor::generationMemorySummary()const{
 auto s=session();if(!s||!s->source)return "Import a vocal before creating voices.";
 const auto c=config();const auto retained=retainedStemBytes(*s);const auto resident=std::uint64_t(s->source->audio.getNumSamples())*s->source->audio.getNumChannels()*sizeof(float)+retained;const auto bytes=renderWorkingBytes(s->source->audio.getNumSamples(),s->source->audio.getNumChannels(),c.stereo?2:1,c.singers,retained);
 return "Estimated render RAM: "+String(double(bytes)/renderMiB,0)+" MiB | safe budget: "+String(double(currentRenderBudget(resident))/renderMiB,0)+" MiB";
}
void ChoirProcessor::generate(int regenerate){if(!requireLicense())return;if(isCapturing())return;const auto settings=config();const auto fx=fxConfig();stop();runJob([this,settings,fx,regenerate]{auto c=settings;
 auto old=session();if(!old||!old->source)throw std::runtime_error("Import or capture a vocal first.");
 const auto roles=regenerate>=0?std::vector<Part>{old->stems.at(std::size_t(regenerate)).part}:arrange(c);
 const auto& source=*old->source;const int frames=source.audio.getNumSamples(),channels=c.stereo?2:1;
 const auto required=renderWorkingBytes(frames,source.audio.getNumChannels(),channels,int(roles.size()),retainedStemBytes(*old));
 const auto resident=std::uint64_t(frames)*source.audio.getNumChannels()*sizeof(float)+retainedStemBytes(*old);const auto budget=currentRenderBudget(resident);
 if(required>budget)throw std::runtime_error(("Render needs about "+String(double(required)/renderMiB,0)+" MiB; current safe budget is "+String(double(budget)/renderMiB,0)+" MiB. Reduce singers, select Mono, shorten the vocal or free RAM.").toStdString());
 auto update=[this](String message,float amount){progress=amount;const ScopedLock lock(dataLock);status=std::move(message);};
 update("Analyzing vocal melody...",0);std::vector<float> mono(frames);for(int i=0;i<frames;++i){double x=0;for(int ch=0;ch<source.audio.getNumChannels();++ch)x+=source.audio.getSample(ch,i);mono[i]=float(x/source.audio.getNumChannels());}
 auto melody=analyseMelody(mono.data(),frames,source.rate,cancelled,[&](float amount){progress=.15f*amount;});
 melody=stableMelody(melody);String detected;
 if(c.autoKey){const auto estimate=estimateKey(melody);if(estimate.confidence>=.12f){c.key=estimate.key;c.scale=estimate.scale;detected="Auto key: "+String(std::array<const char*,12>{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}[c.key])+(c.scale==0?" major":" minor")+". ";}else {detected="Auto key uncertain: thirds/fifths use unison; choose a manual key for harmonies. ";}}
 auto result=std::make_shared<ChoirSession>();result->source=old->source;result->config=c;result->fx=fx;
 if(regenerate>=0)result->stems=old->stems;
 for(int v=0;v<int(roles.size());++v){const int singer=regenerate>=0?old->stems[regenerate].singer+17:v;
  update("Generating "+String(partName(roles[v]))+" ("+String(v+1)+"/"+String(roles.size())+")...",.15f+.7f*v/roles.size());
  auto renderPart=roles[v];if(c.autoKey&&detected.startsWith("Auto key uncertain")&&(renderPart==Part::ThirdAbove||renderPart==Part::ThirdBelow||renderPart==Part::FifthAbove||renderPart==Part::FifthBelow))renderPart=Part::Unison;
  auto data=renderSinger(mono.data(),frames,source.rate,melody,renderPart,singer,c,cancelled,[&](float amount){if(!licensing.canUse())throw std::runtime_error("Activation is no longer valid. Please check your key online.");progress=.15f+.7f*(v+amount)/roles.size();});
  auto dry=std::make_shared<VocalFile>();dry->rate=source.rate;dry->sampleOffset=source.sampleOffset;dry->name=String(partName(roles[v]));dry->audio.setSize(channels,frames);
  for(int ch=0;ch<channels;++ch){if(roles[v]==Part::Main&&c.stereo)dry->audio.copyFrom(ch,0,source.audio,std::min(ch,source.audio.getNumChannels()-1),0,frames);else std::copy(data.begin(),data.end(),dry->audio.getWritePointer(ch));}
  Stem stem{String(partName(roles[v]))+(renderPart!=roles[v]?" - Auto Unison":""),roles[v],singer,dry,postProcess(*dry,fx,cancelled)};
  if(regenerate>=0)result->stems[regenerate]=std::move(stem);else result->stems.push_back(std::move(stem));
 }
 update("Saving voices and preparing stem files...",.88f);cacheSession(*result);publish(result,regenerate<0);prepareDrop(*result);progress=1;const ScopedLock lock(dataLock);status=detected+"Vocals ready. Audition the parts, adjust Post FX, or drag stems to Cubase.";
 });}
void ChoirProcessor::applyFx(){if(!requireLicense())return;const auto fx=fxConfig();runJob([this,fx]{auto old=session();if(!old||old->stems.empty())return;auto result=std::make_shared<ChoirSession>(*old);result->fx=fx;for(int i=0;i<int(result->stems.size());++i){result->stems[i].wet=postProcess(*result->stems[i].dry,fx,cancelled);progress=.8f*(i+1)/result->stems.size();}publish(result,false,true);const ScopedLock lock(dataLock);status="Post FX updated. Playback continues; prepare stems when ready to drag.";});}
void ChoirProcessor::prepareStems(){if(!requireLicense())return;runJob([this]{auto s=session();if(!s||s->stems.empty())throw std::runtime_error("Create vocals first.");auto rendered=std::make_shared<ChoirSession>(*s);rendered->fx=fxConfig();for(auto& stem:rendered->stems)stem.wet=postProcess(*stem.dry,rendered->fx,cancelled);prepareDrop(*rendered);const ScopedLock lock(dataLock);status="Stem files ready. Drag STEMS to Cubase.";});}
void ChoirProcessor::exportTo(File project,int selected){if(!requireLicense())return;const int bits=value("floatWav")>.5f?32:24;const bool print=value("printFx")>.5f,bwf=value("bwf")>.5f;const auto fx=fxConfig();
 runJob([this,project,selected,bits,print,bwf,fx]{auto s=session();if(!s||s->stems.empty())throw std::runtime_error("Create vocals before export.");if(!project.isDirectory())throw std::runtime_error("Choose the existing Cubase project directory.");auto folder=project.getChildFile("NOORI CHOIR");if(folder.createDirectory().failed())throw std::runtime_error("Cannot create NOORI CHOIR output directory.");auto rendered=std::make_shared<ChoirSession>(*s);if(print){rendered->fx=fx;for(auto& stem:rendered->stems)stem.wet=postProcess(*stem.dry,fx,cancelled);}auto files=exportSession(*rendered,folder,mixSnapshot(int(s->stems.size())),bits,print,bwf,selected,cancelled,progress);const ScopedLock lock(dataLock);exports=std::move(files);status="Exported "+String(exports.size())+" WAV file(s) to "+folder.getFullPathName();});}
void ChoirProcessor::clearSource(){if(isBusy()||isCapturing())return;stop();{const ScopedLock lock(dataLock);current.reset();exports.clear();dropFiles.clear();status="Drop a vocal or browse files to begin.";}{const ScopedLock lock(previewLock);preview.reset();}dragDirty=true;++dataRevision;}
void ChoirProcessor::removeStem(int index){if(isBusy())return;auto old=session();if(!old||index<0||index>=int(old->stems.size()))return;auto s=std::make_shared<ChoirSession>(*old);s->stems.erase(s->stems.begin()+index);s->cacheId.clear();for(int i=index;i<int(s->stems.size());++i)setStemMix(i,stemMix(i+1));runJob([this,s]{cacheSession(*s);publish(s);if(!s->stems.empty())prepareDrop(*s);});}
void ChoirProcessor::startCapture(){if(!requireLicense())return;if(isBusy()||isCapturing())return;stop();const ScopedLock lock(captureLock);captureChannels=getTotalNumInputChannels();capture.setSize(captureChannels,int(hostRate.load()*120));capturedFrames=0;captureReady=false;capturing=true;const ScopedLock data(dataLock);status="Capturing track input. Play the vocal in Cubase, then Stop Capture (maximum 120 seconds).";}
void ChoirProcessor::stopCapture(){{const ScopedLock lock(captureLock);capturing=false;captureReady=false;}if(isBusy())return;runJob([this]{const int n=capturedFrames.load();if(n<=0)throw std::runtime_error("Capture is empty. Play the track while capturing.");auto audio=std::make_shared<VocalFile>();audio->rate=hostRate.load();audio->name="Captured Vocal";audio->sampleOffset=captureOffset;{const ScopedLock lock(captureLock);audio->audio.setSize(captureChannels,n);for(int ch=0;ch<captureChannels;++ch)audio->audio.copyFrom(ch,0,capture,ch,0,n);capture.setSize(0,0);}auto s=std::make_shared<ChoirSession>();s->source=audio;s->config=config();s->fx=fxConfig();cacheSession(*s);publish(s,true);progress=1;const ScopedLock lock(dataLock);status="Capture ready. Choose parts and Create Vocals.";});}
void ChoirProcessor::getStateInformation(MemoryBlock& block){auto tree=state.copyState();tree.setProperty("darkMode",darkMode.load(),nullptr);tree.setProperty("uiScale",uiScale.load(),nullptr);auto s=session();tree.setProperty("cacheId",s?s->cacheId:String{},nullptr);ValueTree levels("StemMixes");for(int i=0;i<16;++i){auto m=stemMix(i);ValueTree level("Stem");level.setProperty("gain",m.gain,nullptr);level.setProperty("pan",m.pan,nullptr);level.setProperty("mute",m.mute,nullptr);level.setProperty("solo",m.solo,nullptr);levels.appendChild(level,nullptr);}tree.removeChild(tree.getChildWithName("StemMixes"),nullptr);tree.appendChild(levels,nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,block);}
void ChoirProcessor::setStateInformation(const void* data,int bytes){if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(state.state.getType())){auto tree=ValueTree::fromXml(*xml);if(!tree.getChildWithProperty("id","autoKey").isValid()){ValueTree choice("PARAM");choice.setProperty("id","autoKey",nullptr);choice.setProperty("value",0,nullptr);tree.addChild(choice,-1,nullptr);}darkMode=bool(tree.getProperty("darkMode",true));uiScale=std::clamp(int(tree.getProperty("uiScale",0)),0,3);state.replaceState(tree);std::vector<StemMix> levels;auto saved=tree.getChildWithName("StemMixes");for(int i=0;i<16;++i){auto m=saved.getChild(i);levels.push_back({float(m.getProperty("gain",0)),float(m.getProperty("pan",0)),bool(m.getProperty("mute",false)),bool(m.getProperty("solo",false))});}auto id=tree.getProperty("cacheId").toString();stop();if(id.isNotEmpty())restoreCache(id,levels);}}
void ChoirProcessor::restoreCache(String id,std::vector<StemMix> levels){runJob([this,id,levels]{if((id.length()!=32&&id.length()!=36)||!id.containsOnly("0123456789abcdefABCDEF-"))throw std::runtime_error("Invalid saved audio cache ID.");auto folder=cacheRoot().getChildFile(id);auto manifest=XmlDocument::parse(folder.getChildFile("Session.xml"));if(!manifest||!manifest->hasTagName("NooriCache"))throw std::runtime_error("Saved audio cache is missing. Reimport the original vocal and regenerate.");auto s=std::make_shared<ChoirSession>();auto source=std::make_shared<VocalFile>(readWav(folder.getChildFile("Source.wav"),cancelled));source->name=manifest->getStringAttribute("sourceName");source->originalPath=manifest->getStringAttribute("originalPath");s->source=source;s->cacheId=id;s->config=config();s->fx=fxConfig();
 for(auto* e:manifest->getChildIterator()){if(!e->hasTagName("Stem"))continue;if(s->stems.size()>=16)throw std::runtime_error("Invalid saved voice count.");const int index=int(s->stems.size());auto raw=std::make_shared<VocalFile>(readWav(folder.getChildFile("Voice-"+String(index)+".wav"),cancelled));if(raw->rate!=source->rate||raw->audio.getNumSamples()!=source->audio.getNumSamples())throw std::runtime_error("Saved stem has mismatched duration or sample rate.");auto part=Part(std::clamp(e->getIntAttribute("part"),0,7));s->stems.push_back({e->getStringAttribute("name"),part,e->getIntAttribute("singer"),raw,postProcess(*raw,s->fx,cancelled)});}
 publish(s);for(int i=0;i<int(levels.size());++i)setStemMix(i,levels[i]);if(!s->stems.empty())prepareDrop(*s);progress=1;const ScopedLock lock(dataLock);status="Project audio restored from persistent cache.";
 },true);}
AudioProcessorEditor* ChoirProcessor::createEditor(){return new ChoirEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChoirProcessor;}
