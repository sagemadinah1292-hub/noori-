#include "Plugin.h"
#include "Editor.h"
using namespace juce;
AudioProcessorValueTreeState::ParameterLayout DelayProcessor::layout(){
 AudioProcessorValueTreeState::ParameterLayout p;
 auto f=[&](const char* id,const char* name,NormalisableRange<float> range,float value){p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,range,value));};
 f("feedback","Feedback",{0,100,.1f},50);f("mix","Mix",{0,100,.1f},50);f("manualBpm","Fallback BPM",{1,1000,.01f},120);
 for(int i=1;i<=2;++i){auto n=String(i);p.add(std::make_unique<AudioParameterBool>(ParameterID{"sync"+n,1},"Tempo sync "+n,false));
  p.add(std::make_unique<AudioParameterFloat>(ParameterID{"time"+n,1},"Delay time "+n,NormalisableRange<float>(0,9999,.01f,.35f),500));
  p.add(std::make_unique<AudioParameterChoice>(ParameterID{"note"+n,1},"Note "+n,StringArray{"1/1","1/2","1/4","1/8","1/16","1/32"},2));
  p.add(std::make_unique<AudioParameterChoice>(ParameterID{"rhythm"+n,1},"Rhythm "+n,StringArray{"Straight","Triplet","Dotted"},0));
  p.add(std::make_unique<AudioParameterInt>(ParameterID{"mult"+n,1},"Tempo multiplier "+n,1,10,1));
  p.add(std::make_unique<AudioParameterFloat>(ParameterID{"pan"+n,1},"Pan "+n,NormalisableRange<float>(-100,100,.1f),i==1?-100.f:100.f));}
 return p;
}
DelayProcessor::DelayProcessor():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)),state(*this,nullptr,"NooriDoubleDelay",layout()){feedbackValue=state.getRawParameterValue("feedback");mixValue=state.getRawParameterValue("mix");bpmValue=state.getRawParameterValue("manualBpm");for(int i=0;i<2;++i){int j=0;for(auto id:{"sync","time","note","rhythm","mult","pan"})tapValues[(std::size_t)i][(std::size_t)j++]=state.getRawParameterValue(String(id)+String(i+1));}}
noori_delay::Settings DelayProcessor::settings()const{noori_delay::Settings s;s.feedback=feedbackValue->load()/100.;s.mix=mixValue->load()/100.;s.bpm=tempoAvailable?displayedBpm.load():bpmValue->load();for(std::size_t i=0;i<2;++i){const auto& v=tapValues[i];auto& t=s.taps[i];t.sync=v[0]->load()>.5f;t.milliseconds=v[1]->load();t.note=int(v[2]->load());t.rhythm=int(v[3]->load());t.multiplier=int(v[4]->load());t.pan=v[5]->load()/100.;}return s;}
void DelayProcessor::prepareToPlay(double rate,int){engine.prepare(rate);tempoAvailable=false;clearRequested=false;setLatencySamples(0);}
bool DelayProcessor::isBusesLayoutSupported(const BusesLayout& l)const{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==AudioChannelSet::mono()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
void DelayProcessor::processBlock(AudioBuffer<float>& b,MidiBuffer&){ScopedNoDenormals guard;if(clearRequested.exchange(false))engine.reset();bool found=false;if(auto* head=getPlayHead())if(auto pos=head->getPosition())if(auto bpm=pos->getBpm())if(std::isfinite(*bpm)&&*bpm>0){displayedBpm=*bpm;found=true;}tempoAvailable=found;auto s=settings();limited=false;for(auto& t:s.taps)if(noori_delay::seconds(t,s.bpm)>noori_delay::maxSeconds)limited=true;engine.process(b.getWritePointer(0),b.getNumChannels()>1?b.getWritePointer(1):nullptr,b.getNumSamples(),s);}
void DelayProcessor::getStateInformation(MemoryBlock& b){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,b);}
void DelayProcessor::setStateInformation(const void* data,int bytes){if(auto xml=getXmlFromBinary(data,bytes))if(xml->hasTagName(state.state.getType())){state.replaceState(ValueTree::fromXml(*xml));clearRequested=true;}}
void DelayProcessor::setValue(const String& id,float value){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}}
void DelayProcessor::preset(int index){
 setValue("feedback",50);setValue("mix",50);
 for(int i=1;i<=2;++i){auto n=String(i);setValue("sync"+n,index==1?0.f:1.f);setValue("time"+n,500);setValue("pan"+n,i==1?-100.f:100.f);setValue("note"+n,index==3?3.f:(i==1?3.f:2.f));setValue("rhythm"+n,index==2&&i==1?2.f:0.f);setValue("mult"+n,index==2?float(i+1):1.f);}
 if(index==3)setValue("feedback",30);clearRequested=true;
}
AudioProcessorEditor* DelayProcessor::createEditor(){return new DelayEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new DelayProcessor;}
