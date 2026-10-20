#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <numbers>
static void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}
class HostTransport final:public juce::AudioPlayHead {
public:
 juce::Optional<PositionInfo> getPosition()const override {PositionInfo p;p.setBpm(80);p.setTimeSignature(TimeSignature{6,8});p.setTimeInSamples(sample);p.setTimeInSeconds(sample/48000.);p.setPpqPosition(sample*80./(60*48000));p.setIsPlaying(true);return p;}
 juce::int64 sample=0;
};
static void waitSession(const std::shared_ptr<EditSession>& s,bool success=true){auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(s->busy&&juce::Time::getMillisecondCounterHiRes()<deadline){s->settle();juce::Thread::sleep(2);}s->settle();check(!s->busy,"Worker timed out");if(success)check(s->status.contains("ready"),s->status.toRawUTF8());}
int main(int argc,char** argv){try{
 juce::ScopedJuceInitialiser_GUI gui;
#if JucePlugin_Enable_ARA
 const auto* factory=createARAFactory();check(factory&&juce::String(factory->factoryID)=="audio.nooriturab.tempo.factory.0.1.1"&&juce::String(factory->documentArchiveID)=="audio.nooriturab.tempo.archive.2","ARA factory identifiers");
#endif
 NooriProcessor p;HostTransport host;p.setPlayHead(&host);p.setRateAndBufferSizeDetails(48000,512);p.prepareToPlay(48000,512);juce::AudioBuffer<float> b(2,512);juce::MidiBuffer midi;b.clear();p.processBlock(b,midi);check(p.bpm==80&&p.numerator==6&&p.denominator==8,"Host tempo/time signature");
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
 auto priorBuffer=p.insertSession->audible.load();p.insertSession->align();waitSession(p.insertSession,false);check(p.insertSession->status.contains("Exact click match")&&p.insertSession->audible.load()==priorBuffer,p.insertSession->status.toRawUTF8());clickFile.deleteFile();
 check(p.insertSession->referenceClicks==std::vector<noori::Sample>{13000,37000},"Reference import project anchor");
 juce::MemoryBlock state;p.getStateInformation(state);NooriProcessor reopened;reopened.setRateAndBufferSizeDetails(48000,512);reopened.prepareToPlay(48000,512);reopened.setStateInformation(state.getData(),(int)state.getSize());waitSession(reopened.insertSession);check(reopened.insertSession->referenceClicks==p.insertSession->referenceClicks&&reopened.insertSession->useReference,"Saved reference clicks restored");check(reopened.insertSession->attacks.size()==attacks.size(),"Saved project restored attacks");check(reopened.insertSession->original->channels==audio->channels,"Float source restored losslessly");
 p.audition=true;host.sample=48000;p.processBlock(b,midi);double energy=0;for(int i=0;i<b.getNumSamples();++i)energy+=std::abs(b.getSample(0,i));check(energy>1,"Host-synchronized audition");
 p.beginCapture();for(int block=0;block<20;++block){host.sample=block*512;b.clear();b.addFrom(0,0,audio->channels[0].data()+block*512,512);b.addFrom(1,0,audio->channels[1].data()+block*512,512);p.processBlock(b,midi);}p.finishCapture();waitSession(p.insertSession);check(p.insertSession->original->length()==10240,"Capture stream length");
 // Reopen test fixture for a screenshot of the actual editor, not a design mockup.
 p.insertSession->load([audio]{return *audio;},"Synthetic test vocal | 48 kHz",0,noori::Timeline());waitSession(p.insertSession);p.insertSession->analyse();waitSession(p.insertSession);p.insertSession->align();waitSession(p.insertSession);
 p.insertSession->referenceName="Synthetic metronome reference";p.insertSession->referenceClicks={18000,36000,54000,72000,90000,108000,126000,144000,162000,180000};
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor->getWidth()>=1100,"Editor dimensions");
#if JucePlugin_Enable_ARA
 check(dynamic_cast<juce::AudioProcessorEditorARAExtension*>(editor.get())!=nullptr,"ARA editor lifecycle extension");
#endif
 auto button=[&](const juce::String& title){for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i)))if(b->getButtonText()==title)return b;return (juce::TextButton*)nullptr;};
 check(button("Undo")&&button("Redo")&&button("Import metronome click"),"Required editor controls visible");
 p.audition=false;p.insertSession->compare=true;auto beforeUI=p.insertSession->audible.load();button("Undo")->onClick();
 check(p.audition&&!p.insertSession->compare&&p.insertSession->audible.load()!=beforeUI,"Undo button activates edited audition");
 button("Redo")->onClick();check(p.insertSession->audible.load()==beforeUI,"Redo button restores editor result");
 juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType{});juce::Graphics g(image);editor->paintEntireComponent(g,true);check(image.getPixelAt(10,10).getARGB()==0xff14121d,"Editor screenshot contains painted background");
 auto output=argc>1?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):juce::File::getCurrentWorkingDirectory();output.createDirectory();juce::PNGImageFormat png;auto stream=output.getChildFile("Preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&png.writeImageToStream(image,*stream),"Editor PNG export");output.getChildFile("plugin-smoke.ok").replaceWithText("Host transport, capture, render, undo/redo, save/restore and GUI construction passed. ARA host tests NOT run.\n");p.releaseResources();reopened.releaseResources();std::cout<<"PASS processor and editor smoke tests\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
