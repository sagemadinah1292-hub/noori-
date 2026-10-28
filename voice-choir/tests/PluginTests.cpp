#include "Plugin.h"
#include "Editor.h"
#include <iostream>
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{juce::ScopedJuceInitialiser_GUI init;ChoirProcessor p;p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);check(p.getLatencySamples()==1920,"Reported latency");p.preset(2);juce::MemoryBlock state;p.getStateInformation(state);ChoirProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));check(restored.settings().voices==16&&restored.settings().gain==-6&&restored.settings().width==100,"Preset save restore");
 juce::AudioProcessor::BusesLayout layout;layout.inputBuses.add(juce::AudioChannelSet::mono());layout.outputBuses.add(juce::AudioChannelSet::stereo());check(p.setBusesLayout(layout),"Mono to stereo support");p.prepareToPlay(48000,256);juce::MidiBuffer midi;juce::AudioBuffer<float>b(2,256);double energy=0;for(int n=0;n<16000;n+=256){for(int i=0;i<256;++i){b.setSample(0,i,float(.2*std::sin((n+i)*.035)));b.setSample(1,i,123.f);}p.processBlock(b,midi);for(int i=0;i<256;++i){check(std::isfinite(b.getSample(1,i))&&std::abs(b.getSample(1,i))<1,"Mono input output cleared and rendered");energy+=b.getSample(1,i)*b.getSample(1,i);}}check(energy>.1,"Audible plugin output");
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor&&editor->getWidth()==980,"Editor constructed");check(dynamic_cast<ChoirEditor*>(editor.get())->hasBrandLogo(),"Bundled vector logo decoded");editor->setVisible(true);bool preset=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* c=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i));c&&c->getName().isEmpty()){c->setSelectedId(1,juce::dontSendNotification);c->onChange();preset=true;}check(preset&&p.settings().voices==4,"GUI preset applies");
 auto out=argc>1?juce::File(argv[1]):juce::File::getCurrentWorkingDirectory();out.createDirectory();for(bool dark:{true,false})for(int scale:{1,2,3}){
  for(int i=0;i<editor->getNumChildComponents();++i)if(auto* c=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i))){
   if(c->getName()=="Colour theme"){c->setSelectedId(dark?1:2,juce::dontSendNotification);c->onChange();}
   if(c->getName()=="Interface scale"){c->setSelectedId(scale,juce::dontSendNotification);c->onChange();}
  }
  check(editor->getWidth()==980*scale&&editor->getHeight()==660*scale,"UI scale applied");
  juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType{});
  juce::Graphics g(image);editor->paintEntireComponent(g,true);
  check(image.getPixelAt(40*scale,150*scale)!=image.getPixelAt(5,5),"UI paints panel");
  int ink=0;for(int y=24*scale;y<92*scale;++y)for(int x=38*scale;x<218*scale;++x)if(image.getPixelAt(x,y).getBrightness()<.25f)++ink;check(ink>100*scale*scale,"Bundled NOORI logo rendered");
  juce::PNGImageFormat png;auto file=out.getChildFile("Preview-"+juce::String(dark?"Dark":"Light")+"-"+juce::String(scale*100)+".png").createOutputStream();
  check(file&&png.writeImageToStream(image,*file),"GUI preview");file.reset();
  if(dark&&scale==1){auto defaultFile=out.getChildFile("Preview.png").createOutputStream();check(defaultFile&&png.writeImageToStream(image,*defaultFile),"Default preview");}
 }
 p.darkMode=false;p.uiScale=3;juce::MemoryBlock uiState;p.getStateInformation(uiState);restored.setStateInformation(uiState.getData(),int(uiState.getSize()));check(!restored.darkMode&&restored.uiScale==3,"Appearance saved in host state");
 out.getChildFile("choir-tests.ok").replaceWithText("Engine and processor/editor tests passed. Cubase acceptance listening not yet performed.\n");std::cout<<"PASS processor, state, mono/stereo routing, UI preset and preview\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
