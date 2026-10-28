#include "OfflineVoice.h"
#include <iostream>
#include <vector>
using noori_choir::OfflineVoice;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 for(double rate:{8000.,44100.,48000.,96000.,384000.}){
  const int n=int(rate*.23)+17;std::vector<float> input(n,0.f);
  for(int i=int(rate*.04);i<int(rate*.17);++i)input[i]=float(.4*std::sin(i*220.*6.283185307179586/rate));
  std::vector<std::vector<float>> voices(8,std::vector<float>(n));
  for(int v=0;v<8;++v)for(int i=0;i<n;++i){
   const auto value=OfflineVoice::sample(input.data(),n,i,rate,v,20,30);voices[v][i]=value;
   check(std::isfinite(value)&&std::abs(value)<=1.f,"Finite bounded output");
   if(input[i]==0)check(value==0,"No leading/trailing shift or added sound in silence");
  }
  for(int a=0;a<8;++a)for(int b=a+1;b<8;++b){
   double difference=0;for(int i=0;i<n;++i)difference+=std::abs(voices[a][i]-voices[b][i]);
   check(difference>.01,"Each voice differs independently");
  }
  for(int i=0;i<n;i+=37)check(voices[3][i]==OfflineVoice::sample(input.data(),n,i,rate,3,20,30),"Deterministic random access / block independence");
  std::fill(input.begin(),input.end(),0.f);input[n/2]=.8f;
  for(int v=0;v<8;++v)for(int i=0;i<n;++i){const auto x=OfflineVoice::sample(input.data(),n,i,rate,v,8,12);check((i==n/2)?x>.5f:x==0,"Impulse stays at original sample");}
 }
 float one=.4f;for(int v=0;v<8;++v)check(std::isfinite(OfflineVoice::sample(&one,1,0,48000,v,20,30)),"One-frame input");
 std::cout<<"PASS offline eight voices, sample alignment, silence, independent variation, rates, boundaries and determinism\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
