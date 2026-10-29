#include "ChoirEngine.h"
#include "RenderMemory.h"
#include <iostream>
#include <stdexcept>
using namespace noori_choir;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
 const auto realCase=renderWorkingBytes(2982525,2,2,16);check(realCase>512*renderMiB&&realCase<1024*renderMiB,"62.14 second stereo/16-singer case needs more than old cap, under 1 GiB working budget");
 check(renderBudgetForAvailable(2ULL*1024*renderMiB,512*renderMiB)==1536*renderMiB,"Already-resident buffers do not consume the additional-RAM allowance twice");check(realCase<renderBudgetForAvailable(2ULL*1024*renderMiB),"Real reported case fits with 2 GiB available RAM");check(realCase>renderBudgetForAvailable(1024*renderMiB),"Do not accept reported case when RAM is actually tight");check(renderBudgetForAvailable(64ULL*1024*renderMiB)==2ULL*1024*renderMiB,"Hard cap remains bounded on large machines");
 for(double rate:{44100.,48000.,96000.}){
 Engine e;e.prepare(rate);Settings s;s.mix=0;s.gain=0;
 std::vector<float> l(20000),r(20000);l[0]=.75f;r[0]=-.25f;e.process(l.data(),r.data(),int(l.size()),s);for(int i=0;i<int(l.size());++i){check(std::abs(l[i]-(i==e.latencySamples()?.75f:0.f))<1e-7,"Dry left latency");check(std::abs(r[i]-(i==e.latencySamples()?-.25f:0.f))<1e-7,"Dry right latency");}
 s.mix=100;s.gain=-6;s.voices=16;s.detune=20;s.timing=30;s.width=100;e.reset();
 for(int i=0;i<int(l.size());++i)l[i]=r[i]=float(.5*std::sin(6.283185307*220*i/rate));e.process(l.data(),r.data(),int(l.size()),s);double power=0,diff=0;for(int i=4000;i<int(l.size());++i){check(std::isfinite(l[i])&&std::abs(l[i])<.51,"Bounded choir output");check(std::isfinite(r[i])&&std::abs(r[i])<.51,"Bounded right output");power+=l[i]*l[i];diff+=std::abs(l[i]-r[i]);}check(power>.1&&diff>1,"Audible stereo ensemble");
 e.reset();l.assign(20000,0);r=l;e.process(l.data(),nullptr,20000,s);for(float x:l)check(x==0,"No feedback or stale audio after reset");
 Engine a,b;a.prepare(rate);b.prepare(rate);std::vector<float>x(12000),y(12000);for(int i=0;i<12000;++i)x[i]=y[i]=float(std::sin(i*.034)*.2);a.process(x.data(),nullptr,12000,s);for(int i=0;i<12000;i+=37)b.process(y.data()+i,nullptr,std::min(37,12000-i),s);check(x==y,"Block size independent output");
 // An impulse must finish within the bounded grain buffer, with no word-length drift.
 a.reset();x.assign(40000,0);x[0]=1;a.process(x.data(),nullptr,40000,s);for(int i=int(rate*.15);i<40000;++i)check(x[i]==0,"Finite tail / no cumulative stretch");
 }std::cout<<"PASS choir dry latency, stereo ensemble, bounded output, reset, mono, block partition and finite tail\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
