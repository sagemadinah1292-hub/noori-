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
static void waitSession(const std::shared_ptr<EditSession>& s){auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;while(s->busy&&juce::Time::getMillisecondCounterHiRes()<deadline){s->settle();juce::Thread::sleep(2);}s->settle();check(!s->busy,"Worker timed out");check(s->status.contains("ready"),s->status.toRawUTF8());}
int main(int argc,char** argv){try{
 juce::ScopedJuceInitialiser_GUI gui;
#if JucePlugin_Enable_ARA
 const auto* factory=createARAFactory();check(factory&&juce::String(factory->factoryID)=="audio.nooriturab.tempo.factory.0.1.0"&&juce::String(factory->documentArchiveID)=="audio.nooriturab.tempo.archive.1","ARA factory identifiers");
#endif
 NooriProcessor p;HostTransport host;p.setPlayHead(&host);p.setRateAndBufferSizeDetails(48000,512);p.prepareToPlay(48000,512);juce::AudioBuffer<float> b(2,512);juce::MidiBuffer midi;b.clear();p.processBlock(b,midi);check(p.bpm==80&&p.numerator==6&&p.denominator==8,"Host tempo/time signature");
 auto audio=std::make_shared<noori::Audio>();audio->sampleRate=48000;audio->channels.resize(2,std::vector<float>(48000*4));
 for(auto& c:audio->channels)for(int i=0;i<(int)c.size();++i){double time=i/48000.;double phase=std::fmod(time,.75);double envelope=phase<.04?phase/.04:phase<.50?std::exp(-(phase-.04)*4):0;c[(std::size_t)i]=float(.55*envelope*(std::sin(2*std::numbers::pi*180*time)+.2*std::sin(2*std::numbers::pi*360*time)));}
 p.insertSession->load([audio]{return *audio;},"Synthetic test vocal | 48 kHz",0,noori::Timeline());waitSession(p.insertSession);p.insertSession->align();waitSession(p.insertSession);check(!p.insertSession->attacks.empty(),"Attack analysis");
 auto wav=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("noori-export-test",".wav");
 p.exportFile(wav,p.insertSession);p.exportFile(wav,p.insertSession);juce::AudioFormatManager formats;formats.registerBasicFormats();{std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wav));check(reader&&reader->bitsPerSample==24&&reader->lengthInSamples==audio->length(),"Repeated 24-bit export replaces file correctly");}wav.deleteFile();
 check(p.insertSession->canUndo(),"Undo available");auto attacks=p.insertSession->attacks;p.insertSession->undo();check(p.insertSession->attacks.empty(),"Undo restored original");p.insertSession->redo();check(p.insertSession->attacks.size()==attacks.size(),"Redo restored edits");
 juce::MemoryBlock state;p.getStateInformation(state);NooriProcessor reopened;reopened.setRateAndBufferSizeDetails(48000,512);reopened.prepareToPlay(48000,512);reopened.setStateInformation(state.getData(),(int)state.getSize());waitSession(reopened.insertSession);check(reopened.insertSession->attacks.size()==attacks.size(),"Saved project restored attacks");check(reopened.insertSession->original->channels==audio->channels,"Float source restored losslessly");
 p.audition=true;host.sample=48000;p.processBlock(b,midi);double energy=0;for(int i=0;i<b.getNumSamples();++i)energy+=std::abs(b.getSample(0,i));check(energy>1,"Host-synchronized audition");
 p.beginCapture();for(int block=0;block<20;++block){host.sample=block*512;b.clear();b.addFrom(0,0,audio->channels[0].data()+block*512,512);b.addFrom(1,0,audio->channels[1].data()+block*512,512);p.processBlock(b,midi);}p.finishCapture();waitSession(p.insertSession);check(p.insertSession->original->length()==10240,"Capture stream length");
 // Reopen test fixture for a screenshot of the actual editor, not a design mockup.
 p.insertSession->load([audio]{return *audio;},"Synthetic test vocal | 48 kHz",0,noori::Timeline());waitSession(p.insertSession);p.insertSession->analyse();waitSession(p.insertSession);
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());check(editor->getWidth()>=1100,"Editor dimensions");
#if JucePlugin_Enable_ARA
 check(dynamic_cast<juce::AudioProcessorEditorARAExtension*>(editor.get())!=nullptr,"ARA editor lifecycle extension");
#endif
 juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true);juce::Graphics g(image);editor->paintEntireComponent(g,true);
 auto output=argc>1?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):juce::File::getCurrentWorkingDirectory();output.createDirectory();juce::PNGImageFormat png;auto stream=output.getChildFile("Preview.png").createOutputStream();check(stream&&png.writeImageToStream(image,*stream),"Editor PNG export");output.getChildFile("plugin-smoke.ok").replaceWithText("Host transport, capture, render, undo/redo, save/restore and GUI construction passed. ARA host tests NOT run.\n");p.releaseResources();reopened.releaseResources();std::cout<<"PASS processor and editor smoke tests\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
