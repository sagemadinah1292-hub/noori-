#pragma once
#include "Plugin.h"
class ChoirEditor final:public juce::AudioProcessorEditor {
 ChoirProcessor& p;juce::LookAndFeel_V4 look;
 std::array<juce::Slider,6> knobs;std::array<juce::Label,6> labels;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
 juce::ComboBox presets;
public:
 explicit ChoirEditor(ChoirProcessor&);~ChoirEditor()override{setLookAndFeel(nullptr);}
 void paint(juce::Graphics&)override;void resized()override;
};
