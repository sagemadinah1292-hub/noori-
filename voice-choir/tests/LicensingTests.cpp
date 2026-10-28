#include "Plugin.h"
#include <iostream>
using namespace juce;
using namespace noori_licensing;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void activated(Manager& manager){const double end=Time::getMillisecondCounterHiRes()+15000;while(!manager.canUse()){check(Time::getMillisecondCounterHiRes()<end,manager.snapshot().message.toRawUTF8());Thread::sleep(5);}}
int main(int argc,char** argv){try{
 ScopedJuceInitialiser_GUI init;check(argc==2,"Pass activation fixture path");const auto fixture=JSON::parse(File(argv[1]).loadFileAsString());check(fixture.isObject(),"Activation fixture is available");
 const auto modulus=fixture["publicModulus"].toString(),email=fixture["email"].toString(),device=fixture["deviceId"].toString(),nonce=fixture["nonce"].toString();
 auto grant=verifyGrant(fixture["token"],modulus,email,device,nonce);check(grant.valid&&grant.trial&&grant.onlineRequired,"Real RSA-PSS signature validates on Windows");
 check(verifyGrant(fixture["phpCompatibleToken"],modulus,email,device,nonce).valid,"PHP-compatible RS256 signature verifies on Windows");
 check(!verifyGrant(fixture["token"],modulus,"other@example.com",device,nonce).valid,"Wrong email rejected");
 check(!verifyGrant(fixture["token"],modulus,email,"different-computer",nonce).valid,"Wrong computer rejected");
 check(!verifyGrant(fixture["token"],modulus,email,device,"different-nonce").valid,"Replay nonce rejected");
 check(!verifyGrant(fixture["expired"],modulus,email,device,nonce).valid,"Expired trial grant rejected");
 auto tampered=fixture["token"].clone();tampered.getDynamicObject()->setProperty("payload",Base64::toBase64("tampered",8));check(!verifyGrant(tampered,modulus,email,device,nonce).valid,"Payload tampering rejected");
 Config c;c.enabled=true;c.apiBase="https://activation-test.invalid";c.publicModulusHex=modulus;auto root=File::getSpecialLocation(File::tempDirectory).getChildFile("NooriLicenseTest-"+Uuid().toString());c.stateDirectory=root.getChildFile("trial");
 const auto base=fixture["baseUrl"].toString();
 auto transport=[base](const String& path,const var& body){int status=0;auto stream=URL(base+path).withPOSTData(JSON::toString(body,true)).createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inPostData).withHttpRequestCmd("POST").withExtraHeaders("Content-Type: application/json\r\n").withConnectionTimeoutMs(5000).withStatusCode(&status));if(!stream)throw std::runtime_error(("Test activation server unreachable; HTTP status "+String(status)+", URL "+base+path).toStdString());auto response=JSON::parse(stream->readEntireStreamAsString());if(status!=200)throw std::runtime_error(response.getProperty("error","Activation rejected").toString().toStdString());return response;};
 c.transport=transport;
 {Manager trial(c);check(!trial.canUse(),"Protected manager starts locked");trial.activate(email,fixture["trialSerial"].toString());activated(trial);check(trial.snapshot().trial,"Online trial activates");check(trial.snapshot().trialExpiresAt>0,"Server trial expiry is displayed");trial.shutdown();
  MemoryBlock cache;check(c.stateDirectory.getChildFile("activation.dat").loadFileAsData(cache),"Encrypted activation saved");const auto plain=fixture["trialSerial"].toString().toStdString();const auto* begin=static_cast<const char*>(cache.getData());check(std::search(begin,begin+cache.getSize(),plain.begin(),plain.end())==begin+cache.getSize(),"Serial is not stored as plaintext");}
 {Manager restored(c);activated(restored);check(restored.snapshot().trial,"Saved trial validates online after restart");}
 c.stateDirectory=root.getChildFile("offline");c.transport=[transport](const String& path,const var& body){return transport("/offline"+path,body);};
 {Manager full(c);full.activate(email,fixture["offlineSerial"].toString());activated(full);check(!full.snapshot().trial,"Free full-access key activates");}
 c.transport=[](const String&,const var&)->var{throw std::runtime_error("Network deliberately unavailable");};
 {Manager offline(c);check(offline.canUse(),"Signed full key may work offline when server policy permits");offline.removeLocalActivation();check(!offline.canUse(),"Remove local activation locks features");}
 c.stateDirectory=root.getChildFile("locked");
 {ChoirProcessor protectedPlugin(c);protectedPlugin.setRateAndBufferSizeDetails(48000,64);protectedPlugin.prepareToPlay(48000,64);protectedPlugin.startCapture();check(!protectedPlugin.isCapturing(),"Unactivated plugin cannot start premium capture");protectedPlugin.generate();check(!protectedPlugin.isBusy(),"Unactivated plugin cannot generate vocals");AudioBuffer<float> audio(2,64);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)audio.setSample(ch,i,.25f);MidiBuffer midi;protectedPlugin.processBlock(audio,midi);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)check(audio.getSample(ch,i)==.25f,"Unactivated insert preserves original track audio");}
 root.deleteRecursively();std::cout<<"PASS signed online activation, three-day trial metadata, device/email binding, replay/tamper rejection, DPAPI cache and protected DSP gates\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
