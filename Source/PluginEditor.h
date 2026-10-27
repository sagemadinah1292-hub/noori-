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
 bool keyPressed(const juce::KeyPress&)override;
 void refresh(){timerCallback();}
private:
 class WaveView final : public juce::Component {
 public:
  explicit WaveView(NooriEditor&,bool overview=false);
  void paint(juce::Graphics&)override;
  void mouseDoubleClick(const juce::MouseEvent&)override;
  void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void mouseUp(const juce::MouseEvent&)override;
  void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override;
 private:
  struct Bin {float low=0,high=0,rms=0;};
  struct Envelope {const noori::Audio* audio=nullptr;noori::Sample first=0,last=0;int width=0;float peak=0;std::vector<Bin> bins;};
  std::vector<Envelope> envelopes;
  int referenceBottom()const{return 30+(getHeight()-30)/3;}
  int originalBottom()const{return 30+2*(getHeight()-30)/3;}
  NooriEditor& editor;bool overview;int dragging=-1;noori::Sample dragTarget=0;bool rangeSelecting=false,additiveRange=false,sourceRange=false;float rangeStart=0,rangeEnd=0;
  noori::Sample sampleAt(float)const;float xAt(noori::Sample)const;
 };
 NooriProcessor& processor;std::shared_ptr<EditSession> session;
 juce::LookAndFeel_V4 look;
 juce::Colour bg,panel,line,muted,purple,ink,waveBg,referenceColour;
 std::unique_ptr<juce::Drawable> defaultLogo;
 juce::Image logo;std::shared_ptr<const juce::MemoryBlock> loadedLogo;std::uint64_t seenSerial=0;
 void applyTheme();void edit(juce::String,std::function<void()>,bool merge=false);void restoreUI();void historyStep(bool);

 WaveView waveform,overview;
 juce::TextButton importButton{"Import vocal"},captureButton{"Capture"},analyseButton{"Detect attacks"},exportButton{"Export WAV"};
 juce::TextButton undoButton{"Undo"},redoButton{"Redo"},compareButton{"Listen original"},settingsButton{"Settings"};
 juce::TextButton attackTab{"Attack"},pitchTab{"Pitch"},levelTab{"Level"},selectedButton{"Align selected attacks"},alignButton{"Quantize selected track"};
 juce::ToggleButton sampleAccurate{"Sample accurate"},breath{"Breath protection"},consonant{"Consonant protection"},vowel{"Long vowel protection"},emotion{"Emotional phrase protection"},audition{"Audition with Cubase transport"};
 juce::TextButton themeButton{"Dark theme"},overlayButton{"Overlay reference"},addButton{"Add attack"},removeButton{"Remove selected"};
 juce::TextButton referenceButton{"Import reference"},selectAllButton{"Select all attacks"},clearSelectionButton{"Clear selection"},anchorButton{"Audio start = cursor"};
 juce::ComboBox targetTrack,targetEvent,referenceTrack,referenceAnalysis,referenceStart;
 juce::Slider manualBpm;
 std::vector<ProjectTrack> projectTracks;
 std::vector<std::shared_ptr<EditSession>> batchSessions,groupSessions;
 juce::String targetID,referenceID,inventory,batchStatus;
 void refreshProjectTracks();void chooseTarget();void chooseReference();void performAlign(bool selectedOnly);
 std::vector<ProjectReferenceRegion> referenceSnapshot()const;

 juce::ComboBox matchTarget;
 juce::ComboBox grid,rhythm,mode,presets;
 juce::Slider sensitivity,strength,humanize,tolerance,swing,offset,zoom;
 juce::Label gridLabel,rhythmLabel,modeLabel,sensitivityLabel,strengthLabel,humanizeLabel,toleranceLabel,swingLabel,offsetLabel;
 juce::ScrollBar scroll{false};
 std::unique_ptr<juce::FileChooser> chooser;juce::TooltipWindow tooltips{this,500};
 double viewStart=0,viewFraction=1;bool levelView=false,addingAttack=false;int selectedMarker=-1;
 juce::Rectangle<int> side,editorArea;
 void scrollBarMoved(juce::ScrollBar*,double start)override;
 void timerCallback()override;void syncControls();void pullControls(juce::String label="Timing settings",bool merge=false);void chooseFile(bool save);
 void setupSlider(juce::Slider&,juce::Label&,juce::String,double,double,double,juce::String suffix={});
 void syncTempo();
 void bindSession(std::shared_ptr<EditSession>);void updateScroll();
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NooriEditor)
};
