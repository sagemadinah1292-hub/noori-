#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace noori_choir {
struct Settings { float voices=8, detune=8, timing=12, width=85, mix=65, gain=-3; };
// Dual overlapping variable-delay grains. No feedback and no cumulative time warp.
class Engine {
 double sr=48000; int write=0, latency=1920; std::array<std::vector<float>,2> ring;
 std::array<double,16> phase{}; std::array<float,16> levels{};
 Settings smooth{}; bool first=true;
 float read(int channel,double delay)const {double p=write-delay;while(p<0)p+=ring[0].size();int a=int(p)%int(ring[0].size());int b=(a+1)%int(ring[0].size());float f=float(p-std::floor(p));return ring[channel][a]+f*(ring[channel][b]-ring[channel][a]);}
public:
 void prepare(double rate){sr=rate;latency=int(std::round(.040*sr));for(auto& r:ring)r.assign(std::size_t(std::ceil(sr*.25))+8,0);reset();}
 void reset(){for(auto& r:ring)std::fill(r.begin(),r.end(),0);write=0;for(int v=0;v<16;++v)phase[v]=std::fmod(v*.61803398875,1.);levels.fill(0);first=true;}
 int latencySamples()const{return latency;}
 void process(float* left,float* right,int n,const Settings& input){
  Settings t=input;t.voices=std::clamp(t.voices,2.f,16.f);t.detune=std::clamp(t.detune,0.f,20.f);t.timing=std::clamp(t.timing,0.f,30.f);t.width=std::clamp(t.width,0.f,100.f);t.mix=std::clamp(t.mix,0.f,100.f);t.gain=std::clamp(t.gain,-24.f,6.f);
  if(first){smooth=t;for(int v=0;v<16;++v)levels[v]=v<int(t.voices)?1.f:0.f;first=false;}
  const float a=float(1-std::exp(-1./(.020*sr)));const double window=.040*sr;
  for(int i=0;i<n;++i){
   smooth.detune+=a*(t.detune-smooth.detune);smooth.timing+=a*(t.timing-smooth.timing);smooth.width+=a*(t.width-smooth.width);smooth.mix+=a*(t.mix-smooth.mix);smooth.gain+=a*(t.gain-smooth.gain);
   ring[0][write]=left[i];ring[1][write]=right?right[i]:left[i];double wl=0,wr=0,total=0;
   for(int v=0;v<16;++v){levels[v]+=a*((v<int(t.voices)?1.f:0.f)-levels[v]);if(levels[v]<1e-7f)continue;
    // Paired detunes keep the ensemble centred on the original pitch.
    double signedSpread=(v%2?1.:-1.)*(.25+.75*double(v/2)/7.);
    double ratio=std::pow(2.,signedSpread*smooth.detune/1200.);phase[v]+=(1.-ratio)/window;phase[v]-=std::floor(phase[v]);double p=phase[v],q=p+.5;q-=std::floor(q);
    double w=.5-.5*std::cos(6.283185307179586*p);
    double offset=smooth.timing*.001*sr*std::fmod((v+1)*.38196601125,1.);
    double base=latency-window*.5+offset;
    auto mono=[&](double delay){return .5*(read(0,delay)+read(1,delay));};
    double x=w*mono(base+p*window)+(1-w)*mono(base+q*window);
    double pan=(v%2?1.:-1.)*(.35+.65*double(v/2)/7.)*smooth.width*.01;
    wl+=x*(1-pan)*levels[v];wr+=x*(1+pan)*levels[v];total+=levels[v];
   }
   double mix=smooth.mix*.01,g=std::pow(10.,smooth.gain/20.);total=std::max(1.,total);
   float dl=read(0,latency),dr=read(1,latency);
   left[i]=float(g*((1-mix)*dl+mix*(right?wl:(wl+wr)*.5)/total));if(right)right[i]=float(g*((1-mix)*dr+mix*wr/total));
   write=(write+1)%int(ring[0].size());
  }
 }
};
}
