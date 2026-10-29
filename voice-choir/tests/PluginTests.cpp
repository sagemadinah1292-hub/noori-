#include "Plugin.h"
#include "Editor.h"
#include <iostream>
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void wait(ChoirProcessor& p){double end=juce::Time::getMillisecondCounterHiRes()+240000;while(p.isBusy()){check(juce::Time::getMillisecondCounterHiRes()<end,"Worker timeout");juce::Thread::sleep(2);}}
juce::Component* find(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(int i=0;i<root.getNumChildComponents();++i)if(auto* result=find(*root.getChildComponent(i),id))return result;return nullptr;}
int main(int argc,char** argv){try{using namespace juce;using namespace noori_choir;ScopedJuceInitialiser_GUI init;auto out=argc>1?File(argv[1]):File::getCurrentWorkingDirectory();out.createDirectory();
 ChoirProcessor p(noori_licensing::Config{});p.setRateAndBufferSizeDetails(48000,256);p.prepareToPlay(48000,256);check(p.value("autoKey")>.5f,"New instances default to auto key detection");check(p.getLatencySamples()==0,"Offline workflow uses transparent zero-latency insert");
 std::unique_ptr<ChoirEditor> editor(static_cast<ChoirEditor*>(p.createEditor()));check(editor->hasBrandLogo(),"Supplied SVG decoded");check(editor->getWidth()==540&&editor->getHeight()==750,"New instances fit at 75 percent");
 auto capture=[&](const String& file){Image image(Image::ARGB,editor->getWidth(),editor->getHeight(),true,SoftwareImageType{});Graphics g(image);editor->paintEntireComponent(g,true);PNGImageFormat png;auto stream=out.getChildFile(file).createOutputStream();check(stream&&png.writeImageToStream(image,*stream),"Actual editor screenshot");
  const float factor=editor->getWidth()/720.f;int white=0;
  for(int y=roundToInt(20*factor);y<roundToInt(92*factor);++y)for(int x=roundToInt(139*factor);x<roundToInt(261*factor);++x){auto pixel=image.getPixelAt(x,y);if(pixel.getRed()>240&&pixel.getGreen()>240&&pixel.getBlue()>240)++white;}
  check(white>20,"Logo renders white in both themes at every scale");};
 for(bool dark:{true,false})for(int scale:{0,1,2,3}){
  const float factor=scale==0?.75f:float(scale);const int percent=scale==0?75:scale*100;
  auto* theme=dynamic_cast<ComboBox*>(find(*editor,"theme"));auto* scaling=dynamic_cast<ComboBox*>(find(*editor,"scaling"));check(theme&&scaling,"Appearance controls");theme->setSelectedId(dark?1:2,dontSendNotification);theme->onChange();scaling->setSelectedId(scale==0?4:scale,dontSendNotification);scaling->onChange();check(editor->getWidth()==roundToInt(720*factor)&&editor->getHeight()==roundToInt(1000*factor),"Requested UI scales");
  editor->showPage(ChoirEditor::Page::Import);capture("Preview-Import-"+String(dark?"Dark":"Light")+"-"+String(percent)+".png");
 }
 auto* types=dynamic_cast<ComboBox*>(find(*editor,"character"));check(types&&types->isVisible()&&types->getNumItems()==8,"All eight choir types are accessible before import");
 for(int i=0;i<8;++i){check(types->getItemText(i)==characterName(i),"Full choir type names");types->setSelectedId(i+1,sendNotificationSync);check(p.config().character==i,"Choir selector reaches the corresponding processing preset");}types->setSelectedId(4,sendNotificationSync);
 for(int code:{0,1}){ChoirProcessor saved(noori_licensing::Config{}),loaded(noori_licensing::Config{});saved.uiScale=code;MemoryBlock appearance;saved.getStateInformation(appearance);loaded.setStateInformation(appearance.getData(),int(appearance.getSize()));check(loaded.uiScale==code,"75 percent and legacy 100 percent state survive reload");}
 auto* scaling=dynamic_cast<ComboBox*>(find(*editor,"scaling"));scaling->setSelectedId(1,dontSendNotification);scaling->onChange();auto* theme=dynamic_cast<ComboBox*>(find(*editor,"theme"));theme->setSelectedId(1,dontSendNotification);theme->onChange();
 VocalFile fixture;fixture.rate=48000;fixture.name="Reference vocal";fixture.audio.setSize(1,48000);for(int i=0;i<48000;++i)fixture.audio.setSample(0,i,float(.25*std::sin(2*pi*196*i/48000.)));std::atomic<bool> cancelled{false};auto file=out.getChildFile("UI-Vocal.wav");writeWave(file,fixture,32,false,cancelled);
 p.importWav(file);wait(p);editor.reset(static_cast<ChoirEditor*>(p.createEditor()));editor->showPage(ChoirEditor::Page::Setup);capture("Preview-Setup.png");
 auto* remove=find(*editor,"removeSource");check(remove&&remove->getParentComponent()->getComponentAt(remove->getBounds().getCentre())==remove,"Source removal control is clickable above waveform");
 auto* create=dynamic_cast<TextButton*>(find(*editor,"create"));check(create&&create->onClick,"Create Vocals connected");create->onClick();capture("Preview-Processing.png");wait(p);check(p.stemCount()==8,"Create button generates eight parts");
 editor.reset(static_cast<ChoirEditor*>(p.createEditor()));editor->showPage(ChoirEditor::Page::Results);capture("Preview-Results.png");capture("Preview.png");
 auto* compact=dynamic_cast<ComboBox*>(find(*editor,"scaling"));compact->setSelectedId(4,dontSendNotification);compact->onChange();capture("Preview-Results-75.png");
 auto* play=dynamic_cast<TextButton*>(find(*editor,"playAll"));check(play&&play->onClick,"Play All connected");play->onClick();check(p.isPlaying(),"Results playback action");p.stop();
 editor->showPage(ChoirEditor::Page::Settings);capture("Preview-Settings.png");
 editor->showPage(ChoirEditor::Page::Activation);capture("Preview-Activation-Development.png");
 p.darkMode=false;p.uiScale=3;MemoryBlock state;p.getStateInformation(state);ChoirProcessor restored(noori_licensing::Config{});restored.setStateInformation(state.getData(),int(state.getSize()));wait(restored);check(!restored.darkMode&&restored.uiScale==3&&restored.stemCount()==8,"Appearance and audio restore");
 file.deleteFile();check(out.getChildFile("choir-tests.ok").replaceWithText("PASS supplied logo, reference workflow pages, eight appearance combinations, Create/Play actions and audio restore.\n"),"UI test marker");std::cout<<"PASS reference workflow editor and project state\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
