#pragma once
#include "PluginProcessor.h"
#if JucePlugin_Enable_ARA
class NooriModification final : public juce::ARAAudioModification {
public:
 NooriModification(juce::ARAAudioSource*,ARA::ARAAudioModificationHostRef,const juce::ARAAudioModification*);
 ~NooriModification()override;
 std::shared_ptr<EditSession> session;
 bool needsLoad=true,needsTimeline=true;
};
std::shared_ptr<EditSession> selectedARASession(NooriProcessor&);
#endif
