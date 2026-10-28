#include "Plugin.h"
#include "Editor.h"
using namespace juce;
AudioProcessorValueTreeState::ParameterLayout ChoirProcessor::layout(){AudioProcessorValueTreeState::ParameterLayout p;
 p.add(std::make_unique<AudioParameterInt>(ParameterID{"voices",1},"Voices",2,16,8));
 auto add=[&](const char* id,const char* name,float low,float high,float def){p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(low,high,.1f),def));};
 add("detune","Pitch spread (cents)",0,20,8);add("timing","Timing spread (ms)",0,30,12);add("width","Stereo width (%)",0,100,85);add("mix","Choir blend (%)",0,100,65);add("gain","Output (dB)",-24,6,-3);return p;}
ChoirProcessor::ChoirProcessor():AudioProcessor(BusesProperties().withInput("Vocal",AudioChannelSet::stereo(),true).withOutput("Choir",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NooriVoiceChoir",layout()){int i=0;for(auto id:{"voices","detune","timing","width","mix","gain"})values[i++]=state.getRawParameterValue(id);}
noori_choir::Settings ChoirProcessor::settings()const{return {values[0]->load(),values[1]->load(),values[2]->load(),values[3]->load(),values[4]->load(),values[5]->load()};}
void ChoirProcessor::prepareToPlay(double sr,int){engine.prepare(sr);setLatencySamples(engine.latencySamples());clearRequested=false;}
bool ChoirProcessor::isBusesLayoutSupported(const BusesLayout& l)const{auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();return (in==AudioChannelSet::mono()||in==AudioChannelSet::stereo())&&(out==in||out==AudioChannelSet::stereo());}
void ChoirProcessor::processBlock(AudioBuffer<float>& b,MidiBuffer&){ScopedNoDenormals guard;if(clearRequested.exchange(false))engine.reset();if(getTotalNumInputChannels()==1&&b.getNumChannels()>1)b.copyFrom(1,0,b,0,0,b.getNumSamples());engine.process(b.getWritePointer(0),b.getNumChannels()>1?b.getWritePointer(1):nullptr,b.getNumSamples(),settings());}
void ChoirProcessor::getStateInformation(MemoryBlock& b){auto tree=state.copyState();tree.setProperty("darkMode",darkMode.load(),nullptr);tree.setProperty("uiScale",uiScale.load(),nullptr);auto xml=tree.createXml();copyXmlToBinary(*xml,b);}
void ChoirProcessor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n))if(xml->hasTagName(state.state.getType())){auto tree=ValueTree::fromXml(*xml);darkMode=bool(tree.getProperty("darkMode",true));uiScale=std::clamp(int(tree.getProperty("uiScale",1)),1,3);state.replaceState(tree);clearRequested=true;}}
void ChoirProcessor::setValue(const String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
void ChoirProcessor::preset(int n){const float presets[3][6]={{4,4,7,65,45,-3},{8,8,12,85,65,-3},{16,12,22,100,80,-6}};int i=0;for(auto id:{"voices","detune","timing","width","mix","gain"})setValue(id,presets[std::clamp(n,0,2)][i++]);}
AudioProcessorEditor* ChoirProcessor::createEditor(){return new ChoirEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChoirProcessor;}


ChoirProcessor::~ChoirProcessor(){cancelled=true;if(worker.joinable())worker.join();}
juce::String ChoirProcessor::jobStatus()const{const juce::ScopedLock lock(offlineLock);return status;}
juce::String ChoirProcessor::sourceDescription()const{const juce::ScopedLock lock(offlineLock);if(!vocal)return "No WAV imported";return vocal->name+"  |  "+juce::String(vocal->rate,0)+" Hz  |  "+juce::String(vocal->audio.getNumChannels())+" ch  |  "+juce::String(vocal->audio.getNumSamples()/vocal->rate,2)+" s";}
bool ChoirProcessor::hasSource()const{const juce::ScopedLock lock(offlineLock);return bool(vocal);}
juce::StringArray ChoirProcessor::exportedFiles()const{const juce::ScopedLock lock(offlineLock);return exports;}
void ChoirProcessor::runJob(std::function<void()> fn){
 bool expected=false;if(!busy.compare_exchange_strong(expected,true))return;
 if(worker.joinable())worker.join();cancelled=false;progress=0;
 {const juce::ScopedLock lock(offlineLock);status="Working…";}
 try{worker=std::thread([this,fn=std::move(fn)]{
  try{fn();}catch(const std::exception& e){const juce::ScopedLock lock(offlineLock);status=juce::String::fromUTF8(e.what());}
  catch(...){const juce::ScopedLock lock(offlineLock);status="Audio operation failed.";}
  busy=false;
 });}catch(...){busy=false;const juce::ScopedLock lock(offlineLock);status="Cannot start background audio job.";}
}
void ChoirProcessor::importWav(juce::File file){runJob([this,file]{
 auto imported=std::make_shared<noori_choir::VocalFile>(noori_choir::readWav(file,cancelled));
 const juce::ScopedLock lock(offlineLock);vocal=std::move(imported);exports.clear();progress=1;status="WAV imported. Select your Cubase project folder to export eight voices.";
});}
void ChoirProcessor::exportTo(juce::File directory){
 const auto controls=settings();
 runJob([this,directory,controls]{
  std::shared_ptr<const noori_choir::VocalFile> source;
  {const juce::ScopedLock lock(offlineLock);source=vocal;}
  if(!source)throw std::runtime_error("Import a WAV first.");
  auto files=noori_choir::exportWavs(*source,directory,controls.detune,controls.timing,cancelled,progress);
  const juce::ScopedLock lock(offlineLock);exports=std::move(files);status="Exported eight synchronized WAVs to "+directory.getChildFile("NOORI CHOIR").getFullPathName();
 });
}
