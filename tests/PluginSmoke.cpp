#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DefaultLogo.h"
#include <iostream>
#include <stdexcept>
#include <numbers>
#include "CrashDiagnostics.h"
static void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}
class HostTransport final:public juce::AudioPlayHead {
public:
 juce::Optional<PositionInfo> getPosition()const override {PositionInfo p;p.setBpm(80);p.setTimeSignature(TimeSignature{6,8});p.setTimeInSamples(sample);p.setTimeInSeconds(sample/48000.);p.setPpqPosition(sample*80./(60*48000));p.setIsPlaying(true);return p;}
 juce::int64 sample=0;
};
static void waitSession(const std::shared_ptr<EditSession>& s,bool success=true){auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(s->busy&&juce::Time::getMillisecondCounterHiRes()<deadline){s->settle();juce::Thread::sleep(2);}s->settle();check(!s->busy,"Worker timed out");if(success)check(s->status.contains("ready"),s->status.toRawUTF8());}
#include "ProjectHostTests.h"
#include "HistoryTests.h"
int main(int argc,char** argv){installTestCrashDiagnostics();try{
 juce::ScopedJuceInitialiser_GUI gui;auto builtInLogo=juce::Drawable::createFromImageData(defaultLogoSVG,sizeof(defaultLogoSVG)-1);check(builtInLogo&&!builtInLogo->getDrawableBounds().isEmpty(),"Provided SVG logo loads as vector artwork");historyTests();
#if JucePlugin_Enable_ARA
 const auto* factory=createARAFactory();check(factory&&juce::String(factory->factoryID)=="audio.nooriturab.tempo.factory.0.3.0"&&juce::String(factory->documentArchiveID)=="audio.nooriturab.tempo.archive.4","ARA factory identifiers");
 check(factory->compatibleDocumentArchiveIDsCount==3&&juce::String(factory->compatibleDocumentArchiveIDs[0])=="audio.nooriturab.tempo.archive.1"&&juce::String(factory->compatibleDocumentArchiveIDs[1])=="audio.nooriturab.tempo.archive.2","ARA backward archive IDs");
 projectHostTests(factory,argc>1?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):juce::File::getCurrentWorkingDirectory());
#endif
 auto processorStorage=std::make_unique<NooriProcessor>();auto& p=*processorStorage;
#if JucePlugin_Enable_ARA
 check(!canRequestARAPlayback(p)&&!requestARAPlayback(p,1,true)&&!requestARAStop(p),"Absent ARA playback controller is handled safely");
#endif
 HostTransport host;p.setPlayHead(&host);p.setRateAndBufferSizeDetails(48000,512);p.prepareToPlay(48000,512);juce::AudioBuffer<float> b(2,512);juce::MidiBuffer midi;b.clear();p.processBlock(b,midi);check(p.bpm==80&&p.numerator==6&&p.denominator==8,"Host tempo/time signature");
 auto audio=std::make_shared<noori::Audio>();audio->sampleRate=48000;audio->channels.resize(2,std::vector<float>(48000*4));
 for(auto& c:audio->channels)for(int i=0;i<(int)c.size();++i){double time=i/48000.;double phase=std::fmod(time,.75);double envelope=phase<.04?phase/.04:phase<.50?std::exp(-(phase-.04)*4):0;c[(std::size_t)i]=float(.55*envelope*(std::sin(2*std::numbers::pi*180*time)+.2*std::sin(2*std::numbers::pi*360*time)));}
 p.insertSession->load([audio]{return *audio;},"Synthetic test vocal | 48 kHz",0,noori::Timeline());waitSession(p.insertSession);p.insertSession->align();waitSession(p.insertSession);check(!p.insertSession->attacks.empty(),"Attack analysis");
 auto wav=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("noori-export-test",".wav");
 p.exportFile(wav,p.insertSession);p.exportFile(wav,p.insertSession);juce::AudioFormatManager formats;formats.registerBasicFormats();{std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wav));check(reader&&reader->bitsPerSample==24&&reader->lengthInSamples==audio->length(),"Repeated 24-bit export replaces file correctly");}wav.deleteFile();
 check(p.insertSession->canUndo(),"Undo available");auto attacks=p.insertSession->attacks;p.insertSession->undo();check(p.insertSession->attacks.empty(),"Undo restored original");p.insertSession->redo();check(p.insertSession->attacks.size()==attacks.size(),"Redo restored edits");
 // Undo/redo must restore actual rendered buffers, not merely the marker count.
 auto alignedAudio=p.insertSession->audible.load();p.insertSession->compare=true;p.insertSession->undo();
 check(p.insertSession->audible.load()==p.insertSession->original.get()&&!p.insertSession->compare,"Undo restores source buffer and clears original listening mode");
 p.insertSession->redo();check(p.insertSession->audible.load()==alignedAudio,"Redo restores same rendered samples");
 auto first=p.insertSession->attacks.front().target;p.insertSession->move(0,first+100);waitSession(p.insertSession);
 auto dragged=p.insertSession->attacks.front().target;check(dragged!=first,"Manual edit is a distinct revision");
 p.insertSession->undo();check(p.insertSession->attacks.front().target==first,"Undo manual edit");
 p.insertSession->redo();check(p.insertSession->attacks.front().target==dragged,"Redo manual edit");
 p.insertSession->undo();p.insertSession->move(0,first+150);waitSession(p.insertSession);check(!p.insertSession->canRedo(),"New edit discards stale redo branch");
 p.insertSession->undo();attacks=p.insertSession->attacks;
 // Import a click reference at a non-zero project offset. Detection runs on a worker.
 auto clickFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("noori-click-test",".wav");
 {juce::WavAudioFormat f;auto stream=clickFile.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(f.createWriterFor(stream.release(),48000,1,24,{},0));check(writer!=nullptr,"Reference fixture writer");juce::AudioBuffer<float> clicks(1,48000);clicks.clear();clicks.setSample(0,1000,.8f);clicks.setSample(0,25000,.8f);check(writer->writeFromAudioSampleBuffer(clicks,0,48000),"Reference fixture write");}
 p.insertSession->loadReference(clickFile,12000);waitSession(p.insertSession);
 auto referenceSession=std::make_shared<EditSession>();referenceSession->load([audio]{return *audio;},"Reference match fixture",0,noori::Timeline());waitSession(referenceSession);
 referenceSession->attacks={{38880,38880,.2f,1,true}};referenceSession->loadReference(clickFile,12000);waitSession(referenceSession);referenceSession->align();waitSession(referenceSession);
 check(referenceSession->attacks[0].target==37000,"Session exact reference target");
 check(std::abs(referenceSession->audible.load()->channels[0][37000]-audio->channels[0][38880])<1e-7,"Rendered transient centre lands at exact reference sample");
 auto araState=referenceSession->archive(false);auto araReopened=std::make_shared<EditSession>();araReopened->restore(araState.getData(),araState.getSize(),false);araReopened->load([audio]{return *audio;},"Restored ARA fixture",0,noori::Timeline());waitSession(araReopened);
 check(araReopened->referenceClicks==referenceSession->referenceClicks&&araReopened->useReference,"ARA metadata preload preserves reference");
 auto vocalReferenceFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("noori-vocal-reference-test",".wav");
 {juce::WavAudioFormat f;auto stream=vocalReferenceFile.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(f.createWriterFor(stream.release(),48000,2,24,{},0));check(writer!=nullptr,"Vocal reference writer");juce::AudioBuffer<float> vocal(2,(int)audio->length());for(int c=0;c<2;++c)vocal.copyFrom(c,0,audio->channels[(std::size_t)c].data(),(int)audio->length());check(writer->writeFromAudioSampleBuffer(vocal,0,vocal.getNumSamples()),"Vocal reference fixture write");}
 auto vocalReferenceSession=std::make_shared<EditSession>();vocalReferenceSession->load([audio]{return *audio;},"Vocal reference import",0,noori::Timeline());waitSession(vocalReferenceSession);
 vocalReferenceSession->loadReference(vocalReferenceFile,12000,false);waitSession(vocalReferenceSession);check(!vocalReferenceSession->referenceIsClick&&!vocalReferenceSession->referenceClicks.empty(),"Imported reference honours vocal detector selection");
 auto vocalReferenceState=vocalReferenceSession->archive(true);auto restoredVocalReference=std::make_shared<EditSession>();restoredVocalReference->restore(vocalReferenceState.getData(),vocalReferenceState.getSize(),true);waitSession(restoredVocalReference);check(!restoredVocalReference->referenceIsClick&&restoredVocalReference->referenceClicks==vocalReferenceSession->referenceClicks,"Imported vocal reference mode and attacks survive restore");vocalReferenceFile.deleteFile();
 auto autoSession=std::make_shared<EditSession>();autoSession->load([audio]{return *audio;},"Clustered vocal",0,noori::Timeline());waitSession(autoSession);autoSession->attacks={{37440,37440,1,1,false},{38880,38880,.5f,1,false}};autoSession->referenceClicks={36000};autoSession->useReference=true;autoSession->align(false,{},true);waitSession(autoSession);check(autoSession->attacks[0].target==36000&&autoSession->status.contains("1 exact anchors"),"Automatic track action resolves duplicate click assignments");
 auto previousAuto=autoSession->audible.load();autoSession->referenceClicks={300000};autoSession->align(false,{},true);check(!autoSession->busy&&autoSession->status.contains("does not overlap")&&autoSession->audible.load()==previousAuto,"Nonoverlapping reference explains its placement and preserves audio");
 auto priorBuffer=p.insertSession->audible.load();p.insertSession->align();waitSession(p.insertSession,false);check(p.insertSession->status.contains("Exact click match")&&p.insertSession->audible.load()==priorBuffer,p.insertSession->status.toRawUTF8());clickFile.deleteFile();
 check(p.insertSession->referenceClicks==std::vector<noori::Sample>{13000,37000},"Reference import project anchor");
 juce::MemoryBlock state;p.getStateInformation(state);auto reopenedStorage=std::make_unique<NooriProcessor>();auto& reopened=*reopenedStorage;reopened.setRateAndBufferSizeDetails(48000,512);reopened.prepareToPlay(48000,512);reopened.setStateInformation(state.getData(),(int)state.getSize());waitSession(reopened.insertSession);check(reopened.insertSession->referenceClicks==p.insertSession->referenceClicks&&reopened.insertSession->useReference,"Saved reference clicks restored");check(reopened.insertSession->attacks.size()==attacks.size(),"Saved project restored attacks");check(reopened.insertSession->original->channels==audio->channels,"Float source restored losslessly");
 p.audition=true;host.sample=48000;p.processBlock(b,midi);double energy=0;for(int i=0;i<b.getNumSamples();++i)energy+=std::abs(b.getSample(0,i));check(energy>1,"Host-synchronized audition");
 p.beginCapture();for(int block=0;block<20;++block){host.sample=block*512;b.clear();b.addFrom(0,0,audio->channels[0].data()+block*512,512);b.addFrom(1,0,audio->channels[1].data()+block*512,512);p.processBlock(b,midi);}p.finishCapture();waitSession(p.insertSession);check(p.insertSession->original->length()==10240,"Capture stream length");
 // ARA references map through project time, even at a different source rate.
 auto reference441=std::make_shared<noori::Audio>();reference441->sampleRate=44100;reference441->channels.resize(1,std::vector<float>(88200));reference441->channels[0][22050]=.8f;reference441->channels[0][66150]=.8f;
 auto mapping=std::make_shared<EditSession>();mapping->load([audio]{return *audio;},"Mapping fixture",480000,noori::Timeline());waitSession(mapping);
 mapping->loadProjectReference({{reference441,.25,.75,12},{reference441,.25,.75,12}},"Trimmed click track");waitSession(mapping);
 check(mapping->referenceClicks==std::vector<noori::Sample>{108000},"Reference sample-rate conversion, crop, project origin and duplicate region merge");
 mapping->anchorTo(504000);check(mapping->referenceClicks[0]==84000&&mapping->realtimeOrigin==504000,"Reanchor retains absolute reference placement");
 mapping->attacks={{10000,10000,1,1,false},{50000,50000,1,1,false}};mapping->selectRange(40000,60000);check(!mapping->attacks[0].selected&&mapping->attacks[1].selected,"Range selection");mapping->selectAll();check(mapping->attacks[0].selected&&mapping->attacks[1].selected,"Select all attacks");mapping->selectAll(false);
 // Connected insert references use real processor instances and host track names.
 {
  auto clickProcessor=std::make_unique<NooriProcessor>();juce::AudioProcessor::TrackProperties props;props.name="Project Metronome";clickProcessor->updateTrackProperties(props);
  auto clicks=std::make_shared<noori::Audio>();clicks->sampleRate=48000;clicks->channels.resize(1,std::vector<float>(96000));clicks->channels[0][14400]=.8f;clicks->channels[0][38400]=.8f;
  clickProcessor->insertSession->load([clicks]{return *clicks;},"Captured click",96000,noori::Timeline());waitSession(clickProcessor->insertSession);
  auto vocalStorage=std::make_unique<NooriProcessor>();auto& vocal=*vocalStorage;vocal.setRateAndBufferSizeDetails(48000,512);vocal.prepareToPlay(48000,512);vocal.insertSession->load([audio]{return *audio;},"Connected vocal",0,noori::Timeline());waitSession(vocal.insertSession);vocal.insertSession->attacks={{112800,112800,1,1,false},{136800,136800,1,1,false}};
  auto connected=vocal.connectedReferenceTracks();check(std::any_of(connected.begin(),connected.end(),[](const auto& t){return t.name.startsWith("Project Metronome")&&!t.editable;}),"Host-named connected reference track enumerated");
  std::unique_ptr<juce::AudioProcessorEditor> edit(vocal.createEditor());juce::ComboBox* references=nullptr;juce::TextButton* quantize=nullptr;for(int i=0;i<edit->getNumChildComponents();++i){auto* c=edit->getChildComponent(i);if(c->getName()=="Project reference track")references=dynamic_cast<juce::ComboBox*>(c);if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getName()=="Quantize vocal")quantize=b;}
  check(references&&quantize,"Connected insert dropdown and track action");int choice=0;for(int i=0;i<references->getNumItems();++i)if(references->getItemText(i).startsWith("Project Metronome"))choice=references->getItemId(i);check(choice!=0,"Captured project click offered in actual dropdown");references->setSelectedId(choice,juce::sendNotificationSync);waitSession(vocal.insertSession);
  quantize->onClick();waitSession(vocal.insertSession);check(vocal.insertSession->attacks[0].target==110400&&vocal.insertSession->attacks[1].target==134400,"Connected-track project offset matched exactly");
  auto archived=vocal.insertSession->archive(true);auto saved=std::make_shared<EditSession>();saved->restore(archived.getData(),archived.getSize(),true);waitSession(saved);check(saved->referenceProjectID==vocal.insertSession->referenceProjectID&&saved->referenceIsClick,"Project reference identity saved");
  clickProcessor.reset();auto disconnected=vocal.connectedReferenceTracks();check(std::none_of(disconnected.begin(),disconnected.end(),[](const auto& t){return t.name.startsWith("Project Metronome");}),"Removed reference disconnects safely");vocal.releaseResources();
 }
 {
  auto manualStorage=std::make_unique<NooriProcessor>();auto& manual=*manualStorage;manual.setRateAndBufferSizeDetails(48000,512);manual.prepareToPlay(48000,512);manual.insertSession->load([audio]{return *audio;},"Manual BPM fixture",0,noori::Timeline());waitSession(manual.insertSession);manual.insertSession->attacks={{38880,38880,1,1,false}};
  std::unique_ptr<juce::AudioProcessorEditor> edit(manual.createEditor());juce::Slider* tempo=nullptr;juce::TextButton* quantize=nullptr;for(int i=0;i<edit->getNumChildComponents();++i){auto* c=edit->getChildComponent(i);if(auto* slider=dynamic_cast<juce::Slider*>(c))if(slider->getTextValueSuffix()==" BPM")tempo=slider;if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getName()=="Quantize vocal")quantize=b;}
  check(tempo&&tempo->isEnabled()&&quantize,"Manual tempo fallback available without host metadata");tempo->setValue(120,juce::sendNotificationSync);quantize->onClick();waitSession(manual.insertSession);check(manual.insertSession->timeline.points()[0].bpm==120&&manual.insertSession->attacks[0].target==36000,"UI exact BPM quantization follows the selected tempo");manual.releaseResources();
 }
 // Reopen test fixture for a screenshot of the actual editor, not a design mockup.
 p.insertSession->load([audio]{return *audio;},"Synthetic test vocal | 48 kHz",0,noori::Timeline());waitSession(p.insertSession);p.insertSession->analyse();waitSession(p.insertSession);p.insertSession->align();waitSession(p.insertSession);
 p.insertSession->referenceName="Synthetic metronome reference";p.insertSession->referenceClicks={18000,36000,54000,72000,90000,108000,126000,144000,162000,180000};
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor->getWidth()>=1100,"Editor dimensions");editor->setVisible(true);
#if JucePlugin_Enable_ARA
 check(dynamic_cast<juce::AudioProcessorEditorARAExtension*>(editor.get())!=nullptr,"ARA editor lifecycle extension");
#endif
 auto button=[&](const juce::String& title){for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i)))if(b->getButtonText()==title||(title=="Quantize selected track"&&b->getName()=="Quantize vocal"))return b;return (juce::TextButton*)nullptr;};
 check(button("Undo")&&button("Redo")&&button("Import reference"),"Required editor controls visible");
 p.audition=false;p.insertSession->compare=true;auto beforeUI=p.insertSession->audible.load();button("Undo")->onClick();
 check(p.audition&&!p.insertSession->compare&&p.insertSession->audible.load()!=beforeUI,"Undo button activates edited audition");
 button("Redo")->onClick();check(p.insertSession->audible.load()==beforeUI,"Redo button restores editor result");
 check(button("Quantize selected track")->getButtonText()=="Quantize current vocal","Current vocal action is explicitly named");
 juce::ComboBox* referencePlacement=nullptr;juce::ComboBox* editVocal=nullptr;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* c=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i))){if(c->getName()=="Reference import start")referencePlacement=c;if(c->getName()=="Project target track")editVocal=c;}
 check(referencePlacement&&referencePlacement->getSelectedId()==1,"Reference imports default to vocal start, not moving cursor");check(editVocal&&editVocal->getText().contains(p.insertSession->name),"Current edit vocal identified in selector");
 check(button("Select all attacks")&&button("Quantize selected track"),"Track and attack selection actions are distinct");button("Select all attacks")->onClick();juce::TextButton* alignSelected=nullptr;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i)))if(b->getButtonText().startsWith("Align selected attacks"))alignSelected=b;check(alignSelected&&alignSelected->isEnabled(),"Select all activates Align selected attacks");alignSelected->onClick();waitSession(p.insertSession);button("Clear selection")->onClick();check(!alignSelected->isEnabled(),"Clear selection disables only attack selection action");check(button("Quantize selected track")->isEnabled(),"Track quantization stays active without marker selection");
 // Exercise actual controls, not only the session API.
 juce::ComboBox* gridControl=nullptr;juce::Slider* sensitivityControl=nullptr;for(int i=0;i<editor->getNumChildComponents();++i){auto* c=editor->getChildComponent(i);if(auto* box=dynamic_cast<juce::ComboBox*>(c))if(box->getNumItems()==4&&box->getItemText(0)=="1/4")gridControl=box;if(c->getName()=="Attack sensitivity")sensitivityControl=dynamic_cast<juce::Slider*>(c);}
 check(gridControl&&sensitivityControl,"Editable grid and sensitivity controls");gridControl->setSelectedId(3,juce::sendNotificationSync);check(p.insertSession->settings.division==16,"Grid control edit");button("Undo")->onClick();check(p.insertSession->settings.division==8&&gridControl->getSelectedId()==2,"Undo updates grid and control value");button("Redo")->onClick();check(gridControl->getSelectedId()==3,"Redo updates grid control");button("Undo")->onClick();
 sensitivityControl->setValue(77,juce::sendNotificationSync);check(std::abs(p.insertSession->settings.sensitivity-.77)<1e-8,"Sensitivity control edit");button("Undo")->onClick();check(sensitivityControl->getValue()==55,"Undo updates sensitivity control");button("Redo")->onClick();check(sensitivityControl->getValue()==77,"Redo updates sensitivity control");button("Undo")->onClick();dynamic_cast<NooriEditor*>(editor.get())->refresh();
 juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType{});juce::Graphics g(image);editor->paintEntireComponent(g,true);check(image.getPixelAt(10,10).getARGB()==0xfff5f7fb,"Editor screenshot contains painted background");
 auto output=argc>1?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):juce::File::getCurrentWorkingDirectory();output.createDirectory();juce::PNGImageFormat png;auto stream=output.getChildFile("Preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&png.writeImageToStream(image,*stream),"Editor PNG export");auto* theme=button("Dark theme");check(theme,"Light theme is the default");theme->onClick();check(!p.insertSession->lightTheme,"Theme switch updates session");juce::Image dark(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType{});juce::Graphics dg(dark);editor->paintEntireComponent(dg,true);check(dark.getPixelAt(10,10).getARGB()==0xff14121d,"Dark theme screenshot background");auto darkStream=output.getChildFile("DarkPreview.png").createOutputStream();if(darkStream){darkStream->setPosition(0);darkStream->truncate();}check(darkStream&&png.writeImageToStream(dark,*darkStream),"Dark PNG export");button("Undo")->onClick();check(p.insertSession->lightTheme,"UI undo restores theme");button("Redo")->onClick();check(!p.insertSession->lightTheme,"UI redo restores theme");button("Light theme")->onClick();
 output.getChildFile("plugin-smoke.ok").replaceWithText("Host transport, capture, render, undo/redo, save/restore and GUI construction passed. Official SDK in-process ARA model passed; native Cubase not run.\n");p.releaseResources();reopened.releaseResources();std::cout<<"PASS processor and editor smoke tests\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
