#include "Editor.h"
#include "BinaryData.h"
using namespace juce;
ChoirEditor::ChoirEditor(ChoirProcessor& processor):AudioProcessorEditor(processor),p(processor){
 brandLogo=Drawable::createFromImageData(NooriBrand::NOORI_svg,NooriBrand::NOORI_svgSize);
 setLookAndFeel(&look);
 const char* ids[]={"voices","detune","timing","width","mix","gain"};
 const char* names[]={"LIVE VOICES","PITCH SPREAD","GRAIN VARIATION","LIVE WIDTH","LIVE BLEND","LIVE OUTPUT"};
 const char* suffix[]={" voices"," cents"," ms"," %"," %"," dB"};
 for(int i=0;i<6;++i){auto& s=knobs[i];s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,130,26);s.setTextValueSuffix(suffix[i]);s.setName(names[i]);addAndMakeVisible(s);labels[i].setText(names[i],dontSendNotification);labels[i].setJustificationType(Justification::centred);addAndMakeVisible(labels[i]);attachments.push_back(std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],s));}
 presets.addItem("Small ensemble",1);presets.addItem("Full ensemble",2);presets.addItem("Wide crowd",3);presets.setTextWhenNothingSelected("Starting point");presets.onChange=[this]{p.preset(presets.getSelectedId()-1);};
 theme.addItem("Dark",1);theme.addItem("Light",2);theme.setSelectedId(p.darkMode?1:2,dontSendNotification);theme.setName("Colour theme");theme.onChange=[this]{p.darkMode=theme.getSelectedId()==1;applyAppearance();};
 for(int i=1;i<=3;++i)scaling.addItem(String(i*100)+"%",i);
 scaling.setSelectedId(p.uiScale,dontSendNotification);scaling.setName("Interface scale");scaling.onChange=[this]{p.uiScale=scaling.getSelectedId();applyAppearance();};
 for(auto* c:std::initializer_list<Component*>{&presets,&theme,&scaling,&importButton,&exportButton,&cancelButton,&revealButton,&logoButton,&source,&status})addAndMakeVisible(c);
 source.setJustificationType(Justification::centredLeft);status.setJustificationType(Justification::centredLeft);
 importButton.onClick=[this]{chooseFile(0);};exportButton.onClick=[this]{chooseFile(1);};logoButton.onClick=[this]{chooseFile(2);};
 cancelButton.onClick=[this]{p.cancelJob();};
 revealButton.onClick=[this]{auto files=p.exportedFiles();if(!files.isEmpty())File(files[0]).getParentDirectory().startAsProcess();};
 auto logoPath=p.state.state.getProperty("logoPath").toString();if(logoPath.isNotEmpty())logo=ImageFileFormat::loadFrom(File(logoPath));
 applyAppearance();timerCallback();startTimerHz(8);
}
void ChoirEditor::chooseFile(int kind){
 if(chooser || (kind!=2&&p.isBusy()))return;
 chooser=std::make_unique<FileChooser>(kind==0?"Import vocal WAV":kind==1?"Select Cubase project directory":"Select supplied NOORI logo",File(),kind==0?"*.wav":kind==2?"*.png;*.jpg;*.jpeg":"",true);
 Component::SafePointer<ChoirEditor> safe(this);
 chooser->launchAsync(FileBrowserComponent::openMode | (kind==1?FileBrowserComponent::canSelectDirectories:FileBrowserComponent::canSelectFiles),[safe,kind](const FileChooser& fc){
  if(!safe)return;auto file=fc.getResult();
  if(file.exists()){
   if(kind==0)safe->p.importWav(file);
   else if(kind==1)safe->p.exportTo(file);
   else {auto image=ImageFileFormat::loadFrom(file);if(image.isValid()){safe->logo=image;safe->p.state.state.setProperty("logoPath",file.getFullPathName(),nullptr);safe->repaint();}else safe->status.setText("Cannot decode logo. Select PNG or JPEG.",dontSendNotification);}
  }
  safe->chooser.reset();
 });
}
bool ChoirEditor::isInterestedInFileDrag(const StringArray& files){return files.size()==1&&File(files[0]).hasFileExtension("wav")&&!p.isBusy();}
void ChoirEditor::filesDropped(const StringArray& files,int,int){if(isInterestedInFileDrag(files))p.importWav(File(files[0]));}
void ChoirEditor::timerCallback(){
 const bool busy=p.isBusy();importButton.setEnabled(!busy);exportButton.setEnabled(!busy&&p.hasSource());cancelButton.setEnabled(busy);revealButton.setEnabled(!p.exportedFiles().isEmpty());
 source.setText(p.sourceDescription(),dontSendNotification);
 status.setText(p.jobStatus()+(busy?"  "+String(int(p.jobProgress()*100))+"%":""),dontSendNotification);
 if(dark!=p.darkMode.load()||scale!=p.uiScale.load()){theme.setSelectedId(p.darkMode?1:2,dontSendNotification);scaling.setSelectedId(p.uiScale,dontSendNotification);applyAppearance();}
}
void ChoirEditor::applyAppearance(){
 dark=p.darkMode;scale=std::clamp(p.uiScale.load(),1,3);
 const auto text=dark?Colour(0xffe9f3ef):Colour(0xff152c2a),surface=dark?Colour(0xff243c3b):Colours::white;
 look.setColour(Slider::rotarySliderFillColourId,Colour(0xff52bca2));look.setColour(Slider::rotarySliderOutlineColourId,dark?Colour(0xff36524e):Colour(0xffc8dcd5));
 look.setColour(Slider::textBoxTextColourId,text);look.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
 look.setColour(ComboBox::backgroundColourId,surface);look.setColour(ComboBox::textColourId,text);look.setColour(ComboBox::arrowColourId,text);
 look.setColour(TextButton::buttonColourId,surface);look.setColour(TextButton::textColourOffId,text);look.setColour(Label::textColourId,text);
 for(auto& label:labels)label.setColour(Label::textColourId,text);
 source.setColour(Label::textColourId,text);status.setColour(Label::textColourId,text);
 setSize(980*scale,660*scale);resized();sendLookAndFeelChange();repaint();
}
void ChoirEditor::resized(){
 theme.setBounds(696,24,120,32);scaling.setBounds(832,24,116,32);logoButton.setBounds(28,100,170,26);presets.setBounds(696,84,252,32);
 importButton.setBounds(44,194,176,40);exportButton.setBounds(234,194,218,40);cancelButton.setBounds(466,194,110,40);revealButton.setBounds(590,194,210,40);
 source.setBounds(42,148,894,32);status.setBounds(42,248,890,70);
 for(int i=0;i<6;++i){int x=28+i*156;labels[i].setBounds(x,354,144,26);knobs[i].setBounds(x,386,144,138);}
 for(int i=0;i<getNumChildComponents();++i)getChildComponent(i)->setTransform(AffineTransform::scale(float(scale)));
}
void ChoirEditor::paint(Graphics& g){
 g.fillAll(dark?Colour(0xff102421):Colour(0xffedf4ee));g.addTransform(AffineTransform::scale(float(scale)));
 const auto text=dark?Colour(0xffe9f3ef):Colour(0xff152c2a);g.setColour(text);
 g.setColour(Colours::white);g.fillRoundedRectangle(28,20,200,76,8);
 if(logo.isValid())g.drawImageWithin(logo,38,24,180,68,RectanglePlacement::centred,false);
 else if(brandLogo)brandLogo->drawWithin(g,Rectangle<float>(38,24,180,68),RectanglePlacement::centred,1.f);
 else {g.setColour(Colours::black);g.setFont(36.f);g.drawText("NOORI",38,22,180,48,Justification::centredLeft);}
 g.setFont(27.f);g.drawText("CHOIR",248,26,320,40,Justification::centredLeft);
 g.setFont(14.f);g.drawText("Eight voices. One performance.",248,70,420,24,Justification::centredLeft);
 g.setColour(dark?Colour(0xff1b3430):Colour(0xffdceae0));g.fillRoundedRectangle(28,140,920,190,12);
 g.setColour(text);g.setFont(14.f);g.drawText("OFFLINE VOICES  /  Pitch spread and grain variation shape all eight exported voices",32,326,910,26,Justification::centredLeft);
 g.setColour(Colour(0xff52bca2));g.drawLine(28,548,948,548);
 g.setColour(text);g.setFont(14.f);g.drawText("Drop a WAV here, or Import WAV. Choose the folder containing your Cubase project.",32,562,916,26,Justification::centredLeft);
 g.drawText("Exports: NOORI CHOIR / eight 24-bit WAVs. Same sample rate, channels and duration as source.",32,588,916,26,Justification::centredLeft);
 g.setFont(12.f);g.drawText("Experimental DSP ensemble · No AI model · No ARA · Import exported files at the original event position",32,622,916,22,Justification::centredLeft);
}
