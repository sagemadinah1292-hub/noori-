#pragma once
#include "Plugin.h"
class ChoirEditor final : public juce::AudioProcessorEditor, private juce::Timer,
                          public juce::FileDragAndDropTarget {
 ChoirProcessor& p; juce::LookAndFeel_V4 look;
 std::array<juce::Slider,6> knobs; std::array<juce::Label,6> labels;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
 juce::ComboBox presets, theme, scaling;
 juce::TextButton importButton{"Import WAV"}, exportButton{"Export 8 WAVs…"},
                  cancelButton{"Cancel"}, revealButton{"Open export folder"}, logoButton{"Load NOORI logo…"};
 juce::Label source, status; juce::Image logo;std::unique_ptr<juce::Drawable> brandLogo;
 std::unique_ptr<juce::FileChooser> chooser;
 int scale=1; bool dark=true;
 void timerCallback()override; void applyAppearance(); void chooseFile(int kind);
public:
 explicit ChoirEditor(ChoirProcessor&);
 ~ChoirEditor()override{stopTimer();chooser.reset();setLookAndFeel(nullptr);}
 bool hasBrandLogo()const{return brandLogo!=nullptr;}
 void paint(juce::Graphics&)override; void resized()override;
 bool isInterestedInFileDrag(const juce::StringArray&)override;
 void filesDropped(const juce::StringArray&,int,int)override;
};
