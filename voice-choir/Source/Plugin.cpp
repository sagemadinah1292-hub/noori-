#include "Plugin.h"
#include "Editor.h"
using namespace juce;
using namespace noori_choir;
AudioProcessorValueTreeState::ParameterLayout ChoirProcessor::layout(){
 AudioProcessorValueTreeState::ParameterLayout p;
 auto integer=[&](const char* id,const char* name,int low,int high,int def){p.add(std::make_unique<AudioParameterInt>(ParameterID{id,1},name,low,high,def));};
 auto number=[&](const char* id,const char* name,float low,float high,float def){p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(low,high,.01f),def));};
 auto toggle=[&](const char* id,const char* name,bool def){p.add(std::make_unique<AudioParameterBool>(ParameterID{id,1},name,def));};
 integer("voices","Number of singers",2,16,8);integer("character","Voice character DSP",0,7,3);integer("microphone","Microphone character",0,4,0);
 integer("key","Key",0,11,7);integer("scale","Scale",0,4,0);integer("customMask","Custom scale pitch classes",1,4095,2741);number("detune","Performance variation cents",0,20,6);
 for(auto id:{"octaveUp","octaveDown","thirdAbove","thirdBelow","fifthAbove","fifthBelow"})toggle(id,id,true);
 toggle("stereo","Stereo output",true);number("masterGain","Preview master gain dB",-36,6,-6);
 number("lowCut","Lo Cut Hz",0,500,40);number("brightness","Brightness dB",-12,12,0);number("reverb","Reverb",0,1,.12f);
 toggle("printFx","Print Post FX",true);toggle("floatWav","32 bit float export",false);toggle("bwf","BWF source timestamp",true);
 return p;
}
ChoirProcessor::ChoirProcessor():AudioProcessor(BusesProperties().withInput("Vocal",AudioChannelSet::stereo(),true).withOutput("Choir Preview",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NooriVoiceChoir",layout()){master=state.getRawParameterValue("masterGain");for(auto id:{"lowCut","brightness","reverb","printFx","floatWav","bwf"})state.addParameterListener(id,this);}
ChoirProcessor::~ChoirProcessor(){for(auto id:{"lowCut","brightness","reverb","printFx","floatWav","bwf"})state.removeParameterListener(id,this);shuttingDown=true;capturing=false;cancelled=true;{const ScopedLock lock(dataLock);pendingJob={};}if(worker.joinable())worker.join();}
float ChoirProcessor::value(const char* id)const{auto* v=state.getRawParameterValue(id);return v?v->load():0.f;}
void ChoirProcessor::setValue(const String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
ChoirConfig ChoirProcessor::config()const{ChoirConfig c;c.singers=int(value("voices"));c.character=int(value("character"));c.microphone=int(value("microphone"));c.key=int(value("key"));c.scale=int(value("scale"));c.customMask=int(value("customMask"));c.variation=value("detune");c.octaveUp=value("octaveUp")>.5f;c.octaveDown=value("octaveDown")>.5f;c.thirdAbove=value("thirdAbove")>.5f;c.thirdBelow=value("thirdBelow")>.5f;c.fifthAbove=value("fifthAbove")>.5f;c.fifthBelow=value("fifthBelow")>.5f;c.stereo=value("stereo")>.5f;return c;}
FxConfig ChoirProcessor::fxConfig()const{return {value("lowCut"),value("brightness"),value("reverb"),value("printFx")>.5f};}
void ChoirProcessor::prepareToPlay(double rate,int){capturing=false;captureReady=false;hostRate=rate;setLatencySamples(0);stop();}
bool ChoirProcessor::isBusesLayoutSupported(const BusesLayout& l)const{auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();return (in==AudioChannelSet::mono()||in==AudioChannelSet::stereo())&&(out==in||out==AudioChannelSet::stereo());}
void ChoirProcessor::processBlock(AudioBuffer<float>& buffer,MidiBuffer&){
 ScopedNoDenormals guard;const int frames=buffer.getNumSamples();
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
 if(rewind.exchange(false))cursor=0;
 const int mode=playbackMode.load();if(mode==-3)return;
 buffer.clear();const int length=preview->source->audio.getNumSamples();const double increment=preview->source->rate/hostRate.load();
 const bool sourceMode=mode==-2;bool anySolo=false;for(int v=0;v<int(preview->stems.size());++v)anySolo=anySolo||mixes[v].solo.load();
 const float masterGain=std::pow(10.f,master->load()/20.f);const float normalise=sourceMode||mode>=0?1.f:1.f/std::sqrt(float(std::max(1,int(preview->stems.size()))));
 for(int i=0;i<frames&&cursor<length;++i,cursor+=increment){const int a=int(cursor),b=std::min(a+1,length-1);const float frac=float(cursor-a);
  const int count=sourceMode?1:int(preview->stems.size());for(int v=0;v<count;++v){if(!sourceMode&&((mode>=0&&mode!=v)||mixes[v].mute.load()||(mode==-1&&anySolo&&!mixes[v].solo.load())))continue;
   const auto& audio=sourceMode?preview->source->audio:preview->stems[v].wet->audio;
   const float gain=sourceMode?(sourceMuted?0.f:1.f):std::pow(10.f,mixes[v].gain.load()/20.f);const float pan=sourceMode?0.f:mixes[v].pan.load();
   for(int ch=0;ch<buffer.getNumChannels();++ch){const int inputCh=std::min(ch,audio.getNumChannels()-1);float x=audio.getSample(inputCh,a)+frac*(audio.getSample(inputCh,b)-audio.getSample(inputCh,a));
    if(buffer.getNumChannels()==1&&audio.getNumChannels()==2){float y=audio.getSample(1,a)+frac*(audio.getSample(1,b)-audio.getSample(1,a));x=.5f*(x+y);}
    float balance=buffer.getNumChannels()==1?1.f:std::sqrt(std::max(0.f,ch==0?1-pan:1+pan));buffer.addSample(ch,i,x*gain*balance*masterGain*normalise);
   }
  }
 }
 previewFrame=cursor;if(cursor>=length)playbackMode=-3;
}
void ChoirProcessor::play(int voice){if(isCapturing())return;const auto s=session();if(!s||!s->source||voice>=int(s->stems.size())||(voice==-1&&s->stems.empty()))return;rewind=true;playbackMode=voice;}
void ChoirProcessor::stop(){playbackMode=-3;rewind=true;previewFrame=0;}
std::shared_ptr<const ChoirSession> ChoirProcessor::session()const{const ScopedLock lock(dataLock);return current;}
bool ChoirProcessor::hasSource()const{auto s=session();return s&&s->source;}
int ChoirProcessor::stemCount()const{auto s=session();return s?int(s->stems.size()):0;}
String ChoirProcessor::jobStatus()const{const ScopedLock lock(dataLock);return status;}
StringArray ChoirProcessor::exportedFiles()const{const ScopedLock lock(dataLock);return exports;}
StringArray ChoirProcessor::dragFiles()const{const ScopedLock lock(dataLock);return stemsNeedPreparation()?StringArray{}:dropFiles;}
StemMix ChoirProcessor::stemMix(int v)const{if(v<0||v>=16)return {};return {mixes[v].gain.load(),mixes[v].pan.load(),mixes[v].mute.load(),mixes[v].solo.load()};}
void ChoirProcessor::setStemMix(int v,StemMix m){if(v<0||v>=16)return;mixes[v].gain=std::clamp(m.gain,-60.f,12.f);mixes[v].pan=std::clamp(m.pan,-1.f,1.f);mixes[v].mute=m.mute;mixes[v].solo=m.solo;invalidateStems();}
std::vector<StemMix> ChoirProcessor::mixSnapshot(int n)const{std::vector<StemMix> result;for(int i=0;i<n;++i)result.push_back(stemMix(i));return result;}
void ChoirProcessor::runJob(std::function<void()> fn,bool replace){
 {const ScopedLock lock(dataLock);if(busy){if(replace){pendingJob=std::move(fn);cancelled=true;}return;}busy=true;cancelled=false;progress=0;status="Working...";}
 if(worker.joinable())worker.join();
 try{worker=std::thread([this,task=std::move(fn)]()mutable{
  for(;;){
   try{task();}catch(const std::exception& e){const ScopedLock lock(dataLock);status=String::fromUTF8(e.what());}catch(...){const ScopedLock lock(dataLock);status="Audio operation failed.";}
   const ScopedLock lock(dataLock);
   if(pendingJob&&!shuttingDown){task=std::move(pendingJob);pendingJob={};cancelled=false;progress=0;status="Restoring saved project audio...";}
   else {busy=false;break;}
  }
 });}catch(...){const ScopedLock lock(dataLock);busy=false;status="Cannot start audio worker.";}
}
void ChoirProcessor::publish(std::shared_ptr<ChoirSession> s,bool resetMix){
 stop();if(resetMix)sourceMuted=false;if(resetMix)for(int i=0;i<16;++i){auto m=StemMix{};if(s->config.stereo&&s->stems.size()>1)m.pan=float(-.75+1.5*i/std::max(1,int(s->stems.size())-1));setStemMix(i,m);}
 {const ScopedLock lock(dataLock);current=s;exports.clear();dropFiles.clear();dragDirty=true;}
 {const ScopedLock lock(previewLock);preview=std::move(s);}
 ++dataRevision;
}
void ChoirProcessor::cacheSession(ChoirSession& s){
 if(s.cacheId.isEmpty())s.cacheId=Uuid().toString();auto dir=cacheRoot().getChildFile(s.cacheId);
 if(dir.createDirectory().failed())throw std::runtime_error("Cannot create persistent audio cache.");
 try{writeWave(dir.getChildFile("Source.wav"),*s.source,32,true,cancelled);XmlElement manifest("NooriCache");manifest.setAttribute("sourceName",s.source->name);manifest.setAttribute("originalPath",s.source->originalPath);
  for(int i=0;i<int(s.stems.size());++i){const auto& stem=s.stems[i];const auto name="Voice-"+String(i)+".wav";writeWave(dir.getChildFile(name),*stem.dry,32,true,cancelled);auto* e=manifest.createNewChildElement("Stem");e->setAttribute("name",stem.name);e->setAttribute("part",int(stem.part));e->setAttribute("singer",stem.singer);e->setAttribute("file",name);}
  if(!manifest.writeTo(dir.getChildFile("Session.xml")))throw std::runtime_error("Cannot save audio cache manifest.");
 }catch(...){dir.deleteRecursively();throw;}
}
void ChoirProcessor::prepareDrop(const ChoirSession& s){
 auto folder=cacheRoot().getChildFile("DragFiles");if(folder.createDirectory().failed())throw std::runtime_error("Cannot prepare drag files.");
 const auto revision=mixRevision.load();auto files=exportSession(s,folder,mixSnapshot(int(s.stems.size())),value("floatWav")>.5f?32:24,value("printFx")>.5f,value("bwf")>.5f,-1,cancelled,progress);
 const ScopedLock lock(dataLock);dropFiles=std::move(files);preparedMixRevision=revision;dragDirty=false;
}
void ChoirProcessor::importWav(File file){if(isCapturing())return;stop();runJob([this,file]{auto s=std::make_shared<ChoirSession>();s->source=std::make_shared<VocalFile>(readWav(file,cancelled));s->config=config();s->fx=fxConfig();cacheSession(*s);publish(s,true);progress=1;const ScopedLock lock(dataLock);status="Vocal loaded. Choose choir parts and Create Vocals.";});}
void ChoirProcessor::generate(int regenerate){if(isCapturing())return;const auto c=config();const auto fx=fxConfig();stop();runJob([this,c,fx,regenerate]{
 auto old=session();if(!old||!old->source)throw std::runtime_error("Import or capture a vocal first.");
 const auto roles=regenerate>=0?std::vector<Part>{old->stems.at(std::size_t(regenerate)).part}:arrange(c);
 const auto& source=*old->source;const int frames=source.audio.getNumSamples(),channels=c.stereo?2:1;
 const auto budget=juce::int64(frames)*4*(source.audio.getNumChannels()+2*channels*(regenerate>=0?old->stems.size():roles.size()));
 if(budget>512LL*1024*1024)throw std::runtime_error("Source plus generated voices exceed 512 MiB. Use a shorter vocal or fewer singers.");
 auto update=[this](String message,float amount){progress=amount;const ScopedLock lock(dataLock);status=std::move(message);};
 update("Analyzing vocal melody...",0);std::vector<float> mono(frames);for(int i=0;i<frames;++i){double x=0;for(int ch=0;ch<source.audio.getNumChannels();++ch)x+=source.audio.getSample(ch,i);mono[i]=float(x/source.audio.getNumChannels());}
 auto melody=analyseMelody(mono.data(),frames,source.rate,cancelled,[&](float amount){progress=.15f*amount;});
 auto result=std::make_shared<ChoirSession>();result->source=old->source;result->config=c;result->fx=fx;
 if(regenerate>=0)result->stems=old->stems;
 for(int v=0;v<int(roles.size());++v){const int singer=regenerate>=0?old->stems[regenerate].singer+17:v;
  update("Generating "+String(partName(roles[v]))+" ("+String(v+1)+"/"+String(roles.size())+")...",.15f+.7f*v/roles.size());
  auto data=renderSinger(mono.data(),frames,source.rate,melody,roles[v],singer,c,cancelled,[&](float amount){progress=.15f+.7f*(v+amount)/roles.size();});
  auto dry=std::make_shared<VocalFile>();dry->rate=source.rate;dry->sampleOffset=source.sampleOffset;dry->name=String(partName(roles[v]));dry->audio.setSize(channels,frames);
  for(int ch=0;ch<channels;++ch)std::copy(data.begin(),data.end(),dry->audio.getWritePointer(ch));
  Stem stem{String(partName(roles[v])),roles[v],singer,dry,postProcess(*dry,fx,cancelled)};
  if(regenerate>=0)result->stems[regenerate]=std::move(stem);else result->stems.push_back(std::move(stem));
 }
 update("Saving voices and preparing stem files...",.88f);cacheSession(*result);publish(result,regenerate<0);prepareDrop(*result);progress=1;const ScopedLock lock(dataLock);status="Vocals ready. Audition the parts, adjust Post FX, or drag stems to Cubase.";
 });}
void ChoirProcessor::applyFx(){const auto fx=fxConfig();stop();runJob([this,fx]{auto old=session();if(!old||old->stems.empty())return;auto result=std::make_shared<ChoirSession>(*old);result->fx=fx;for(int i=0;i<int(result->stems.size());++i){result->stems[i].wet=postProcess(*result->stems[i].dry,fx,cancelled);progress=.8f*(i+1)/result->stems.size();}publish(result);prepareDrop(*result);const ScopedLock lock(dataLock);status="Post FX rendered. Stem files ready.";});}
void ChoirProcessor::prepareStems(){runJob([this]{auto s=session();if(!s||s->stems.empty())throw std::runtime_error("Create vocals first.");prepareDrop(*s);const ScopedLock lock(dataLock);status="Stem files ready. Drag STEMS to Cubase.";});}
void ChoirProcessor::exportTo(File project,int selected){const int bits=value("floatWav")>.5f?32:24;const bool print=value("printFx")>.5f,bwf=value("bwf")>.5f;
 runJob([this,project,selected,bits,print,bwf]{auto s=session();if(!s||s->stems.empty())throw std::runtime_error("Create vocals before export.");if(!project.isDirectory())throw std::runtime_error("Choose the existing Cubase project directory.");auto folder=project.getChildFile("NOORI CHOIR");if(folder.createDirectory().failed())throw std::runtime_error("Cannot create NOORI CHOIR output directory.");auto files=exportSession(*s,folder,mixSnapshot(int(s->stems.size())),bits,print,bwf,selected,cancelled,progress);const ScopedLock lock(dataLock);exports=std::move(files);status="Exported "+String(exports.size())+" WAV file(s) to "+folder.getFullPathName();});}
void ChoirProcessor::clearSource(){if(isBusy()||isCapturing())return;stop();{const ScopedLock lock(dataLock);current.reset();exports.clear();dropFiles.clear();status="Drop a vocal or browse files to begin.";}{const ScopedLock lock(previewLock);preview.reset();}dragDirty=true;++dataRevision;}
void ChoirProcessor::removeStem(int index){if(isBusy())return;auto old=session();if(!old||index<0||index>=int(old->stems.size()))return;auto s=std::make_shared<ChoirSession>(*old);s->stems.erase(s->stems.begin()+index);s->cacheId.clear();for(int i=index;i<int(s->stems.size());++i)setStemMix(i,stemMix(i+1));runJob([this,s]{cacheSession(*s);publish(s);if(!s->stems.empty())prepareDrop(*s);});}
void ChoirProcessor::startCapture(){if(isBusy()||isCapturing())return;stop();const ScopedLock lock(captureLock);captureChannels=getTotalNumInputChannels();capture.setSize(captureChannels,int(hostRate.load()*120));capturedFrames=0;captureReady=false;capturing=true;const ScopedLock data(dataLock);status="Capturing track input. Play the vocal in Cubase, then Stop Capture (maximum 120 seconds).";}
void ChoirProcessor::stopCapture(){{const ScopedLock lock(captureLock);capturing=false;captureReady=false;}if(isBusy())return;runJob([this]{const int n=capturedFrames.load();if(n<=0)throw std::runtime_error("Capture is empty. Play the track while capturing.");auto audio=std::make_shared<VocalFile>();audio->rate=hostRate.load();audio->name="Captured Vocal";audio->sampleOffset=captureOffset;{const ScopedLock lock(captureLock);audio->audio.setSize(captureChannels,n);for(int ch=0;ch<captureChannels;++ch)audio->audio.copyFrom(ch,0,capture,ch,0,n);capture.setSize(0,0);}auto s=std::make_shared<ChoirSession>();s->source=audio;s->config=config();s->fx=fxConfig();cacheSession(*s);publish(s,true);progress=1;const ScopedLock lock(dataLock);status="Capture ready. Choose parts and Create Vocals.";});}
void ChoirProcessor::getStateInformation(MemoryBlock& block){auto tree=state.copyState();tree.setProperty("darkMode",darkMode.load(),nullptr);tree.setProperty("uiScale",uiScale.load(),nullptr);auto s=session();tree.setProperty("cacheId",s?s->cacheId:String{},nullptr);ValueTree levels("StemMixes");for(int i=0;i<16;++i){auto m=stemMix(i);ValueTree level("Stem");level.setProperty("gain",m.gain,nullptr);level.setProperty("pan",m.pan,nullptr);level.setProperty("mute",m.mute,nullptr);level.setProperty("solo",m.solo,nullptr);levels.appendChild(level,nullptr);}tree.removeChild(tree.getChildWithName("StemMixes"),nullptr);tree.appendChild(levels,nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,block);}
void ChoirProcessor::setStateInformation(const void* data,int bytes){if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(state.state.getType())){auto tree=ValueTree::fromXml(*xml);darkMode=bool(tree.getProperty("darkMode",true));uiScale=std::clamp(int(tree.getProperty("uiScale",1)),1,3);state.replaceState(tree);std::vector<StemMix> levels;auto saved=tree.getChildWithName("StemMixes");for(int i=0;i<16;++i){auto m=saved.getChild(i);levels.push_back({float(m.getProperty("gain",0)),float(m.getProperty("pan",0)),bool(m.getProperty("mute",false)),bool(m.getProperty("solo",false))});}auto id=tree.getProperty("cacheId").toString();stop();if(id.isNotEmpty())restoreCache(id,levels);}}
void ChoirProcessor::restoreCache(String id,std::vector<StemMix> levels){runJob([this,id,levels]{if((id.length()!=32&&id.length()!=36)||!id.containsOnly("0123456789abcdefABCDEF-"))throw std::runtime_error("Invalid saved audio cache ID.");auto folder=cacheRoot().getChildFile(id);auto manifest=XmlDocument::parse(folder.getChildFile("Session.xml"));if(!manifest||!manifest->hasTagName("NooriCache"))throw std::runtime_error("Saved audio cache is missing. Reimport the original vocal and regenerate.");auto s=std::make_shared<ChoirSession>();auto source=std::make_shared<VocalFile>(readWav(folder.getChildFile("Source.wav"),cancelled));source->name=manifest->getStringAttribute("sourceName");source->originalPath=manifest->getStringAttribute("originalPath");s->source=source;s->cacheId=id;s->config=config();s->fx=fxConfig();
 for(auto* e:manifest->getChildIterator()){if(!e->hasTagName("Stem"))continue;if(s->stems.size()>=16)throw std::runtime_error("Invalid saved voice count.");const int index=int(s->stems.size());auto raw=std::make_shared<VocalFile>(readWav(folder.getChildFile("Voice-"+String(index)+".wav"),cancelled));if(raw->rate!=source->rate||raw->audio.getNumSamples()!=source->audio.getNumSamples())throw std::runtime_error("Saved stem has mismatched duration or sample rate.");auto part=Part(std::clamp(e->getIntAttribute("part"),0,7));s->stems.push_back({e->getStringAttribute("name"),part,e->getIntAttribute("singer"),raw,postProcess(*raw,s->fx,cancelled)});}
 publish(s);for(int i=0;i<int(levels.size());++i)setStemMix(i,levels[i]);if(!s->stems.empty())prepareDrop(*s);progress=1;const ScopedLock lock(dataLock);status="Project audio restored from persistent cache.";
 },true);}
AudioProcessorEditor* ChoirProcessor::createEditor(){return new ChoirEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChoirProcessor;}
