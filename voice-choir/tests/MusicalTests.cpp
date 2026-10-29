#include "MusicalEngine.h"
#include <iostream>
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
double frequency(const std::vector<float>& x,double rate){int count=0;int start=int(rate*.4),end=int(x.size()-rate*.3);for(int i=start+1;i<end;++i)if(x[i-1]<=0&&x[i]>0)++count;return count*rate/(end-start);}
int main(){try{using namespace noori_choir;std::atomic<bool> cancel{false};
 check(harmonySemitones(67,Part::ThirdAbove,7,0)==4,"G major third");check(harmonySemitones(69,Part::ThirdAbove,7,0)==3,"A diatonic third is minor in G major");check(harmonySemitones(67,Part::ThirdAbove,7,1)==3,"G minor third");check(harmonySemitones(67.24,Part::FifthBelow,7,0)==-7,"Expressive cents retained");
 check(harmonySemitones(62,Part::ThirdAbove,0,4,661)==5,"Custom pentatonic harmony follows selected pitch classes");
 for(double rate:{44100.,48000.,96000.}){const int n=int(rate*1.2);std::vector<float> source(n);for(int i=0;i<n;++i)source[i]=float(.25*std::sin(2*pi*220*i/rate));
 auto melody=analyseMelody(source.data(),n,rate,cancel);check(std::abs(melody.at(n/2).hz-220)<2,"Pitch analysis");
 ChoirConfig config;config.character=7;config.variation=0;
 for(auto part:{Part::Main,Part::OctaveUp,Part::OctaveDown,Part::ThirdAbove}){auto voice=renderSinger(source.data(),n,rate,melody,part,1,config,cancel);check(int(voice.size())==n,"Exact duration");for(auto value:voice)check(std::isfinite(value)&&std::abs(value)<=1,"Bounded rendered audio");double target=part==Part::OctaveUp?440:part==Part::OctaveDown?110:part==Part::ThirdAbove?261.63:220;check(std::abs(frequency(voice,rate)-target)<5,"Actual audio follows musical part");}
 }
 // Known major/minor melodies in several keys, with tonic/cadence emphasis.
 for(int mode=0;mode<2;++mode)for(int key:{0,7,9}){Melody m;for(int degree=0;degree<7;++degree)for(int n=0;n<(degree==0?120:degree==2?65:degree==4?80:18);++n){const int note=60+key+scaleNotes(mode)[degree];m.points.push_back({float(440*std::pow(2.,(note-69)/12.)),.99f,.2f});}auto detected=estimateKey(m);check(detected.key==key&&detected.scale==mode&&detected.confidence>=.12f,"Automatic major/minor key from melody");}
 Melody oneNote;oneNote.points.assign(100,{220,.99f,.2f});check(estimateKey(oneNote).confidence==0,"Do not invent a key from one note");
 // Main remains exactly the original, including quiet syllables and breaths.
 std::vector<float> original(48000);for(int i=0;i<int(original.size());++i)original[i]=float(.13*std::sin(i*.03));Melody gaps;gaps.hop=960;gaps.points.assign(50,{220,.99f,.1f});for(int i=10;i<20;++i)gaps.points[i]={};ChoirConfig clean;clean.variation=0;
 check(renderSinger(original.data(),int(original.size()),48000,gaps,Part::Main,0,clean,cancel)==original,"Main does not pass through pitch/formant/drive processing");
 auto sustained=renderSinger(original.data(),int(original.size()),48000,gaps,Part::OctaveUp,1,clean,cancel);double gapEnergy=0;for(int i=12000;i<17000;++i)gapEnergy+=sustained[i]*sustained[i];check(gapEnergy>1,"Low-confidence windows must not chop quiet vocal audio");
 std::vector<float> loud(48000);for(int i=0;i<int(loud.size());++i)loud[i]=float(1.3*std::sin(2*pi*220*i/48000.));auto loudMelody=analyseMelody(loud.data(),int(loud.size()),48000,cancel);ChoirConfig bright;bright.character=1;bright.variation=0;
 auto safe=renderSinger(loud.data(),int(loud.size()),48000,loudMelody,Part::Unison,1,bright,cancel);for(float value:safe)check(std::abs(value)<=.981f,"Generated singer keeps headroom without internal hard clipping");
 ChoirConfig config;check(arrange(config).size()==8,"Eight parts");config.singers=2;bool rejected=false;try{arrange(config);}catch(...){rejected=true;}check(rejected,"Reject more roles than singers");
 std::cout<<"PASS melody, diatonic intervals, real octave/harmony audio, exact duration and part counts\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
