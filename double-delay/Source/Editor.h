#pragma once
#include "Plugin.h"
class DelayEditor final:public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit DelayEditor(DelayProcessor&);~DelayEditor()override;
 void paint(juce::Graphics&)override;void resized()override;
private:
 struct Skin:juce::LookAndFeel_V4 {void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&)override;};
 struct Graph:juce::Component {explicit Graph(DelayEditor& owner):editor(owner){}void paint(juce::Graphics&)override;void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void mouseUp(const juce::MouseEvent&)override;juce::Point<float> point(int)const;DelayEditor& editor;int active=-1;juce::String horizontal;};
 DelayProcessor& p;Skin skin;Graph graph;
 juce::ComboBox presets;
 juce::TextButton clear{"Clear echoes"};
 std::array<juce::Slider,5> knobs;std::array<juce::Label,5> labels;
 std::array<juce::Slider,2> multipliers;std::array<juce::Label,2> multiplierLabels;
 std::array<juce::ToggleButton,2> sync;
 std::array<juce::ComboBox,2> notes,rhythms;
 juce::Slider mix,bpm;juce::Label mixLabel,bpmLabel;juce::Label status;
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::vector<std::unique_ptr<SA>> sliders;std::vector<std::unique_ptr<CA>> combos;std::vector<std::unique_ptr<BA>> buttons;
 void knob(juce::Slider&,juce::Label&,juce::String,juce::String);
 void timerCallback()override;
};
