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
 bool autoKey=true;
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
struct KeyEstimate {int key=0,scale=0;float confidence=0;int pitchClasses=0;};
inline KeyEstimate estimateKey(const Melody& melody) {
 std::array<double,12> chroma{};double total=0;int voiced=0;
 for(auto p:melody.points)if(p.hz>0&&p.confidence>.8f&&p.rms>1e-4f){
  const double midi=69+12*std::log2(p.hz/440.);const int note=int(std::lround(midi));
  const double weight=p.confidence*std::sqrt(p.rms)*std::exp(-std::pow((midi-note)/.35,2));chroma[(note%12+12)%12]+=weight;total+=weight;++voiced;
 }
 KeyEstimate result;if(total<=0||voiced<12)return result;
 for(double v:chroma)if(v>total*.025)++result.pitchClasses;
 // A single note or a short interval cannot identify a key; do not invent one.
 if(result.pitchClasses<4)return result;
 static constexpr double profiles[2][12]={{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17}};
 double best=-2,second=-2;
 for(int mode=0;mode<2;++mode)for(int key=0;key<12;++key){double dot=0,a2=0,b2=0,mean=0;for(double v:profiles[mode])mean+=v/12;
  for(int n=0;n<12;++n){const double a=chroma[(key+n)%12]/total-1./12,b=profiles[mode][n]-mean;dot+=a*b;a2+=a*a;b2+=b*b;}
  const double score=dot/std::sqrt(std::max(1e-20,a2*b2));if(score>best){second=best;best=score;result.key=key;result.scale=mode;}else second=std::max(second,score);
 }
 result.confidence=float(std::clamp((best-second)*4.,0.,1.));return result;
}
inline Melody stableMelody(const Melody& input) {
 Melody out=input;
 for(int i=1;i+1<int(out.points.size());++i){const auto a=input.points[i-1],b=input.points[i],c=input.points[i+1];
  if(a.hz>0&&b.hz>0&&c.hz>0&&a.confidence>.8f&&b.confidence>.75f&&c.confidence>.8f){std::array<float,3> hz{a.hz,b.hz,c.hz};std::sort(hz.begin(),hz.end());out.points[i].hz=hz[1];}
 }
 return out;
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
 if(part==Part::Main){std::vector<float> original(source,source+frames);if(cancel)throw std::runtime_error("Cancelled.");return original;}
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
 const double pole=std::exp(-2*pi*1800/rate);double low=0,blend=0,shift=0,lastHarmony=0;
 const double blendStep=1-std::exp(-1./(rate*.025)),transposeStep=1-std::exp(-block/(rate*.025));
 std::vector<float> fundamentals;for(auto p:melody.points)if(p.hz>0&&p.confidence>.8f)fundamentals.push_back(p.hz);
 float base=180;if(!fundamentals.empty()){std::sort(fundamentals.begin(),fundamentals.end());base=fundamentals[fundamentals.size()/2];}
 stretch.setFormantSemitones(profile.formant+float((singer%5-2)*.12),true);
 stretch.setFormantBase(base/float(rate));
 int pendingFrames=0;double candidate=0;bool haveHarmony=false;
 for(int offset=0;offset<frames+latency;offset+=block){
  if(cancel)throw std::runtime_error("Cancelled.");
  const int n=std::min(block,frames+latency-offset);
  for(int i=0;i<n;++i)in[i]=offset+i<frames?source[offset+i]:0;
  const int position=std::clamp(offset-stretch.inputLatency(),0,frames-1);const auto pitch=melody.at(position);
  if(pitch.hz>0&&pitch.confidence>.8f){const double midi=69+12*std::log2(pitch.hz/440.);
   const double next=harmonySemitones(midi,part,config.key,config.scale,config.customMask);
   if(!haveHarmony){lastHarmony=next;shift=next;haveHarmony=true;}
   if(next!=candidate){candidate=next;pendingFrames=0;}else pendingFrames+=n;
   if(pendingFrames>=int(rate*.025))lastHarmony=candidate;
  }
  // Hold the last harmony through consonants instead of jumping to unison.
  const double target=lastHarmony+detune*.01;shift+=transposeStep*(target-shift);
  stretch.setTransposeSemitones(float(shift));
  stretch.process(inputs,n,outputs,n);
  for(int i=0;i<n;++i){const int frame=offset+i-latency;if(frame<0||frame>=frames)continue;
   const auto localPitch=melody.at(frame);const double targetBlend=localPitch.hz>0?std::clamp((localPitch.confidence-.55f)*3.f,0.f,1.f):0;
   blend+=blendStep*(targetBlend-blend);const double wet=blend;
   // Retain unvoiced consonants. Main retains its original melody without a pitch shift.
   double x=wet*out[i]+(1-wet)*source[frame];
   low=pole*low+(1-pole)*x;x=low+(x-low)*highGain;
   if(profile.drive>1)x=std::tanh(x*profile.drive)/std::tanh(profile.drive);
   const double drift=part==Part::Main?1:(.96+.025*std::sin(frame/rate*(1.6+singer*.09)+singer));
   // Preserve the source's silence boundary. Reverb, if selected, is applied later.
   if(frame<first||frame>last)x=0;
   rendered[frame]=float(x*profile.level*drift);if(!std::isfinite(rendered[frame]))throw std::runtime_error("Non-finite singer audio.");
  }
  if(progress)progress(float(offset+n)/(frames+latency));
 }
 float peak=0;for(float value:rendered)peak=std::max(peak,std::abs(value));if(peak>.98f){const float gain=.98f/peak;for(auto& value:rendered)value*=gain;}
 // No sample-by-sample time warp: consonant-dependent offsets caused audible
 // modulation. Independent small detunes supply variation without chopping time.
 return rendered;
}
}
