#include "Licensing.h"
#include "NooriLicenseConfig.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#endif
using namespace juce;
namespace noori_licensing {
namespace {
String normaliseEmail(String text){return text.trim().toLowerCase();}
String digest(const String& text){return SHA256(text.toRawUTF8(),text.getNumBytesAsUTF8()).toHexString();}
bool decode(const String& text,MemoryBlock& bytes){if(text.length()>32768)return false;MemoryOutputStream out;const bool good=Base64::convertFromBase64(out,text);bytes=out.getMemoryBlock();return good;}
var object(std::initializer_list<std::pair<const char*,var>> values){auto* obj=new DynamicObject();for(const auto& p:values)obj->setProperty(p.first,p.second);return var(obj);}
bool verifySignature(const MemoryBlock& payload,const MemoryBlock& signature,const String& modulus,const String& algorithmName){
#if JUCE_WINDOWS
 if(modulus.length()!=512||!modulus.containsOnly("0123456789abcdefABCDEF")||signature.getSize()!=256)return false;
 MemoryBlock n;n.loadFromHexString(modulus);if(n.getSize()!=256)return false;
 BCRYPT_RSAKEY_BLOB header{};header.Magic=BCRYPT_RSAPUBLIC_MAGIC;header.BitLength=2048;header.cbPublicExp=3;header.cbModulus=256;
 MemoryBlock blob(&header,sizeof(header));const unsigned char exponent[]{1,0,1};blob.append(exponent,3);blob.append(n.getData(),n.getSize());
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_KEY_HANDLE key=nullptr;bool ok=false;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_RSA_ALGORITHM,nullptr,0)>=0){
  if(BCryptImportKeyPair(algorithm,nullptr,BCRYPT_RSAPUBLIC_BLOB,&key,static_cast<PUCHAR>(blob.getData()),ULONG(blob.getSize()),0)>=0){
   const auto hash=SHA256(payload.getData(),payload.getSize()).getRawData();
   BCRYPT_PSS_PADDING_INFO pss{BCRYPT_SHA256_ALGORITHM,32};BCRYPT_PKCS1_PADDING_INFO pkcs1{BCRYPT_SHA256_ALGORITHM};
   void* padding=algorithmName=="RS256"?static_cast<void*>(&pkcs1):static_cast<void*>(&pss);
   const ULONG flags=algorithmName=="RS256"?BCRYPT_PAD_PKCS1:BCRYPT_PAD_PSS;
   if(algorithmName=="PS256"||algorithmName=="RS256")ok=BCryptVerifySignature(key,padding,static_cast<PUCHAR>(const_cast<void*>(hash.getData())),ULONG(hash.getSize()),static_cast<PUCHAR>(const_cast<void*>(signature.getData())),ULONG(signature.getSize()),flags)>=0;
   BCryptDestroyKey(key);
  }
  BCryptCloseAlgorithmProvider(algorithm,0);
 }
 return ok;
#else
 ignoreUnused(payload,signature,modulus,algorithmName);return false;
#endif
}
MemoryBlock protect(const MemoryBlock& input,bool encrypt){
#if JUCE_WINDOWS
 DATA_BLOB in{DWORD(input.getSize()),static_cast<BYTE*>(const_cast<void*>(input.getData()))},out{};
 BOOL good=encrypt?CryptProtectData(&in,L"NOORI CHOIR activation",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out):CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
 if(!good)return {};MemoryBlock result(out.pbData,out.cbData);LocalFree(out.pbData);return result;
#else
 ignoreUnused(input,encrypt);return {};
#endif
}
}
Config buildConfig(){Config c;c.enabled=NOORI_ENABLE_LICENSING!=0;c.apiBase=NOORI_LICENSE_API_BASE;c.publicModulusHex=NOORI_LICENSE_PUBLIC_MODULUS;c.stateDirectory=File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("NOORI CHOIR").getChildFile("Activation");return c;}
String computerId(){
 String identity;
#if JUCE_WINDOWS
 HKEY key=nullptr;
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Cryptography",0,KEY_READ|KEY_WOW64_64KEY,&key)==ERROR_SUCCESS){wchar_t text[256]{};DWORD size=sizeof(text),type=0;if(RegQueryValueExW(key,L"MachineGuid",nullptr,&type,reinterpret_cast<LPBYTE>(text),&size)==ERROR_SUCCESS&&type==REG_SZ)identity=String(text);RegCloseKey(key);}
#endif
 if(identity.isEmpty())identity=SystemStats::getUniqueDeviceID();
 return digest("NOORI CHOIR device v1:"+identity);
}
Grant verifyGrant(const var& token,const String& modulus,const String& expectedEmail,const String& expectedDevice,const String& nonce){
 Grant g;auto fail=[&](const String& error){g.error=error;return g;};
 if(!token.isObject())return fail("Invalid activation response.");MemoryBlock payload,signature;
 if(!decode(token.getProperty("payload",{}).toString(),payload)||!decode(token.getProperty("signature",{}).toString(),signature)||!verifySignature(payload,signature,modulus,token.getProperty("algorithm","PS256").toString()))return fail("Activation signature could not be verified.");
 const auto data=JSON::parse(String::fromUTF8(static_cast<const char*>(payload.getData()),int(payload.getSize())));
 if(!data.isObject()||data.getProperty("version",0)!=var(1)||data.getProperty("product",{}).toString()!="noori-choir")return fail("Activation is for a different product or protocol.");
 g.email=data.getProperty("email",{}).toString();g.deviceId=data.getProperty("deviceId",{}).toString();g.licenseId=data.getProperty("licenseId",{}).toString();
 if(g.email!=normaliseEmail(expectedEmail)||g.deviceId!=expectedDevice||g.licenseId.isEmpty())return fail("This activation belongs to another email or computer.");
 if(nonce.isNotEmpty()&&data.getProperty("nonce",{}).toString()!=nonce)return fail("Activation response did not match this request.");
 const auto kind=data.getProperty("kind",{}).toString();if(kind!="trial"&&kind!="full")return fail("Unknown activation type.");g.trial=kind=="trial";
 g.issuedAt=int64(data.getProperty("issuedAt",0));g.validUntil=int64(data.getProperty("validUntil",0));g.trialExpiresAt=int64(data.getProperty("trialExpiresAt",0));g.onlineRequired=bool(data.getProperty("onlineRequired",true));
 if(g.issuedAt<1577836800LL)return fail("Invalid activation time.");
 if(g.trial&&(!g.onlineRequired||g.trialExpiresAt<=g.issuedAt||g.trialExpiresAt-g.issuedAt>259200))return fail("Your trial has expired or its expiry is invalid.");
 if(g.onlineRequired&&(g.validUntil<=g.issuedAt||g.validUntil-g.issuedAt>3600))return fail("Online activation lease has expired or is invalid.");
 if(!g.onlineRequired&&(g.trial||g.validUntil!=0))return fail("Invalid offline activation.");
 if(g.trial&&g.validUntil>g.trialExpiresAt)return fail("Activation extends beyond the trial expiry.");
 g.valid=true;return g;
}
Manager::Manager(Config c):Thread("NOORI activation"),config(std::move(c)),device(computerId()){
 message=config.enabled?"Enter the email and serial supplied by NOORI.":"Development build: online activation has not been configured.";
 if(config.enabled){loadState();startThread();}
}
Manager::~Manager(){shutdown();}
void Manager::shutdown(){signalThreadShouldExit();notify();if(isThreadRunning())stopThread(-1);}
bool Manager::canUse()const noexcept{if(!config.enabled)return true;const double limit=deadline.load();return entitled.load()&&(limit==0||Time::getMillisecondCounterHiRes()<limit);}
Snapshot Manager::snapshot()const{const ScopedLock guard(lock);Snapshot s;s.configured=config.enabled;s.busy=working;s.active=canUse();s.trial=grant.trial;s.email=email;s.deviceId=device;s.message=message;s.trialExpiresAt=grant.trialExpiresAt;if(entitled&&!s.active)s.message="Online validation required. Connect to the internet and check your key.";return s;}
void Manager::activate(String account,String key){if(!config.enabled)return;const ScopedLock guard(lock);email=normaliseEmail(account);serial=key.trim().toUpperCase();if(!email.containsChar('@')||serial.isEmpty()){message="Enter your registered email and serial number.";return;}entitled=false;deadline=0;activatePending=true;message="Activating online...";notify();}
void Manager::checkOnline(){if(!config.enabled)return;const ScopedLock guard(lock);checkPending=true;notify();}
void Manager::removeLocalActivation(){const ScopedLock guard(lock);entitled=false;deadline=0;email.clear();serial.clear();cachedToken={};grant={};activatePending=checkPending=false;config.stateDirectory.getChildFile("activation.dat").deleteFile();message="Local activation removed. This does not release the key for another computer.";}
void Manager::loadState(){
 auto file=config.stateDirectory.getChildFile("activation.dat");MemoryBlock encrypted;if(!file.loadFileAsData(encrypted)||encrypted.getSize()>65536)return;const auto raw=protect(encrypted,false);if(raw.getSize()==0)return;
 const auto saved=JSON::parse(raw.toString());email=normaliseEmail(saved.getProperty("email",{}).toString());serial=saved.getProperty("serial",{}).toString();cachedToken=saved.getProperty("token",{});
 if(email.isEmpty()||serial.isEmpty())return;grant=verifyGrant(cachedToken,config.publicModulusHex,email,device,{});
 if(grant.valid&&!grant.onlineRequired){entitled=true;deadline=0;message="Full access activated for "+email;}
 else {checkPending=true;message="Checking saved activation online...";}
}
void Manager::saveState(){
 const auto saved=object({{"email",email},{"serial",serial},{"token",cachedToken}});const auto text=JSON::toString(saved,true);MemoryBlock raw(text.toRawUTF8(),text.getNumBytesAsUTF8());const auto encrypted=protect(raw,true);
 if(encrypted.getSize()==0||config.stateDirectory.createDirectory().failed()||!config.stateDirectory.getChildFile("activation.dat").replaceWithData(encrypted.getData(),encrypted.getSize()))throw std::runtime_error("Activation succeeded, but Windows could not save it. Check your user-folder permissions.");
}
var Manager::request(const String& path,const var& body){
 if(config.transport)return config.transport(path,body);
 if(!config.apiBase.startsWithIgnoreCase("https://"))throw std::runtime_error("The activation server is not configured with HTTPS.");
 int status=0;auto url=URL(config.apiBase.trimCharactersAtEnd("/")+path).withPOSTData(JSON::toString(body,true));
 auto input=url.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inPostData).withHttpRequestCmd("POST").withExtraHeaders("Content-Type: application/json\r\n").withConnectionTimeoutMs(5000).withStatusCode(&status));
 if(!input)throw std::runtime_error("Cannot reach the activation server. Check your internet connection.");
 MemoryOutputStream data;char block[1024];while(!input->isExhausted()&&!threadShouldExit()){const int n=input->read(block,sizeof(block));if(n<=0)break;data.write(block,std::size_t(n));if(data.getDataSize()>32768)throw std::runtime_error("Activation response was too large.");}
 if(threadShouldExit())throw std::runtime_error("Activation cancelled.");const auto response=JSON::parse(data.toString());
 if(status!=200){const auto error=response.getProperty("error","Activation is temporarily unavailable.").toString();throw std::runtime_error(error.toStdString());}
 return response;
}
void Manager::run(){
 while(!threadShouldExit()){
  String account,key;bool activateNow=false,check=false;
  {const ScopedLock guard(lock);activateNow=activatePending;check=checkPending||(entitled&&grant.onlineRequired&&Time::getMillisecondCounterHiRes()>=refreshAt);activatePending=checkPending=false;account=email;key=serial;}
  if((activateNow||check)&&account.isNotEmpty()&&key.isNotEmpty()){
   working=true;
   try{const auto nonce=Uuid().toString();const auto body=object({{"email",account},{"serial",key},{"deviceId",device},{"nonce",nonce}});auto token=request(activateNow?"/v1/activate":"/v1/check",body);auto verified=verifyGrant(token,config.publicModulusHex,account,device,nonce);if(!verified.valid)throw std::runtime_error(verified.error.toStdString());
    const ScopedLock guard(lock);if(account==email&&key==serial){grant=verified;cachedToken=std::move(token);const double now=Time::getMillisecondCounterHiRes();deadline=grant.onlineRequired?now+double(grant.validUntil-grant.issuedAt)*1000:0;refreshAt=grant.onlineRequired?now+double(grant.validUntil-grant.issuedAt)*500:0;entitled=true;message=(grant.trial?"Three-day trial activated for ":"Full access activated for ")+email;saveState();}
   }catch(const std::exception& e){const ScopedLock guard(lock);message=String::fromUTF8(e.what());refreshAt=Time::getMillisecondCounterHiRes()+60000;}
   working=false;
  }
  wait(1000);
 }
}
}
