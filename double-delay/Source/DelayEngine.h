#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
namespace noori_delay {
constexpr double maxSeconds=10.0;
struct Tap {bool sync=false;double milliseconds=500;int note=2;int rhythm=0;int multiplier=1;double pan=-1;};
struct Settings {std::array<Tap,2> taps{Tap{},Tap{false,500,2,0,1,1}};double feedback=.5,mix=.5,bpm=120;};
inline double seconds(const Tap& t,double bpm) noexcept {
 if(!t.sync)return std::clamp(std::isfinite(t.milliseconds)?t.milliseconds:500.,0.,9999.)*.001;
 auto beat=4./double(1<<std::clamp(t.note,0,5));
 if(t.rhythm==1)beat*=2./3.;else if(t.rhythm==2)beat*=1.5;
 return beat*std::clamp(t.multiplier,1,10)*60./std::clamp(std::isfinite(bpm)?bpm:120.,1.,1000.);
}
class Engine {
 struct Line {std::vector<float> data;std::size_t write=0;double current=1,from=1,target=1;int fade=0;};
 std::array<Line,2> lines;double rate=48000,feedback=.5,mix=.5;std::array<double,2> pan{-1,1};bool initialized=false;int fadeSamples=960;double slew=1./960.;
 static float read(const Line& l,double delay) noexcept {
  auto pos=double(l.write)-delay;while(pos<0)pos+=double(l.data.size());
  const auto a=std::size_t(pos)%l.data.size(),b=(a+1)%l.data.size();const auto f=float(pos-std::floor(pos));
  return l.data[a]+f*(l.data[b]-l.data[a]);
 }
public:
 void prepare(double sr) {rate=std::clamp(sr,8000.,192000.);fadeSamples=std::max(1,int(rate*.02));slew=1./double(fadeSamples);for(auto& l:lines){l.data.assign(std::size_t(std::ceil(rate*maxSeconds))+4,0);l.write=0;l.fade=0;}initialized=false;}
 void reset() noexcept {for(auto& l:lines){std::fill(l.data.begin(),l.data.end(),0);l.write=0;l.fade=0;}initialized=false;}
 void configure(const Settings& s) noexcept {
  for(std::size_t i=0;i<2;++i){auto& l=lines[i];const double d=std::clamp(seconds(s.taps[i],s.bpm)*rate,1.,rate*maxSeconds);
   if(!initialized){l.current=l.from=l.target=d;pan[i]=std::clamp(s.taps[i].pan,-1.,1.);}else if(std::abs(d-l.target)>.0001){
    // Finish the current crossfade before taking another time target. Retargeting
    // each block during tempo ramps would otherwise freeze a pending transition.
    if(l.fade==0){l.from=l.current;l.target=d;l.fade=fadeSamples;}
   }
  }
  if(!initialized){feedback=std::clamp(s.feedback,0.,1.);mix=std::clamp(s.mix,0.,1.);initialized=true;}
 }
 void process(float* left,float* right,int count,const Settings& s) noexcept {
  if(lines[0].data.empty())return;configure(s);
  const auto fb=std::clamp(std::isfinite(s.feedback)?s.feedback:0.,0.,1.);const auto wet=std::clamp(std::isfinite(s.mix)?s.mix:0.,0.,1.);
  std::array<double,2> desiredPan{std::clamp(s.taps[0].pan,-1.,1.),std::clamp(s.taps[1].pan,-1.,1.)};
  for(int n=0;n<count;++n){auto inL=std::isfinite(left[n])?left[n]:0.f;auto inR=right?(std::isfinite(right[n])?right[n]:0.f):inL;const float mono=right?.5f*(inL+inR):inL;double outL=0,outR=0;
   feedback+=std::clamp(fb-feedback,-slew,slew);mix+=std::clamp(wet-mix,-slew,slew);
   for(std::size_t i=0;i<2;++i){auto& l=lines[i];pan[i]+=std::clamp(desiredPan[i]-pan[i],-2*slew,2*slew);float delayed=read(l,l.current);
    if(l.fade){double x=double(fadeSamples-l.fade+1)/fadeSamples;delayed=float(read(l,l.from)*(1-x)+read(l,l.target)*x);if(--l.fade==0)l.current=l.target;}
    double stored=double(mono)+feedback*delayed;
    // Emergency guard only at extreme overload; ordinary audio is uncoloured.
    stored=std::clamp(stored,-64.,64.);if(std::abs(stored)<1e-25)stored=0;l.data[l.write]=float(stored);l.write=(l.write+1)%l.data.size();
    outL+=delayed*(1-pan[i])*.5;outR+=delayed*(1+pan[i])*.5;
   }
   left[n]=float(inL*(1-mix)+(right?outL:(outL+outR)*.5)*mix);if(right)right[n]=float(inR*(1-mix)+outR*mix);
  }
 }
 std::size_t allocatedBytes()const noexcept{return (lines[0].data.size()+lines[1].data.size())*sizeof(float);}
};
}
