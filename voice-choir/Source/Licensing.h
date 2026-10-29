#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>

namespace noori_licensing {
struct Config {
 bool enabled=false;
 juce::String apiBase,publicModulusHex;
 juce::File stateDirectory;
 // Tests can provide a transport; distributed plugin builds use HTTPS only.
 std::function<juce::var(const juce::String&,const juce::var&)> transport;
};
struct Grant {
 bool valid=false,trial=false,onlineRequired=true;
 juce::int64 issuedAt=0,validUntil=0,trialExpiresAt=0;
 juce::String email,deviceId,licenseId,error;
};
struct Snapshot {
 bool configured=false,busy=false,active=false,trial=false,registered=false;
 juce::String message,email,deviceId;
 juce::int64 trialExpiresAt=0;
};
Config buildConfig();
juce::String computerId();
Grant verifyGrant(const juce::var&,const juce::String& publicModulusHex,
 const juce::String& email,const juce::String& deviceId,const juce::String& nonce);

class Manager final:private juce::Thread {
public:
 explicit Manager(Config config=buildConfig());
 ~Manager()override;
 void activate(juce::String email,juce::String serial);
 void checkOnline();void removeLocalActivation();void shutdown();
 bool canUse()const noexcept;
 Snapshot snapshot()const;
private:
 Config config;const juce::String device;
 mutable juce::CriticalSection lock;
 juce::String email,serial,message;
 juce::var cachedToken;
 Grant grant;
 std::atomic<bool> entitled{false},working{false};
 std::atomic<double> deadline{0};
 bool activatePending=false,checkPending=false;
 double refreshAt=0;
 void run()override;
 juce::var request(const juce::String&,const juce::var&);
 void loadState();void saveState();
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Manager)
};
}
