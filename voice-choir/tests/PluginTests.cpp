#include "Plugin.h"
#include "Editor.h"
#include <iostream>
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void wait(ChoirProcessor& p){double end=juce::Time::getMillisecondCounterHiRes()+240000;while(p.isBusy()){check(juce::Time::getMillisecondCounterHiRes()<end,"Worker timeout");juce::Thread::sleep(2);}}
juce::Component* find(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(int i=0;i<root.getNumChildComponents();++i)if(auto* result=find(*root.getChildComponent(i),id))return result;return nullptr;}
int main(int argc,char** argv){try{using namespace juce;using namespace noori_choir;ScopedJuceInitialiser_GUI init;auto out=argc>1?File(argv[1]):File::getCurrentWorkingDirectory();out.createDirectory();
 ChoirProcessor p;p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);check(p.getLatencySamples()==0,"Offline workflow uses transparent zero-latency insert");
 std::unique_ptr<ChoirEditor> editor(static_cast<ChoirEditor*>(p.createEditor()));check(editor->hasBrandLogo(),"Supplied SVG decoded");check(editor->getWidth()==720&&editor->getHeight()==1000,"Reference-oriented tall editor");
 auto capture=[&](const String& file){Image image(Image::ARGB,editor->getWidth(),editor->getHeight(),true,SoftwareImageType{});Graphics g(image);editor->paintEntireComponent(g,true);PNGImageFormat png;auto stream=out.getChildFile(file).createOutputStream();check(stream&&png.writeImageToStream(image,*stream),"Actual editor screenshot");};
 for(bool dark:{true,false})for(int scale:{1,2,3}){
  auto* theme=dynamic_cast<ComboBox*>(find(*editor,"theme"));auto* scaling=dynamic_cast<ComboBox*>(find(*editor,"scaling"));check(theme&&scaling,"Appearance controls");theme->setSelectedId(dark?1:2,dontSendNotification);theme->onChange();scaling->setSelectedId(scale,dontSendNotification);scaling->onChange();check(editor->getWidth()==720*scale&&editor->getHeight()==1000*scale,"Requested UI scales");
  editor->showPage(ChoirEditor::Page::Import);capture("Preview-Import-"+String(dark?"Dark":"Light")+"-"+String(scale*100)+".png");
 }
 auto* scaling=dynamic_cast<ComboBox*>(find(*editor,"scaling"));scaling->setSelectedId(1,dontSendNotification);scaling->onChange();auto* theme=dynamic_cast<ComboBox*>(find(*editor,"theme"));theme->setSelectedId(1,dontSendNotification);theme->onChange();
 VocalFile fixture;fixture.rate=48000;fixture.name="Reference vocal";fixture.audio.setSize(1,48000);for(int i=0;i<48000;++i)fixture.audio.setSample(0,i,float(.25*std::sin(2*pi*196*i/48000.)));std::atomic<bool> cancelled{false};auto file=out.getChildFile("UI-Vocal.wav");writeWave(file,fixture,32,false,cancelled);
 p.importWav(file);wait(p);editor.reset(static_cast<ChoirEditor*>(p.createEditor()));editor->showPage(ChoirEditor::Page::Setup);capture("Preview-Setup.png");
 auto* remove=find(*editor,"removeSource");check(remove&&remove->getParentComponent()->getComponentAt(remove->getBounds().getCentre())==remove,"Source removal control is clickable above waveform");
 auto* create=dynamic_cast<TextButton*>(find(*editor,"create"));check(create&&create->onClick,"Create Vocals connected");create->onClick();capture("Preview-Processing.png");wait(p);check(p.stemCount()==8,"Create button generates eight parts");
 editor.reset(static_cast<ChoirEditor*>(p.createEditor()));editor->showPage(ChoirEditor::Page::Results);capture("Preview-Results.png");capture("Preview.png");
 auto* play=dynamic_cast<TextButton*>(find(*editor,"playAll"));check(play&&play->onClick,"Play All connected");play->onClick();check(p.isPlaying(),"Results playback action");p.stop();
 editor->showPage(ChoirEditor::Page::Settings);capture("Preview-Settings.png");
 p.darkMode=false;p.uiScale=3;MemoryBlock state;p.getStateInformation(state);ChoirProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));wait(restored);check(!restored.darkMode&&restored.uiScale==3&&restored.stemCount()==8,"Appearance and audio restore");
 file.deleteFile();check(out.getChildFile("choir-tests.ok").replaceWithText("PASS supplied logo, reference workflow pages, six appearance combinations, Create/Play actions and audio restore.\n"),"UI test marker");std::cout<<"PASS reference workflow editor and project state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
