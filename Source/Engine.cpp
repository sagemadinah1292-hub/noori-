#include "Engine.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <random>
#include <limits>
namespace noori {
static constexpr double pi = std::numbers::pi;
void Audio::validate() const {
 if ((channels.size()!=1 && channels.size()!=2) || !std::isfinite(sampleRate) || sampleRate<8000 || sampleRate>192000 || length()<=0)
  throw std::invalid_argument("Expected finite mono/stereo audio, 8-192 kHz");
 for (const auto& c:channels) {
  if ((Sample)c.size()!=length()) throw std::invalid_argument("Channel lengths differ");
  for(float s:c) if(!std::isfinite(s)) throw std::invalid_argument("Non-finite audio");
 }
}
Timeline::Timeline(double r, std::vector<TempoPoint> p):rate(r),map(std::move(p)) {
 if(!std::isfinite(rate)||rate<=0||map.empty()) throw std::invalid_argument("Invalid tempo map");
 for(std::size_t i=0;i<map.size();++i) {
  const auto& t=map[i];
  if(!std::isfinite(t.bpm)||t.bpm<1||t.bpm>1000||!std::isfinite(t.quarter)) throw std::invalid_argument("Invalid tempo");
  if(i && (t.sample<=map[i-1].sample || t.quarter<=map[i-1].quarter)) throw std::invalid_argument("Unordered tempo map");
  if(i) {
   const auto& prev=map[i-1];
   const double expected=prev.quarter+double(t.sample-prev.sample)*prev.bpm/(60*rate);
   if(std::abs(expected-t.quarter)>0.0001) throw std::invalid_argument("Discontinuous tempo map");
  }
 }
}
double Timeline::quarterAt(Sample s) const {
 auto it=std::upper_bound(map.begin(),map.end(),s,[](Sample a,const TempoPoint& b){return a<b.sample;});
 if(it!=map.begin()) --it;
 return it->quarter+double(s-it->sample)*it->bpm/(60*rate);
}
Sample Timeline::sampleAt(double q) const {
 if(!std::isfinite(q)) throw std::invalid_argument("Invalid beat");
 auto it=std::upper_bound(map.begin(),map.end(),q,[](double a,const TempoPoint& b){return a<b.quarter;});
 if(it!=map.begin()) --it;
 return it->sample+(Sample)std::llround((q-it->quarter)*60*rate/it->bpm);
}
static void fft(std::vector<std::complex<float>>& a) {
 const auto n=a.size();
 for(std::size_t i=1,j=0;i<n;++i) { auto bit=n>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit; if(i<j) std::swap(a[i],a[j]); }
 for(std::size_t len=2;len<=n;len*=2) {
  const std::complex<float> step=std::polar(1.f,float(-2*pi/len));
  for(std::size_t i=0;i<n;i+=len) { std::complex<float> w=1; for(std::size_t j=0;j<len/2;++j) {auto u=a[i+j],v=a[i+j+len/2]*w; a[i+j]=u+v; a[i+j+len/2]=u-v; w*=step;} }
 }
}
static bool progress(const Progress& p,float v) {if(p && !p(v)) throw std::runtime_error("Cancelled");return true;}
Attacks detect(const Audio& audio,const Settings& settings,Progress cb) {
 audio.validate();
 const int n=settings.percussion?256:1024, hop=std::max(1,(int)std::llround(audio.sampleRate*(settings.percussion?.001:.005)));
 const auto count=audio.length();
 // Highest-energy channel avoids cancellation in polarity-inverted stereo recordings.
 std::size_t active=0; double best=0;
 for(std::size_t c=0;c<audio.channels.size();++c) {double e=0;for(auto x:audio.channels[c])e+=double(x)*x;if(e>best){best=e;active=c;}}
 const auto& mono=audio.channels[active];
 struct Frame {double energy=0, flux=0, voice=0, noise=0, score=0; Sample at=0;};
 std::vector<Frame> frames;
 std::vector<float> previous(n/2+1,0);
 std::vector<std::complex<float>> bins(n);
 for(Sample start=0;start<count;start+=hop) {
  Frame f; f.at=start;
  int zc=0;
  for(int i=0;i<n;++i) {float x=start+i<count?mono[(std::size_t)(start+i)]:0; f.energy+=double(x)*x; bins[i]=x*float(.5-.5*std::cos(2*pi*i/(n-1)));if(i&&start+i<count)zc+=(x*mono[(std::size_t)(start+i-1)]<0);}
  f.energy=std::sqrt(f.energy/n); f.noise=double(zc)/n;
  fft(bins);
  double magSum=0;
  for(int k=1;k<=n/2;++k) {auto m=std::abs(bins[k]); if(k*audio.sampleRate/n>80 && k*audio.sampleRate/n<8000) { f.flux+=std::max(0.f,m-previous[k]);magSum+=m; } previous[k]=m;}
  f.flux/=std::max(1.0,magSum);
  // Normalised autocorrelation estimates periodicity, not language or phonemes.
  const int lo=(int)(audio.sampleRate/500),hi=std::min(n/2,(int)(audio.sampleRate/70));
  for(int lag=lo;lag<=hi;lag+=3) {double xy=0,xx=0,yy=0;for(int i=0;i<n-lag;i+=2) {float x=start+i<count?mono[(std::size_t)(start+i)]:0,y=start+i+lag<count?mono[(std::size_t)(start+i+lag)]:0;xy+=double(x)*y;xx+=double(x)*x;yy+=double(y)*y;} f.voice=std::max(f.voice,xy/std::sqrt(std::max(1e-20,xx*yy)));}
  frames.push_back(f);
  if(frames.size()%64==0) progress(cb,float(start)/float(count)*.9f);
 }
 double peak=0;for(auto& f:frames)peak=std::max(peak,f.energy);
 if(peak<1e-5) return {};
 Attacks result;
 const double threshold=.09+.35*(1-std::clamp(settings.sensitivity,0.,1.));
 for(std::size_t i=1;i+1<frames.size();++i) {
  auto& f=frames[i]; const auto prev=frames[i-1].energy;
  double rise=std::max(0.,(f.energy-prev)/std::max(peak*.03,prev));
  f.score=.65*f.flux+.35*std::min(1.,rise);
  if(!settings.percussion && settings.breathProtection && f.voice<.35 && f.noise>.22) f.score*=.3;
  if(!settings.percussion && settings.vowelProtection && rise<.05 && f.voice>.65) f.score*=.45;
 }
 for(std::size_t i=1;i+1<frames.size();++i) {
  const auto& f=frames[i];
  if(f.score<threshold || f.energy<peak*.035 || f.score<frames[i-1].score || f.score<frames[i+1].score)continue;
  Sample onset=f.at;
  // Locate a sustained energy onset using a short RMS envelope. A single
  // zero crossing is not an onset; refine the threshold crossing sample-by-sample.

  const Sample lower=std::max<Sample>(0,onset-n),upper=std::min<Sample>(count-1,f.at+n);
  const Sample window=std::max<Sample>(8,(Sample)std::llround(audio.sampleRate*.001));
  double sum=0;for(Sample j=lower;j<std::min(count,lower+window);++j)sum+=double(mono[(std::size_t)j])*mono[(std::size_t)j];
  const double gate=std::max(peak*.018,f.energy*.12);bool found=false;
  for(Sample at=lower;at<=upper;++at){if(std::sqrt(std::max(0.,sum)/window)>=gate){onset=at;found=true;break;}sum-=double(mono[(std::size_t)at])*mono[(std::size_t)at];if(at+window<count)sum+=double(mono[(std::size_t)(at+window)])*mono[(std::size_t)(at+window)];}
  if(found){const double amplitudeGate=std::max(peak*.008,f.energy*.025);auto stop=std::min(count,onset+window);while(onset<stop&&std::abs(mono[(std::size_t)onset])<amplitudeGate)++onset;}
  onset=std::clamp<Sample>(onset,1,count-1);
  const Sample minGap=(Sample)(audio.sampleRate*(settings.percussion?.025:settings.naturalNaat?.11:.065));
  if(!result.empty() && onset-result.back().source<minGap) {if(f.score>result.back().confidence) result.back()={onset,onset,(float)f.score,(float)f.voice,false};continue;}
  result.push_back({onset,onset,(float)std::min(1.,f.score),(float)f.voice,false});
 }
 progress(cb,1);return result;
}
static void validateMarkers(const Attacks& a,Sample length,double maxStretch) {
 if(length<2||!std::isfinite(maxStretch)||maxStretch<1||maxStretch>2)throw std::invalid_argument("Invalid stretch limit");
 Sample lastS=0,lastT=0;
 for(std::size_t i=0;i<=a.size();++i) {
  auto s=i<a.size()?a[i].source:length,t=i<a.size()?a[i].target:length;
  if(s<=lastS||s>length||t<=lastT||t>length)throw std::invalid_argument("Crossed/out-of-range markers");
  double ratio=double(t-lastT)/double(s-lastS);
  if(ratio<1/maxStretch-1e-7||ratio>maxStretch+1e-7)throw std::invalid_argument("Stretch safety limit exceeded");
  lastS=s;lastT=t;
 }
}
std::vector<Sample> detectClicks(const Audio& audio) {
 audio.validate();float peak=0;
 for(const auto& c:audio.channels)for(float x:c)peak=std::max(peak,std::abs(x));
 std::vector<Sample> clicks;if(peak<1e-6f)return clicks;
 // Threshold crossing, not the peak of the ringing click: retain its onset sample.
 const float threshold=peak*.04f;const Sample quietNeeded=std::max<Sample>(1,(Sample)(audio.sampleRate*.025));
 Sample quiet=quietNeeded;
 for(Sample i=0;i<audio.length();++i){float level=0;for(const auto& c:audio.channels)level=std::max(level,std::abs(c[(std::size_t)i]));
  if(level>=threshold){if(quiet>=quietNeeded)clicks.push_back(i);quiet=0;}else ++quiet;
 }
 return clicks;
}
static Attacks constrain(Attacks a,Sample length,double limit) {
 // Sequential feasible projection; final endpoint is fixed and included in each constraint.
 Sample prevS=0,prevT=0;
 for(auto& m:a) {
  const Sample dx=m.source-prevS, remaining=length-m.source;
  if(dx<=0||remaining<=0)throw std::invalid_argument("Unordered source markers");
  auto lo=std::max(prevT+(Sample)std::ceil(dx/limit),length-(Sample)std::floor(remaining*limit));
  auto hi=std::min(prevT+(Sample)std::floor(dx*limit),length-(Sample)std::ceil(remaining/limit));
  if(lo>hi)throw std::invalid_argument("No feasible warp");
  m.target=std::clamp(m.target,lo,hi);prevS=m.source;prevT=m.target;
 }
 validateMarkers(a,length,limit);return a;
}
Attacks quantize(const Attacks& attacks,Sample length,Sample origin,const Timeline& timeline,const Settings& s,bool selectedOnly,const std::vector<Sample>& reference,bool exactBpm) {
 if(s.division!=4&&s.division!=8&&s.division!=16&&s.division!=32)throw std::invalid_argument("Unsupported grid");
 if(!std::isfinite(s.strength)||!std::isfinite(s.swing)||!std::isfinite(s.offsetMs)||!std::isfinite(s.humanizeMs)||!std::isfinite(s.toleranceMs))throw std::invalid_argument("Invalid quantization settings");
 double step=4./s.division;
 if(s.rhythm==Settings::Rhythm::triplet)step*=2./3;
 if(s.rhythm==Settings::Rhythm::dotted)step*=1.5;
 auto out=attacks; std::mt19937 random(0x4e5454);std::uniform_real_distribution<double> human(-1,1);
 for(auto& a:out) {
  if(selectedOnly&&!a.selected)continue;
  const auto project=a.source+origin;const auto q=timeline.quarterAt(project);
  auto k=(Sample)std::floor(q/step);Sample target=project;double distance=1e99;
  for(auto j=k-2;j<=k+2;++j) {
   double beat=j*step+(j%2!=0?std::clamp(s.swing,0.,.75)*step:0.);
   auto candidate=timeline.sampleAt(beat)+(Sample)std::llround(s.offsetMs*.001*timeline.sampleRate());
   if(std::abs(double(candidate-project))<distance){target=candidate;distance=std::abs(double(candidate-project));}
  }
  if(!reference.empty()) {
   auto it=std::lower_bound(reference.begin(),reference.end(),a.source);
   target=it==reference.end()?reference.back():*it;
   if(it!=reference.begin()&&std::abs(double(*(it-1)-a.source))<=std::abs(double(target-a.source)))target=*(it-1);
   target+=origin;distance=std::abs(double(target-project));
  }
  if(exactBpm&&(target-origin<=0||target-origin>=length)){a.target=a.source;continue;} // Fixed clip endpoints cannot be warped onto themselves.
  if(distance<s.toleranceMs*.001*timeline.sampleRate()){a.target=a.source;continue;}
  double strength=std::clamp(s.strength,0.,1.);
  if(s.emotionalProtection&&strength<1.) strength*=.65+.35*std::clamp(double(a.confidence),0.,1.);
  auto shift=(Sample)std::llround((target-project)*strength+human(random)*s.humanizeMs*.001*timeline.sampleRate()*strength);
  if(!s.sampleAccurate)shift=(Sample)std::llround(double(shift)/(timeline.sampleRate()*.001))*(Sample)std::llround(timeline.sampleRate()*.001);
  a.target=a.source+shift;
 }
 if(!reference.empty()||exactBpm) {
  // An exact reference request is a simultaneous set of anchors. Validate it
  // directly; projecting one marker at a time can hide a collision or reject a
  // valid selected group because its neighbours have not moved yet.
  try {validateMarkers(out,length,s.maxStretch);}
  catch(const std::exception&) {throw std::runtime_error(exactBpm?"Exact BPM alignment blocked: attacks collide on one beat or exceed the stretch limit. Choose a finer grid, select fewer attacks, or increase the stretch limit. Previous audio kept.":"Exact click match exceeds stretch limits or maps multiple attacks to one click. Select fewer attacks, use a finer click track, or increase the stretch limit.");}
  return out;
 }
 if(selectedOnly){auto result=attacks;for(std::size_t i=0;i<out.size();++i)if(out[i].selected)result=moveAttack(result,i,out[i].target,length,s.maxStretch);return result;}
 return constrain(out,length,std::clamp(s.maxStretch,1.,2.));
}
Attacks moveAttack(const Attacks& a,std::size_t index,Sample target,Sample length,double limit) {
 validateMarkers(a,length,limit);if(index>=a.size())throw std::out_of_range("Marker index");auto out=a;
 Sample ps=index?a[index-1].source:0,pt=index?a[index-1].target:0;
 Sample ns=index+1<a.size()?a[index+1].source:length,nt=index+1<a.size()?a[index+1].target:length;
 Sample lo=std::max(pt+(Sample)std::ceil((a[index].source-ps)/limit),nt-(Sample)std::floor((ns-a[index].source)*limit));
 Sample hi=std::min(pt+(Sample)std::floor((a[index].source-ps)*limit),nt-(Sample)std::ceil((ns-a[index].source)/limit));
 out[index].target=std::clamp(target,lo,hi);validateMarkers(out,length,limit);return out;
}
Audio render(const Audio& input,const Attacks& a,const Settings& s,Progress cb) {
 input.validate();validateMarkers(a,input.length(),s.maxStretch);
 bool identity=true;for(const auto& m:a)identity&=m.source==m.target;if(identity)return input;
 const int channels=(int)input.channels.size();const Sample length=input.length();
 std::vector<std::pair<Sample,Sample>> anchors{{0,0}};
 for(const auto& m:a)anchors.emplace_back(m.target,m.source);
 anchors.emplace_back(length,length);
 std::size_t segment=1;
 auto sourceAt=[&](Sample target)->double {
  if(target<0)return double(target);
  if(target>length)return double(target);
  while(segment+1<anchors.size()&&target>anchors[segment].first)++segment;
  auto [y0,x0]=anchors[segment-1];auto [y1,x1]=anchors[segment];
  return x0+double(target-y0)*double(x1-x0)/double(y1-y0);
 };
 signalsmith::stretch::SignalsmithStretch<float> stretch(42);
 stretch.presetDefault(channels,(float)input.sampleRate);
 stretch.setTransposeFactor(1); // Unity pitch; no resampling pitch shift.
 const int inLatency=stretch.inputLatency(),outLatency=stretch.outputLatency();
 std::vector<std::vector<float>> prime(channels,std::vector<float>(inLatency));
 for(int c=0;c<channels;++c)for(int i=0;i<inLatency;++i)if(i<length)prime[c][i]=input.channels[c][i];
 stretch.seek(prime,inLatency,1.);
 Audio output{input.sampleRate,std::vector<std::vector<float>>(channels,std::vector<float>((std::size_t)length))};
 Sample cursor=inLatency;
 for(Sample out=0;out<length+outLatency;) {
  const int n=(int)std::min<Sample>(128,length+outLatency-out);
  const auto desired=(Sample)std::llround(sourceAt(out+n))+inLatency;
  const int consumed=(int)std::max<Sample>(0,desired-cursor);
  std::vector<std::vector<float>> in(channels,std::vector<float>((std::size_t)std::max(1,consumed),0)),block(channels,std::vector<float>(n));
  for(int c=0;c<channels;++c)for(int i=0;i<consumed;++i)if(cursor+i<length)in[c][i]=input.channels[c][(std::size_t)(cursor+i)];
  stretch.process(in,consumed,block,n);
  for(int c=0;c<channels;++c)for(int i=0;i<n;++i){auto at=out+i-outLatency;if(at>=0&&at<length)output.channels[c][(std::size_t)at]=block[c][i];}
  cursor=desired;out+=n;if(out%4096<128)progress(cb,float(out)/float(length+outLatency));
 }
 // Preserve consonant transients using a unity-rate island with raised-cosine seams.
 if(s.consonantProtection)for(const auto& m:a) {
  const Sample radius=std::min<Sample>((Sample)(input.sampleRate*.008),std::min({m.source,length-m.source,m.target,length-m.target}));
  for(Sample j=-radius;j<radius;++j) {double weight=.5+.5*std::cos(pi*double(j)/std::max<Sample>(1,radius));for(int c=0;c<channels;++c){auto& dst=output.channels[c][(std::size_t)(m.target+j)];dst=float(dst*(1-weight)+input.channels[c][(std::size_t)(m.source+j)]*weight);}}
 }
 progress(cb,1);return output;
}
std::string serializeAttacks(const Attacks& a) {
 std::ostringstream o;o<<"NTT1 "<<a.size()<<'\n'<<std::setprecision(9);
 for(auto m:a)o<<m.source<<' '<<m.target<<' '<<m.confidence<<' '<<m.voicing<<' '<<m.selected<<'\n';
 return o.str();
}
Attacks deserializeAttacks(const std::string& text,Sample length) {
 std::istringstream in(text);std::string magic;std::size_t n=0;
 if(!(in>>magic>>n)||magic!="NTT1"||n>100000)throw std::invalid_argument("Invalid marker archive");
 Attacks a(n);Sample ps=0,pt=0;
 for(auto& m:a) {if(!(in>>m.source>>m.target>>m.confidence>>m.voicing>>m.selected)||m.source<=ps||m.target<=pt||m.source>=length||m.target>=length||!std::isfinite(m.confidence)||!std::isfinite(m.voicing))throw std::invalid_argument("Invalid archived marker");ps=m.source;pt=m.target;}
 return a;
}
}
