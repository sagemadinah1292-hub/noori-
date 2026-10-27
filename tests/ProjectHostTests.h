#pragma once
#if JucePlugin_Enable_ARA
#include <ARA_Library/Dispatch/ARAHostDispatch.h>
// A minimal in-process ARA host: tests project enumeration and actual dropdown
// bindings without claiming that this replaces acceptance in Cubase.
class TestAudioAccess final:public ARA::Host::AudioAccessControllerInterface {
 ARA::ARAAudioReaderHostRef createAudioReaderForSource(ARA::ARAAudioSourceHostRef,bool)noexcept override{return nullptr;}
 bool readAudioSamples(ARA::ARAAudioReaderHostRef,ARA::ARASamplePosition,ARA::ARASampleCount,void* const[])noexcept override{return false;}
 void destroyAudioReader(ARA::ARAAudioReaderHostRef)noexcept override{}
};
class TestArchives final:public ARA::Host::ArchivingControllerInterface {
 ARA::ARASize getArchiveSize(ARA::ARAArchiveReaderHostRef)noexcept override{return 0;}
 bool readBytesFromArchive(ARA::ARAArchiveReaderHostRef,ARA::ARASize,ARA::ARASize,ARA::ARAByte[])noexcept override{return false;}
 bool writeBytesToArchive(ARA::ARAArchiveWriterHostRef,ARA::ARASize,ARA::ARASize,const ARA::ARAByte[])noexcept override{return false;}
 void notifyDocumentArchivingProgress(float)noexcept override{}
 void notifyDocumentUnarchivingProgress(float)noexcept override{}
 ARA::ARAPersistentID getDocumentArchiveID(ARA::ARAArchiveReaderHostRef)noexcept override{return "audio.nooriturab.tempo.archive.3";}
};
inline void projectHostTests(const ARA::ARAFactory* factory,const juce::File& output){
 std::cerr<<"ARA regression: initialize factory\n";
 ARA::ARAInterfaceConfiguration config{};config.structSize=sizeof(config);config.desiredApiGeneration=factory->highestSupportedApiGeneration;factory->initializeARAWithConfiguration(&config);
 TestAudioAccess audioAccess;TestArchives archives;ARA::Host::DocumentControllerHostInstance host(&audioAccess,&archives);
 ARA::ARADocumentProperties document{};document.structSize=sizeof(document);document.name="Noori regression project";
 std::cerr<<"ARA regression: create document\n";
 auto* instance=factory->createDocumentControllerWithDocument(&host,&document);check(instance!=nullptr,"ARA document construction");ARA::Host::DocumentController controller(instance);
 std::vector<ARA::ARARegionSequenceRef> sequences;std::vector<ARA::ARAAudioSourceRef> sources;std::vector<ARA::ARAAudioModificationRef> modifications;std::vector<ARA::ARAPlaybackRegionRef> regions;
 int identifiers[9]{};
 std::cerr<<"ARA regression: create musical context\n";
 controller.beginEditing();ARA::ARAMusicalContextProperties contextProps{};contextProps.structSize=sizeof(contextProps);contextProps.name="Project tempo";auto context=controller.createMusicalContext(reinterpret_cast<ARA::ARAMusicalContextHostRef>(&identifiers[8]),&contextProps);
 for(int i=0;i<2;++i){const char* name=i?"Metronome project track":"Lead project track";
  std::cerr<<"ARA regression: create sequence "<<i<<"\n";
  ARA::ARARegionSequenceProperties sequence{};sequence.structSize=sizeof(sequence);sequence.name=name;sequence.orderIndex=i;sequence.musicalContextRef=context;sequences.push_back(controller.createRegionSequence(reinterpret_cast<ARA::ARARegionSequenceHostRef>(&identifiers[i]),&sequence));
  std::cerr<<"ARA regression: create source "<<i<<"\n";
  ARA::ARAAudioSourceProperties source{};source.structSize=sizeof(source);source.name=name;source.persistentID=i?"click-source":"lead-source";source.sampleCount=96000;source.sampleRate=48000;source.channelCount=1;sources.push_back(controller.createAudioSource(reinterpret_cast<ARA::ARAAudioSourceHostRef>(&identifiers[i+2]),&source));
  std::cerr<<"ARA regression: create modification "<<i<<"\n";
  ARA::ARAAudioModificationProperties modification{};modification.structSize=sizeof(modification);modification.name=name;modification.persistentID=i?"click-edit":"lead-edit";modifications.push_back(controller.createAudioModification(sources.back(),reinterpret_cast<ARA::ARAAudioModificationHostRef>(&identifiers[i+4]),&modification));
  std::cerr<<"ARA regression: create region "<<i<<"\n";
  ARA::ARAPlaybackRegionProperties region{};region.structSize=sizeof(region);region.name=name;region.regionSequenceRef=sequences.back();region.startInModificationTime=.25;region.durationInModificationTime=1;region.startInPlaybackTime=10;region.durationInPlaybackTime=1;regions.push_back(controller.createPlaybackRegion(modifications.back(),reinterpret_cast<ARA::ARAPlaybackRegionHostRef>(&identifiers[i+6]),&region));
 }
 controller.endEditing();
 {
  std::cerr<<"ARA regression: bind processor\n";
  auto processorStorage=std::make_unique<NooriProcessor>();auto& p=*processorStorage;p.bindToARA(instance->documentControllerRef,ARA::kARAPlaybackRendererRole|ARA::kARAEditorRendererRole|ARA::kARAEditorViewRole,ARA::kARAEditorViewRole);p.setRateAndBufferSizeDetails(48000,512);p.prepareToPlay(48000,512);
  std::cerr<<"ARA regression: enumerate tracks\n";
  auto tracks=availableProjectTracks(p);check(tracks.size()==2&&tracks[1].name=="Metronome project track","Enumerate actual ARA project tracks, including unselected reference");
  for(int i=0;i<2;++i){auto a=std::make_shared<noori::Audio>();a->sampleRate=48000;a->channels.resize(1,std::vector<float>(96000));for(auto sample:i?std::vector<int>{14400,38400,76800}:std::vector<int>{16800,40800,76800})for(int j=0;j<1200;++j)a->channels[0][(std::size_t)(sample+j)]=.8f*(float)std::exp(-j/300.)*(float)std::cos(2*std::numbers::pi*1000*j/48000.);auto s=tracks[(std::size_t)i].regions[0].session;s->load([a]{return *a;},tracks[(std::size_t)i].name,468000,noori::Timeline());waitSession(s);}
  auto lead=tracks[0].regions[0].session;lead->attacks={{16800,16800,1,1,false},{40800,40800,1,1,false},{76800,76800,1,1,false}};
  std::cerr<<"ARA regression: create editor\n";
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());editor->setVisible(true);
  auto combo=[&](const juce::String& name){for(int i=0;i<editor->getNumChildComponents();++i)if(auto* c=dynamic_cast<juce::ComboBox*>(editor->getChildComponent(i)))if(c->getName()==name)return c;return (juce::ComboBox*)nullptr;};
  auto* target=combo("Project target track");auto* reference=combo("Project reference track");check(target&&reference&&target->getNumItems()==3&&reference->getNumItems()==4,"Real project tracks populate both dropdowns");target->setSelectedId(100,juce::sendNotificationSync);reference->setSelectedId(101,juce::sendNotificationSync);waitSession(lead);
  check(lead->referenceClicks==std::vector<noori::Sample>{14400,38400},"Project reference honours trimmed audio and project offset");
  juce::TextButton* quantize=nullptr;for(int i=0;i<editor->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(editor->getChildComponent(i)))if(b->getName()=="Quantize vocal")quantize=b;
  check(quantize&&quantize->isEnabled(),"Selected project track action active without selecting markers");quantize->onClick();waitSession(lead);check(lead->attacks[0].target==14400&&lead->attacks[1].target==38400&&lead->attacks[2].target==76800,"Track quantization matches reference and preserves attacks outside trimmed event");
  check(std::abs(lead->snapshot()->channels[0][14400]-.8f)<1e-7,"ARA project-reference transient rendered at exact project-mapped sample");
  lead->overlay=true;dynamic_cast<NooriEditor*>(editor.get())->refresh();
  output.createDirectory();juce::Image image(juce::Image::ARGB,editor->getWidth(),editor->getHeight(),true,juce::SoftwareImageType{});juce::Graphics graphics(image);editor->paintEntireComponent(graphics,true);juce::PNGImageFormat png;auto stream=output.getChildFile("ProjectPreview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}check(stream&&png.writeImageToStream(image,*stream),"ARA project editor preview");
  p.releaseResources();
 }
 controller.beginEditing();for(int i=1;i>=0;--i){controller.destroyPlaybackRegion(regions[(std::size_t)i]);controller.destroyAudioModification(modifications[(std::size_t)i]);controller.destroyAudioSource(sources[(std::size_t)i]);controller.destroyRegionSequence(sequences[(std::size_t)i]);}controller.destroyMusicalContext(context);controller.endEditing();controller.destroyDocumentController();factory->uninitializeARA();
}
#endif
