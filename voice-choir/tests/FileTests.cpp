#include "Plugin.h"
#include <iostream>
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void wait(ChoirProcessor& p){auto deadline=juce::Time::getMillisecondCounterHiRes()+90000;while(p.isBusy()){check(juce::Time::getMillisecondCounterHiRes()<deadline,"Job timeout");juce::Thread::sleep(2);}}
int main(int argc,char** argv){try{
 juce::ScopedJuceInitialiser_GUI init;
 const auto out=argc>1?juce::File(argv[1]):juce::File::getCurrentWorkingDirectory();
 const auto project=out.getChildFile("WavTest-"+juce::Uuid().toString());check(project.createDirectory().wasOk(),"Test project");
 struct Cleanup{juce::File dir;~Cleanup(){dir.deleteRecursively();}} cleanup{project};
 std::atomic<bool> cancel{false};std::atomic<float> progress{0};
 for(int channels:{1,2})for(double rate:{44100.,48000.,96000.}){
  const auto input=project.getChildFile("Vocal-"+juce::String(channels)+"-"+juce::String(rate,0)+".wav");
  const int frames=int(rate*.12)+13;juce::AudioBuffer<float> audio(channels,frames);audio.clear();
  for(int ch=0;ch<channels;++ch)for(int i=100;i<frames-100;++i)audio.setSample(ch,i,float(.35*std::sin(i*.09+ch*.5)));
  juce::WavAudioFormat format;auto stream=input.createOutputStream();check(bool(stream),"Input fixture stream");
  std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,unsigned(channels),24,{},0));check(bool(writer),"Fixture writer");stream.release();check(writer->writeFromAudioSampleBuffer(audio,0,frames),"Fixture write");writer.reset();
  ChoirProcessor p;p.importWav(input);wait(p);check(p.hasSource(),"Background WAV import");p.exportTo(project);wait(p);
  const auto files=p.exportedFiles();check(files.size()==8,"Exactly eight exported files");
  std::vector<juce::AudioBuffer<float>> rendered;
  for(const auto& path:files){auto vocal=noori_choir::readWav(juce::File(path),cancel);check(vocal.rate==rate&&vocal.audio.getNumChannels()==channels&&vocal.audio.getNumSamples()==frames,"Preserved rate, channels and exact duration");check(juce::File(path).getParentDirectory()==project.getChildFile("NOORI CHOIR"),"Project output location");
   for(int ch=0;ch<channels;++ch)for(int i=0;i<100;++i){check(vocal.audio.getSample(ch,i)==0,"Leading silence retained");check(vocal.audio.getSample(ch,frames-1-i)==0,"Trailing silence retained");}
   rendered.push_back(std::move(vocal.audio));
  }
  for(int a=0;a<8;++a)for(int b=a+1;b<8;++b){double delta=0;for(int i=0;i<frames;++i)delta+=std::abs(rendered[a].getSample(0,i)-rendered[b].getSample(0,i));check(delta>.01,"Separate independently varied WAV contents");}
  p.importWav(project.getChildFile("missing.wav"));wait(p);check(p.hasSource(),"Failed import retains previous vocal");
  const auto source=noori_choir::readWav(input,cancel);
  const auto dest=project.getChildFile("NOORI CHOIR");const auto before=dest.getNumberOfChildFiles(juce::File::findFiles);
  cancel=true;bool rejected=false;try{noori_choir::exportWavs(source,project,8,12,cancel,progress);}catch(...){rejected=true;}cancel=false;
  check(rejected&&dest.getNumberOfChildFiles(juce::File::findFiles)==before,"Cancellation publishes no partial files");
  check(dest.getNumberOfChildFiles(juce::File::findDirectories)==0,"Staging cleaned after cancellation");
 }
 check(out.getChildFile("choir-files.ok").replaceWithText("PASS background import, eight WAV stems, rate/channels/frame count, independent audio, cancellation and failure recovery.\n"),"File test marker");
 std::cout<<"PASS WAV integration tests\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
