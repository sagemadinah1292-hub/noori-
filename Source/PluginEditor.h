#pragma once
#include "PluginProcessor.h"
#include "ARAIntegration.h"
class NooriEditor final : public juce::AudioProcessorEditor,
#if JucePlugin_Enable_ARA
 public juce::AudioProcessorEditorARAExtension,
#endif
 private juce::Timer,private juce::ScrollBar::Listener {
public:
 explicit NooriEditor(NooriProcessor&);~NooriEditor()override;
 void paint(juce::Graphics&)override;void resized()override;
private:
 class WaveView final : public juce::Component {
 public:
  explicit WaveView(NooriEditor&,bool overview=false);
  void paint(juce::Graphics&)override;
  void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void mouseUp(const juce::MouseEvent&)override;
  void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override;
 private:
  NooriEditor& editor;bool overview;int dragging=-1;noori::Sample dragTarget=0;
  noori::Sample sampleAt(float)const;float xAt(noori::Sample)const;
 };
 NooriProcessor& processor;std::shared_ptr<EditSession> session;
 juce::LookAndFeel_V4 look;
 WaveView waveform,overview;
 juce::TextButton importButton{"Import vocal"},captureButton{"Capture"},analyseButton{"Detect attacks"},exportButton{"Export WAV"};
 juce::TextButton undoButton{"Undo"},redoButton{"Redo"},compareButton{"Compare original"},settingsButton{"Settings"};
 juce::TextButton attackTab{"Attack"},pitchTab{"Pitch"},levelTab{"Level"},selectedButton{"Align selected attacks"},alignButton{"ALIGN ALL ATTACKS TO GRID"};
 juce::ToggleButton sampleAccurate{"Sample accurate"},breath{"Breath protection"},consonant{"Consonant protection"},vowel{"Long vowel protection"},emotion{"Emotional phrase protection"},audition{"Audition with Cubase transport"};
 juce::ComboBox grid,rhythm,mode,presets;
 juce::Slider sensitivity,strength,humanize,tolerance,swing,offset,zoom;
 juce::Label gridLabel,rhythmLabel,modeLabel,sensitivityLabel,strengthLabel,humanizeLabel,toleranceLabel,swingLabel,offsetLabel;
 juce::ScrollBar scroll{false};
 std::unique_ptr<juce::FileChooser> chooser;juce::TooltipWindow tooltips{this,500};
 double viewStart=0,viewFraction=1;bool levelView=false;int selectedMarker=-1;
 juce::Rectangle<int> side,editorArea;
 void scrollBarMoved(juce::ScrollBar*,double start)override;
 void timerCallback()override;void syncControls();void pullControls();void chooseFile(bool save);
 void setupSlider(juce::Slider&,juce::Label&,juce::String,double,double,double,juce::String suffix={});
 void bindSession(std::shared_ptr<EditSession>);void updateScroll();
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NooriEditor)
};
