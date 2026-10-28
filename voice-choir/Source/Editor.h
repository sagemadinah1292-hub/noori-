#pragma once
#include "Plugin.h"
class ChoirEditor final:public juce::AudioProcessorEditor,private juce::Timer,public juce::FileDragAndDropTarget {
public:
 enum class Page {Import,Setup,Processing,Results,Settings,Activation};
 explicit ChoirEditor(ChoirProcessor&);~ChoirEditor()override;
 void resized()override;void paint(juce::Graphics&)override;
 bool isInterestedInFileDrag(const juce::StringArray&)override;void filesDropped(const juce::StringArray&,int,int)override;
 void showPage(Page);Page visiblePage()const{return page;}bool hasBrandLogo()const{return brandLogo!=nullptr;}
private:
 class GoldLook;class Canvas;class WaveDisplay;class SingerRow;class StemDragButton;
 ChoirProcessor& p;std::unique_ptr<GoldLook> look;std::unique_ptr<Canvas> canvas;
 std::unique_ptr<juce::Drawable> brandLogo;
 std::unique_ptr<WaveDisplay> sourceWave;
 std::unique_ptr<StemDragButton> stemsButton;
 std::unique_ptr<juce::FileChooser> chooser;
 juce::TextButton back{"START OVER"},settingsButton{"SETTINGS"},browse{"BROWSE FILES"},capture{"CAPTURE FROM TRACK"},
 sourcePlay{"PLAY"},sourceMute{"M"},removeSource{"X"},create{"CREATE VOCALS"},cancel{"CANCEL"},
 playAll{"PLAY ALL"},stopAll{"STOP"},exportAll{"EXPORT STEMS"},exportMix{"EXPORT MIX"},openFolder{"OPEN FOLDER"};
 juce::TextButton activationButton{"REGISTRATION / TRIAL"},activateKey{"ACTIVATE KEY"},checkKey{"CHECK ONLINE"},removeKey{"REMOVE LOCAL ACTIVATION"};
 juce::TextEditor licenseEmail,licenseSerial;juce::Label licenseStatus,licenseExpiry,deviceLabel;bool lastLicensed=true;
 juce::ComboBox character,microphone,singers,key,scale,output,theme,scaling;
 std::array<juce::TextButton,6> parts;std::array<juce::TextButton,12> customNotes;
 juce::ToggleButton printFx{"PRINT POST FX TO STEMS"},floatWav{"32-bit float WAV (off = 24-bit PCM)"},bwf{"Include source BWF timestamp"};
 juce::Slider masterGain,lowCut,brightness,reverb,variation;
 juce::Label status,sourceName,progressLabel;
 juce::Viewport viewport;juce::Component lanes;std::vector<std::unique_ptr<SingerRow>> rows;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
 Page page=Page::Import,returnPage=Page::Import;int lastRevision=-1,lastScaleCode=-1;float scaleFactor=.75f;bool dark=true;double fxChangedAt=0;
 noori_choir::FxConfig renderedFx;
 void timerCallback()override;void paintPage(juce::Graphics&);void updateVisibility();void applyAppearance();void rebuildLanes();
 void chooseInput();void chooseExport(int);void hookSlider(juce::Slider&,const char*,double,double,const juce::String&);
 void hookCombo(juce::ComboBox&,const char*,juce::StringArray);void beginJob(Page);
};
