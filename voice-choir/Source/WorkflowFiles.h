#pragma once
#include "OfflineFiles.h"
#include "MusicalEngine.h"
#include <bit>
namespace noori_choir {
struct Stem {
 juce::String name;Part part=Part::Main;int singer=0;
 std::shared_ptr<const VocalFile> dry,wet;
};
struct ChoirSession { std::shared_ptr<const VocalFile> source;std::vector<Stem> stems;ChoirConfig config;FxConfig fx;juce::String cacheId; };
struct StemMix { float gain=0,pan=0;bool mute=false,solo=false; };
inline std::shared_ptr<VocalFile> postProcess(const VocalFile& input,const FxConfig& fx,const std::atomic<bool>& cancelled) {
 auto out=std::make_shared<VocalFile>();out->rate=input.rate;out->name=input.name;out->sampleOffset=input.sampleOffset;out->audio.makeCopyOf(input.audio);
 const double hp=std::exp(-2*pi*std::clamp(fx.lowCut,0.f,500.f)/input.rate),lp=std::exp(-2*pi*2500/input.rate),highGain=std::pow(10.,fx.brightness/20.);
 for(int ch=0;ch<out->audio.getNumChannels();++ch){double x1=0,y1=0,low=0;auto* samples=out->audio.getWritePointer(ch);
  for(int i=0;i<out->audio.getNumSamples();++i){if((i&8191)==0&&cancelled)throw std::runtime_error("Cancelled.");double x=samples[i];double high=fx.lowCut>0?hp*(y1+x-x1):x;x1=x;y1=high;low=lp*low+(1-lp)*high;samples[i]=float(low+(high-low)*highGain);}
 }
 if(fx.reverb>0){juce::Reverb reverb;reverb.setSampleRate(input.rate);juce::Reverb::Parameters parameters;parameters.roomSize=.55f;parameters.damping=.5f;parameters.wetLevel=std::clamp(fx.reverb,0.f,1.f)*.35f;parameters.dryLevel=1;parameters.width=1;reverb.setParameters(parameters);
  for(int offset=0;offset<out->audio.getNumSamples();offset+=4096){if(cancelled)throw std::runtime_error("Cancelled.");const int n=std::min(4096,out->audio.getNumSamples()-offset);if(out->audio.getNumChannels()==1)reverb.processMono(out->audio.getWritePointer(0,offset),n);else reverb.processStereo(out->audio.getWritePointer(0,offset),out->audio.getWritePointer(1,offset),n);}
 }
 return out;
}
// Explicit little-endian WAV writing supports PCM24 and IEEE float32. BWF timing
// is optional and uses the source sample reference; duration is never extended.
inline void writeWave(const juce::File& target,const VocalFile& source,int bits,bool bwf,
 const std::atomic<bool>& cancelled,StemMix mix={},bool forceMono=false) {
 const int frames=source.audio.getNumSamples(),channels=forceMono?1:source.audio.getNumChannels(),bytes=bits==32?4:3;
 const juce::int64 dataSize=juce::int64(frames)*channels*bytes;const int bextSize=bwf?610:0;
 if(dataSize>0xffffffffLL-36-bextSize-1)throw std::runtime_error("WAV exceeds RIFF size limit.");
 auto stream=target.createOutputStream();if(!stream)throw std::runtime_error("Cannot create WAV file.");stream->setPosition(0);stream->truncate();
 stream->write("RIFF",4);stream->writeInt(int(36+bextSize+dataSize+(dataSize&1)));stream->write("WAVE",4);
 if(bwf){stream->write("bext",4);stream->writeInt(602);std::array<char,602> chunk{};const std::string description="NOORI CHOIR synchronized vocal stem";std::copy(description.begin(),description.end(),chunk.begin());auto reference=std::uint64_t(std::max(juce::int64(0),source.sampleOffset));for(int i=0;i<8;++i)chunk[338+i]=char(reference>>(8*i));chunk[346]=1;stream->write(chunk.data(),chunk.size());}
 stream->write("fmt ",4);stream->writeInt(16);stream->writeShort(bits==32?3:1);stream->writeShort(short(channels));stream->writeInt(int(source.rate));stream->writeInt(int(source.rate)*channels*bytes);stream->writeShort(short(channels*bytes));stream->writeShort(short(bits));stream->write("data",4);stream->writeInt(int(dataSize));
 const float gain=mix.mute?0.f:std::pow(10.f,mix.gain/20.f);
 std::array<float,2> balance{std::sqrt(std::max(0.f,1-mix.pan)),std::sqrt(std::max(0.f,1+mix.pan))};
 juce::MemoryOutputStream block;
 for(int offset=0;offset<frames;offset+=4096){if(cancelled)throw std::runtime_error("Cancelled.");block.reset();const int n=std::min(4096,frames-offset);
  for(int i=0;i<n;++i)for(int ch=0;ch<channels;++ch){float x=source.audio.getSample(std::min(ch,source.audio.getNumChannels()-1),offset+i);if(forceMono&&source.audio.getNumChannels()==2)x=.5f*(source.audio.getSample(0,offset+i)+source.audio.getSample(1,offset+i));x*=gain*(channels==2?balance[ch]:1.f);if(!std::isfinite(x))throw std::runtime_error("Non-finite export audio.");
   if(bits==32)block.writeInt(std::bit_cast<std::int32_t>(x));else {auto value=std::uint32_t(std::int32_t(std::round(std::clamp(x,-1.f,1.f)*8388607)));char packed[]{char(value),char(value>>8),char(value>>16)};block.write(packed,3);}
  }
  if(!stream->write(block.getData(),block.getDataSize()))throw std::runtime_error("Disk write failed.");
 }
 if(dataSize&1)stream->writeByte(0);stream->flush();if(stream->getStatus().failed())throw std::runtime_error("WAV flush failed.");
}
inline juce::File cacheRoot(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NOORI CHOIR").getChildFile("Cache");}
inline juce::StringArray exportSession(const ChoirSession& session,const juce::File& folder,
 const std::vector<StemMix>& mix,int bits,bool printFx,bool bwf,int selected,const std::atomic<bool>& cancelled,std::atomic<float>& progress) {
 if(!folder.isDirectory())throw std::runtime_error("Select an existing output folder.");
 if(session.stems.empty())throw std::runtime_error("Create vocals first.");
 const auto id=juce::Uuid().toString();auto staging=folder.getChildFile(".noori-"+id);if(staging.createDirectory().failed())throw std::runtime_error("Cannot create export staging folder.");
 juce::StringArray published;
 try{
  const int count=selected>=0?1:int(session.stems.size());
  if(selected>=int(session.stems.size()))throw std::runtime_error("Invalid selected stem.");
  if(selected==-2){ // explicit combined mix, separate from the all-stems path
   VocalFile combined;combined.rate=session.source->rate;combined.sampleOffset=session.source->sampleOffset;combined.audio.setSize(session.config.stereo?2:1,session.source->audio.getNumSamples());combined.audio.clear();
   bool anySolo=std::any_of(mix.begin(),mix.end(),[](auto m){return m.solo;});
   for(int v=0;v<int(session.stems.size());++v){auto m=mix[v];if(m.mute||(anySolo&&!m.solo))continue;const auto& data=printFx?*session.stems[v].wet:*session.stems[v].dry;const float gain=std::pow(10.f,m.gain/20.f)/std::sqrt(float(session.stems.size()));
    for(int ch=0;ch<combined.audio.getNumChannels();++ch){float pan=combined.audio.getNumChannels()==1?1.f:std::sqrt(std::max(0.f,ch==0?1-m.pan:1+m.pan));combined.audio.addFrom(ch,0,data.audio,std::min(ch,data.audio.getNumChannels()-1),0,combined.audio.getNumSamples(),gain*pan);}
   }
   writeWave(staging.getChildFile("NOORI_CHOIR_Mix_"+id+".wav"),combined,bits,bwf,cancelled);
  }else for(int index=0;index<count;++index){const int v=selected>=0?selected:index;const auto& stem=session.stems[v];const auto name="NOORI_CHOIR_"+juce::String(v+1).paddedLeft('0',2)+"_"+juce::File::createLegalFileName(stem.name).replaceCharacter(' ','_')+"_"+id+".wav";
   auto stemMix=mix[v];stemMix.mute=false; // mute/solo affect audition and mix, not individual file availability
   writeWave(staging.getChildFile(name),printFx?*stem.wet:*stem.dry,bits,bwf,cancelled,stemMix);progress=float(index+1)/count;
  }
  auto files=staging.findChildFiles(juce::File::findFiles,false,"*.wav");for(const auto& file:files){if(cancelled)throw std::runtime_error("Cancelled.");auto target=folder.getChildFile(file.getFileName());if(target.exists()||!file.moveFileTo(target))throw std::runtime_error("Cannot publish exported WAV.");published.add(target.getFullPathName());}
  staging.deleteRecursively();progress=1;return published;
 }catch(...){for(auto& f:published)juce::File(f).deleteFile();staging.deleteRecursively();throw;}
}
}
