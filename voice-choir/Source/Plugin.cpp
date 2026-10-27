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
void ChoirProcessor::getStateInformation(MemoryBlock& b){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,b);}
void ChoirProcessor::setStateInformation(const void* d,int n){if(auto xml=getXmlFromBinary(d,n))if(xml->hasTagName(state.state.getType())){state.replaceState(ValueTree::fromXml(*xml));clearRequested=true;}}
void ChoirProcessor::setValue(const String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
void ChoirProcessor::preset(int n){const float presets[3][6]={{4,4,7,65,45,-3},{8,8,12,85,65,-3},{16,12,22,100,80,-6}};int i=0;for(auto id:{"voices","detune","timing","width","mix","gain"})setValue(id,presets[std::clamp(n,0,2)][i++]);}
AudioProcessorEditor* ChoirProcessor::createEditor(){return new ChoirEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ChoirProcessor;}
