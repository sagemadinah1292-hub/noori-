#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <vector>
#include "../ThirdParty/signalsmith-stretch.h"

namespace noori_choir {
constexpr double pi = 3.14159265358979323846;
enum class Part { Main, Unison, OctaveUp, OctaveDown, ThirdAbove, ThirdBelow, FifthAbove, FifthBelow };
inline const char* partName(Part p) {
 static const char* names[]{"Main","Unison","Octave Up","Octave Down","Third Above","Third Below","Fifth Above","Fifth Below"};
 return names[int(p)];
}
struct ChoirConfig {
 int singers=8, character=0, microphone=0, key=7, scale=0, customMask=2741;
 bool octaveUp=true, octaveDown=true, thirdAbove=true, thirdBelow=true, fifthAbove=true, fifthBelow=true, stereo=true;
 float variation=6.f;
};
struct FxConfig { float lowCut=40, brightness=0, reverb=.12f; bool print=true; };
inline std::vector<Part> arrange(const ChoirConfig& c) {
 if(c.singers<2||c.singers>16)throw std::runtime_error("Choose 2 to 16 singers.");
 std::vector<Part> parts{Part::Main};
 for(auto pair: {std::pair{c.octaveUp,Part::OctaveUp},{c.octaveDown,Part::OctaveDown},
      {c.thirdAbove,Part::ThirdAbove},{c.thirdBelow,Part::ThirdBelow},{c.fifthAbove,Part::FifthAbove},{c.fifthBelow,Part::FifthBelow}})
  if(pair.first)parts.push_back(pair.second);
 if(int(parts.size())>c.singers)throw std::runtime_error("More parts selected than singers. Increase singers or turn off some parts.");
 while(int(parts.size())<c.singers)parts.push_back(Part::Unison);
 return parts;
}
inline const std::array<int,7>& scaleNotes(int scale) {
 static const std::array<std::array<int,7>,4> notes{{{{0,2,4,5,7,9,11}},{{0,2,3,5,7,8,10}},{{0,2,3,5,7,8,11}},{{0,2,3,5,7,9,11}}}};
 return notes[std::clamp(scale,0,3)];
}
// Anchor to the nearest scale degree, then preserve the input note's expressive
// cents offset while moving the requested number of diatonic steps.
inline double harmonySemitones(double midi, Part part, int key, int scale, int customMask=2741) {
 if(part==Part::OctaveUp)return 12; if(part==Part::OctaveDown)return -12;
 int steps=part==Part::ThirdAbove?2:part==Part::ThirdBelow?-2:part==Part::FifthAbove?4:part==Part::FifthBelow?-4:0;
 if(!steps)return 0;
 std::vector<int> notes;if(scale==4){for(int n=0;n<12;++n)if(customMask&(1<<n))notes.push_back(n);if(notes.empty())throw std::runtime_error("Custom scale needs at least one note.");}else {const auto& selected=scaleNotes(scale);notes.assign(selected.begin(),selected.end());}
 const int degrees=int(notes.size());int nearest=0;double distance=1e9;
 auto noteAt=[&](int degree){int octave=int(std::floor(double(degree)/degrees));return key+12*octave+notes[degree-degrees*octave];};
 for(int d=-3*degrees;d<=15*degrees;++d){double delta=std::abs(noteAt(d)-midi);if(delta<distance){distance=delta;nearest=d;}}
 return noteAt(nearest+steps)-noteAt(nearest);
}
struct PitchPoint { float hz=0, confidence=0, rms=0; };
struct Melody { int hop=256; double rate=48000;std::vector<PitchPoint> points;
 PitchPoint at(int frame)const {if(points.empty())return {};return points[std::clamp(frame/hop,0,int(points.size())-1)];}
};
inline Melody analyseMelody(const float* input,int frames,double rate,const std::atomic<bool>& cancel,std::function<void(float)> progress={}) {
 const int decimation=std::max(1,int(rate/12000));const double dsRate=rate/decimation;
 std::vector<float> mono((frames+decimation-1)/decimation);
 for(int i=0;i<int(mono.size());++i){double sum=0;int n=0;for(int j=0;j<decimation&&i*decimation+j<frames;++j){sum+=input[i*decimation+j];++n;}mono[i]=float(sum/std::max(1,n));}
 Melody result;result.hop=std::max(64,int(rate*.020));result.rate=rate;
 const int window=std::max(512,int(dsRate*.085)),minLag=std::max(2,int(dsRate/1000)),maxLag=std::min(window/2,int(dsRate/65));
 std::vector<double> signal(window),difference(maxLag+1);
 for(int frame=0;frame<frames;frame+=result.hop){
  if(cancel)throw std::runtime_error("Cancelled.");
  const int centre=frame/decimation;double energy=0,mean=0;
  for(int i=0;i<window;++i){int p=centre-window/2+i;signal[i]=p>=0&&p<int(mono.size())?mono[p]:0;mean+=signal[i];}
  mean/=window;for(auto& x:signal){x-=mean;energy+=x*x;}
  PitchPoint point;point.rms=float(std::sqrt(energy/window));
  if(point.rms>1e-4f){
   difference[0]=1;double cumulative=0;
   for(int tau=1;tau<=maxLag;++tau){double sum=0;for(int i=0;i<window-maxLag;++i){double d=signal[i]-signal[i+tau];sum+=d*d;}cumulative+=sum;difference[tau]=sum*tau/std::max(cumulative,1e-20);}
   int lag=0;
   for(int tau=minLag;tau<maxLag;++tau)if(difference[tau]<.16){while(tau+1<maxLag&&difference[tau+1]<difference[tau])++tau;lag=tau;break;}
   if(lag>0){double refined=lag;if(lag>1&&lag<maxLag){double a=difference[lag-1],b=difference[lag],c=difference[lag+1];double den=a-2*b+c;if(std::abs(den)>1e-10)refined+=std::clamp(.5*(a-c)/den,-.5,.5);}
    point.hz=float(dsRate/refined);point.confidence=float(1-difference[lag]);}
  }
  result.points.push_back(point);if(progress)progress(float(frame+1)/frames);
 }
 return result;
}
// Keep these indices stable so existing projects retain their voice character.
inline const char* characterName(int index) {
 static const char* names[]{"Gospel Choir","Children's Choir","Shout / Gang Choir",
  "Nasheed / Naat Choir","Soft Emotional Choir","Male Choir","Female Choir","Mixed Choir"};
 return names[std::clamp(index,0,7)];
}
struct Character { float formant, tilt, drive, level; };
inline Character characterProfile(int n) {
 static const Character profiles[]{{0,1.5f,1.15f,.85f},{3.5f,3.f,1.f,.82f},{-.6f,2.f,1.8f,.72f},
 {-.3f,-1.f,1.f,.9f},{.3f,-2.f,1.f,.9f},{-1.8f,-.8f,1.f,.88f},{1.8f,1.f,1.f,.86f},{0,0,1.f,.88f}};
 return profiles[std::clamp(n,0,7)];
}
inline std::vector<float> renderSinger(const float* source,int frames,double rate,const Melody& melody,
 Part part,int singer,const ChoirConfig& config,const std::atomic<bool>& cancel,std::function<void(float)> progress={}) {
 signalsmith::stretch::SignalsmithStretch<float> stretch(1931+singer);
 stretch.presetDefault(1,float(rate));
 const int latency=stretch.inputLatency()+stretch.outputLatency(),block=256;
 const auto profile=characterProfile(config.character);
 std::vector<float> rendered(frames),in(block),out(block);
 int first=0,last=frames-1;while(first<frames&&std::abs(source[first])<1e-9f)++first;while(last>=first&&std::abs(source[last])<1e-9f)--last;
 const float* inputs[]{in.data()};float* outputs[]{out.data()};
 const double detune=(singer%2?1.:-1.)*(.3+.7*(singer%8)/7.)*config.variation;
 const double micTilt=std::array<float,5>{0,-2,1.5f,-1,3}[std::clamp(config.microphone,0,4)];
 const double highGain=std::pow(10.,(profile.tilt+micTilt)/20.);
 const double pole=std::exp(-2*pi*1800/rate);double low=0;
 for(int offset=0;offset<frames+latency;offset+=block){
  if(cancel)throw std::runtime_error("Cancelled.");
  const int n=std::min(block,frames+latency-offset);
  for(int i=0;i<n;++i)in[i]=offset+i<frames?source[offset+i]:0;
  const int position=std::clamp(offset-stretch.inputLatency(),0,frames-1);const auto pitch=melody.at(position);
  double semitones=0;
  if(pitch.hz>0&&pitch.confidence>.75f){const double midi=69+12*std::log2(pitch.hz/440.);
   semitones=harmonySemitones(midi,part,config.key,config.scale,config.customMask);
   if(part!=Part::Main)semitones+=detune*(1+.18*std::sin(position/rate*(2.2+.11*singer)+singer))*.01;
  }
  stretch.setTransposeSemitones(float(semitones));
  stretch.setFormantSemitones(profile.formant+(part==Part::Main?0:float((singer%5-2)*.22)),true);
  stretch.setFormantBase(pitch.hz>0?pitch.hz/float(rate):180.f/float(rate));
  stretch.process(inputs,n,outputs,n);
  for(int i=0;i<n;++i){const int frame=offset+i-latency;if(frame<0||frame>=frames)continue;
   const auto localPitch=melody.at(frame);double wet=std::clamp((localPitch.confidence-.55f)*3.f,0.f,1.f);
   // Retain unvoiced consonants. Main retains its original melody without a pitch shift.
   double x=wet*out[i]+(1-wet)*source[frame];
   low=pole*low+(1-pole)*x;x=low+(x-low)*highGain;
   if(profile.drive>1)x=std::tanh(x*profile.drive)/std::tanh(profile.drive);
   const double drift=part==Part::Main?1:(.96+.025*std::sin(frame/rate*(1.6+singer*.09)+singer));
   // Preserve the source's silence boundary. Reverb, if selected, is applied later.
   if(localPitch.rms<1e-5f||frame<first||frame>last)x=0;
   rendered[frame]=float(std::clamp(x*profile.level*drift,-1.,1.));
  }
  if(progress)progress(float(offset+n)/(frames+latency));
 }
 if(part!=Part::Main&&config.variation>0){
  const auto aligned=rendered;
  for(int i=first;i<=last;++i){
   if((i&8191)==0&&cancel)throw std::runtime_error("Cancelled.");
   const double attack=i>0?std::clamp(std::abs(source[i]-source[i-1])*8.,0.,1.):1.;
   const double shift=std::sin(i/rate*(1.1+singer*.07)+singer*.91)*rate*.001*std::min(4.f,config.variation*.25f)*(1-attack);
   const double pos=std::clamp(i+shift,double(first),double(last));const int a=int(pos),b=std::min(a+1,last);rendered[i]=float(aligned[a]+(pos-a)*(aligned[b]-aligned[a]));
  }
 }
 return rendered;
}
}
