#include "Engine.h"
#include <iostream>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>
#include <fstream>
using namespace noori;
static int checks=0;
static void require(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
static Audio tone(double sr,double hz,double seconds,int channels=1) {
 Audio a{sr,std::vector<std::vector<float>>(channels,std::vector<float>((std::size_t)(sr*seconds)))};
 for(auto& c:a.channels)for(std::size_t i=0;i<c.size();++i)c[i]=.3f*float(std::sin(2*std::numbers::pi*hz*i/sr));return a;
}
static double frequency(const std::vector<float>& c,int begin,int end,double sr){int crosses=0;for(int i=begin+1;i<end;++i)crosses+=c[i-1]<=0&&c[i]>0;return crosses*sr/(end-begin);}
int main(int argc,char** argv) {try {
 for(double sr:{44100.,48000.}) {
  Timeline t(sr,{{0,80,0},{(Sample)(sr*3),120,4}});
  require(t.sampleAt(4)==(Sample)(sr*3),"Tempo boundary");
  require(std::abs(t.quarterAt((Sample)(sr*4))-6)<1e-9,"Tempo change");
  for(Sample i=-100;i<sr*5;i+=137)require(std::abs(t.sampleAt(t.quarterAt(i))-i)<=1,"Sample/beat round trip");
  Settings s;s.emotionalProtection=false;s.toleranceMs=0;
  auto a=tone(sr,220,4,2);auto onset=(Sample)(.81*sr);Attacks markers{{onset,onset,1,1,true}};
  auto q=quantize(markers,a.length(),0,Timeline(sr),s);
  require(std::abs(q[0].target-(Sample)std::llround(.75*sr))<=1,"80 BPM eighth-note grid");
  s.strength=0;require(quantize(markers,a.length(),0,Timeline(sr),s)[0].target==onset,"Zero strength");s.strength=1;
  for(auto r:{Settings::Rhythm::straight,Settings::Rhythm::triplet,Settings::Rhythm::dotted}){s.rhythm=r;auto x=quantize(markers,a.length(),0,Timeline(sr),s);require(x[0].target>0,"Rhythm variants");}s.rhythm=Settings::Rhythm::straight;
  Attacks selected{{(Sample)(sr*.5),(Sample)(sr*.5),1,1,false},{(Sample)(sr*.9),(Sample)(sr*.9),1,1,true},{(Sample)(sr*1.4),(Sample)(sr*1.4),1,1,false}};
  auto selectedResult=quantize(selected,a.length(),0,Timeline(sr),s,true);
  require(selectedResult[0].target==selected[0].target&&selectedResult[2].target==selected[2].target,"Selected-only preserves other markers");
  s.humanizeMs=10;s.strength=0;require(quantize(markers,a.length(),0,Timeline(sr),s)[0].target==onset,"Zero strength also disables humanize");s.humanizeMs=0;s.strength=1;
  auto exact=render(a,markers,s);require(exact.channels==a.channels,"Identity must be bit-exact");
  auto corrected=render(a,q,s);require(corrected.length()==a.length(),"Duration preserved");
  auto freq=frequency(corrected.channels[0],(int)sr,(int)(sr*2.5),sr);
  require(std::abs(freq-220)<2,"Pitch preservation within 2 Hz");
  double stereoError=0, stereoEnergy=0; for(std::size_t i=0;i<corrected.channels[0].size();++i){stereoError+=std::pow(corrected.channels[0][i]-corrected.channels[1][i],2);stereoEnergy+=std::pow(corrected.channels[0][i],2);} std::cout<<"stereo error ratio="<<stereoError/stereoEnergy<<"\n"; require(stereoError/stereoEnergy<.001,"Stereo coherence below -30 dB difference");
  double mse=0;for(std::size_t i=0;i<a.channels[0].size();++i){float x=corrected.channels[0][i];require(std::isfinite(x),"Finite render");mse+=std::pow(x-a.channels[0][i],2);}require(mse>1,"Actual timing modification");
  require(deserializeAttacks(serializeAttacks(q),a.length())[0].target==q[0].target,"Archive round trip");
  auto moved=moveAttack(q,0,a.length()-2,a.length(),s.maxStretch);require(moved[0].target<a.length()-2,"Manual safety clamp");
  Audio silence{sr,{{}}};silence.channels[0].resize((std::size_t)sr);require(detect(silence,s).empty(),"Silence false positives");
  auto synthetic=silence;synthetic.channels[0].resize((std::size_t)(sr*3));
  for(double start:{.4,1.2,2.0})for(int i=0;i<(int)(sr*.35);++i){auto at=(int)(start*sr)+i;float env=float(std::min(1.,i/(sr*.008))*std::min(1.,(sr*.35-i)/(sr*.04)));synthetic.channels[0][at]=.5f*env*float(std::sin(2*std::numbers::pi*180*i/sr));}
  auto detected=detect(synthetic,s);std::cout<<"rate="<<sr<<" attacks="<<detected.size()<<" pitch="<<freq<<" Hz\n";
  for(double expected:{.4,1.2,2.0}){bool found=false;for(auto m:detected)found|=std::abs(m.source/sr-expected)<.055;require(found,"Synthetic syllable onset within 55 ms");}
  auto aligned=quantize(detected,synthetic.length(),0,Timeline(sr),s);auto vocal=render(synthetic,aligned,s);
  for(const auto& m:aligned){auto begin=std::max<Sample>(0,m.target-(Sample)(sr*.04));auto end=std::min<Sample>(vocal.length(),m.target+(Sample)(sr*.07));double energy=0;for(auto i=begin;i<end;++i)energy+=double(vocal.channels[0][(std::size_t)i])*vocal.channels[0][(std::size_t)i];require(energy>1,"Energy at corrected attack");}
  // Optional raw fixture for independent listening/plots, never a platform plugin.
  if(argc>1 && sr==48000){std::ofstream out(argv[1],std::ios::binary);out.write((char*)vocal.channels[0].data(),(std::streamsize)(vocal.length()*sizeof(float)));}
 }
 bool rejected=false;try{Timeline invalid(48000,{{0,80,0},{48000,120,7}});}catch(...){rejected=true;}require(rejected,"Reject discontinuous tempo map");
 std::cout<<"PASS "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
