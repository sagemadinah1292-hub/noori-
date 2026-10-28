#include "Editor.h"
#include "BinaryData.h"
using namespace juce;
using namespace noori_choir;
namespace {
Colour gold(){return Colour(0xffe6d17b);}Colour ink(){return Colour(0xffd8f0e6);}
void bevel(Graphics& g,Rectangle<float> r,Colour top,Colour bottom,float radius=5){g.setColour(Colours::black.withAlpha(.35f));g.fillRoundedRectangle(r.translated(0,3),radius);g.setGradientFill(ColourGradient(top,r.getTopLeft(),bottom,r.getBottomRight(),false));g.fillRoundedRectangle(r,radius);g.setColour(Colours::white.withAlpha(.12f));g.drawRoundedRectangle(r.reduced(.5f),radius,1);}
}
class ChoirEditor::GoldLook final:public LookAndFeel_V4 {
public:
 GoldLook(){setColour(Label::textColourId,ink());setColour(Slider::textBoxTextColourId,ink());setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);setColour(ComboBox::textColourId,ink());setColour(PopupMenu::backgroundColourId,Colour(0xff471e1c));setColour(PopupMenu::textColourId,ink());setColour(PopupMenu::highlightedBackgroundColourId,Colour(0xff8f3e29));setColour(TextButton::textColourOffId,ink());setColour(TextButton::textColourOnId,gold());setColour(ToggleButton::textColourId,ink());}
 void drawButtonBackground(Graphics& g,Button& b,const Colour&,bool over,bool down)override{
  auto r=b.getLocalBounds().toFloat().reduced(2);const bool selected=b.getToggleState();bool brass=bool(b.getProperties().getWithDefault("gold",false));
  auto top=brass?Colour(0xffc6ad42):selected?Colour(0xff8b3125):Colour(0xff283e40);auto bottom=brass?Colour(0xff76601d):selected?Colour(0xff481b1b):Colour(0xff101e23);
  bevel(g,r,top.brighter(over?.15f:0),bottom,5);if(down){g.setColour(Colours::black.withAlpha(.22f));g.fillRoundedRectangle(r,5);}if(!b.isEnabled()){g.setColour(Colours::black.withAlpha(.3f));g.fillRoundedRectangle(r,5);}
 }
 void drawButtonText(Graphics& g,TextButton& b,bool,bool)override{g.setColour((bool(b.getProperties().getWithDefault("gold",false))?Colour(0xff241d08):b.getToggleState()?gold():ink()).withAlpha(b.isEnabled()?1.f:.4f));g.setFont(FontOptions(14.f,Font::bold));g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(7,3),Justification::centred,2);}
 void drawComboBox(Graphics& g,int w,int h,bool,int,int,int,int,ComboBox&)override{bevel(g,Rectangle<float>(0,0,float(w),float(h)).reduced(1),Colour(0xff873b28),Colour(0xff3c1718));g.setColour(gold());Path p;p.addTriangle(float(w-22),h*.42f,float(w-10),h*.42f,float(w-16),h*.62f);g.fillPath(p);}
 void positionComboBoxText(ComboBox& c,Label& label)override{label.setBounds(14,1,c.getWidth()-44,c.getHeight()-2);label.setFont(FontOptions(17.f));label.setColour(Label::textColourId,ink());}
 void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float position,float start,float end,Slider&)override{
  auto r=Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(8);float size=std::min(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(size,size);auto centre=r.getCentre();float radius=size*.5f;
  g.setColour(Colours::black.withAlpha(.45f));g.fillEllipse(r.translated(3,5));g.setColour(Colour(0xff263b39));g.fillEllipse(r.expanded(3));
  for(int i=0;i<72;++i){float a=float(2*pi*i/72),b=float(2*pi*(i+1)/72);float light=.5f+.5f*std::sin(a*3+1);g.setColour(Colour(0xff957420).interpolatedWith(Colour(0xfff3e18b),light));Path sector;sector.startNewSubPath(centre);sector.lineTo(centre.x+radius*std::cos(a),centre.y+radius*std::sin(a));sector.lineTo(centre.x+radius*std::cos(b),centre.y+radius*std::sin(b));sector.closeSubPath();g.fillPath(sector);}
  g.setColour(Colour(0xff3c3516));g.drawEllipse(r,2);float angle=start+position*(end-start);Point<float> tip(centre.x+std::sin(angle)*radius*.8f,centre.y-std::cos(angle)*radius*.8f);g.setColour(Colour(0xff3c321a));g.drawLine(centre.x,centre.y,tip.x,tip.y,5);g.setColour(Colour(0xffc4b266));g.drawLine(centre.x-1,centre.y-1,tip.x-1,tip.y-1,1);
 }
};
class ChoirEditor::Canvas final:public Component {public:std::function<void(Graphics&)> draw;void paint(Graphics& g)override{if(draw)draw(g);}};
class ChoirEditor::WaveDisplay final:public Component {
 std::shared_ptr<const VocalFile> audio;std::array<std::vector<std::pair<float,float>>,2> peaks;
public:
 double position=-1;
 void setAudio(std::shared_ptr<const VocalFile> data){audio=std::move(data);for(auto& p:peaks)p.clear();if(audio){const int count=std::min(1024,audio->audio.getNumSamples());for(int ch=0;ch<audio->audio.getNumChannels();++ch){auto& p=peaks[ch];p.reserve(count);for(int bin=0;bin<count;++bin){int begin=int(juce::int64(bin)*audio->audio.getNumSamples()/count),end=int(juce::int64(bin+1)*audio->audio.getNumSamples()/count);auto mm=audio->audio.findMinMax(ch,begin,end-begin);p.emplace_back(mm.getStart(),mm.getEnd());}}}repaint();}
 void paint(Graphics& g)override{auto r=getLocalBounds().toFloat();bevel(g,r.reduced(1),Colour(0xff7d2c22),Colour(0xff330f13),4);if(!audio)return;g.setColour(Colour(0xffffcc81));const int channels=audio->audio.getNumChannels();for(int ch=0;ch<channels;++ch){float mid=(ch+.5f)*getHeight()/channels,height=getHeight()*.42f/channels;g.setColour(Colour(0xffba754c));g.drawHorizontalLine(int(mid),3,float(getWidth()-3));g.setColour(Colour(0xffffcc81));const auto& p=peaks[ch];for(int i=0;i<int(p.size());++i){float x=3+(getWidth()-6)*float(i)/p.size();g.drawVerticalLine(int(x),mid-std::max(.004f,p[i].second)*height,mid-std::min(-.004f,p[i].first)*height);}}
  if(position>=0){float x=float(position/audio->audio.getNumSamples())*getWidth();g.setColour(Colours::white.withAlpha(.8f));g.drawVerticalLine(int(x),2,float(getHeight()-2));}
 }
};
class ChoirEditor::SingerRow final:public Component {
 ChoirProcessor& p;int index;WaveDisplay wave;Label name;TextButton play{"PLAY"},mute{"M"},solo{"S"},remove{"X"},regenerate{"REGEN"},exportOne{"WAV"};Slider gain,pan;
public:
 std::function<void(int)> onExport;
 SingerRow(ChoirProcessor& processor,int i,const Stem& stem):p(processor),index(i){
  wave.setAudio(stem.wet);name.setText(String(i+1).paddedLeft('0',2)+"  "+stem.name,dontSendNotification);name.setFont(FontOptions(14.f,Font::bold));
  for(auto* c:std::initializer_list<Component*>{&wave,&name,&play,&mute,&solo,&remove,&regenerate,&exportOne,&gain,&pan})addAndMakeVisible(c);
  mute.setClickingTogglesState(true);solo.setClickingTogglesState(true);auto m=p.stemMix(i);mute.setToggleState(m.mute,dontSendNotification);solo.setToggleState(m.solo,dontSendNotification);
  gain.setRange(-60,12,.1);gain.setValue(m.gain,dontSendNotification);gain.setTextValueSuffix(" dB");gain.setSliderStyle(Slider::LinearHorizontal);gain.setTextBoxStyle(Slider::TextBoxRight,false,66,20);gain.setName("Voice gain");
  pan.setRange(-1,1,.01);pan.setValue(m.pan,dontSendNotification);pan.setSliderStyle(Slider::LinearHorizontal);pan.setTextBoxStyle(Slider::TextBoxRight,false,52,20);pan.setName("Voice pan");
  auto update=[this]{p.setStemMix(index,{float(gain.getValue()),float(pan.getValue()),mute.getToggleState(),solo.getToggleState()});};gain.onValueChange=update;pan.onValueChange=update;mute.onClick=update;solo.onClick=update;
  play.onClick=[this]{if(p.isPlaying()&&p.playingVoice()==index)p.stop();else p.play(index);};remove.onClick=[this]{p.removeStem(index);};regenerate.onClick=[this]{p.generate(index);};exportOne.onClick=[this]{if(onExport)onExport(index);};
 }
 void resized()override{wave.setBounds(78,8,getWidth()-164,64);name.setBounds(78,76,getWidth()-170,22);play.setBounds(3,8,68,32);mute.setBounds(3,44,32,28);solo.setBounds(38,44,32,28);gain.setBounds(78,103,252,26);pan.setBounds(352,103,208,26);remove.setBounds(getWidth()-76,8,66,26);regenerate.setBounds(getWidth()-76,38,66,26);exportOne.setBounds(getWidth()-76,70,66,26);}
 void refresh(){wave.position=p.isPlaying()?p.playhead():-1;wave.repaint();auto enabled=!p.isBusy();remove.setEnabled(enabled);regenerate.setEnabled(enabled);exportOne.setEnabled(enabled);play.setToggleState(p.isPlaying()&&p.playingVoice()==index,dontSendNotification);}
 void paint(Graphics& g)override{g.setColour(Colours::black.withAlpha(.3f));g.drawHorizontalLine(getHeight()-1,0,float(getWidth()));}
};
class ChoirEditor::StemDragButton final:public TextButton {
 ChoirProcessor& p;bool sent=false;
public:
 explicit StemDragButton(ChoirProcessor& processor):TextButton("STEMS - DRAG TO CUBASE"),p(processor){setTooltip("Drag the prepared WAV files into Cubase. If the host refuses the drop, use EXPORT STEMS.");}
 void mouseDown(const MouseEvent& e)override{sent=false;TextButton::mouseDown(e);}
 void mouseDrag(const MouseEvent& e)override{TextButton::mouseDrag(e);if(sent||e.getDistanceFromDragStart()<8||p.isBusy())return;auto files=p.dragFiles();if(files.isEmpty())return;sent=true;DragAndDropContainer::performExternalDragDropOfFiles(files,false,this);}
};
ChoirEditor::ChoirEditor(ChoirProcessor& processor):AudioProcessorEditor(processor),p(processor),look(std::make_unique<GoldLook>()),canvas(std::make_unique<Canvas>()),sourceWave(std::make_unique<WaveDisplay>()),stemsButton(std::make_unique<StemDragButton>(p)){
 setLookAndFeel(look.get());brandLogo=Drawable::createFromImageData(NooriBrand::NOORI_svg,NooriBrand::NOORI_svgSize);addAndMakeVisible(*canvas);canvas->draw=[this](Graphics& g){paintPage(g);};
 auto add=[this](Component& c,const char* id){canvas->addAndMakeVisible(c);c.setComponentID(id);};
 add(back,"back");add(settingsButton,"settings");add(browse,"browse");add(capture,"capture");add(sourcePlay,"sourcePlay");add(sourceMute,"sourceMute");add(removeSource,"removeSource");add(create,"create");add(cancel,"cancel");add(playAll,"playAll");add(stopAll,"stopAll");add(exportAll,"exportAll");add(exportMix,"exportMix");add(openFolder,"openFolder");add(*stemsButton,"stems");add(*sourceWave,"sourceWave");add(sourceName,"sourceName");add(status,"status");add(progressLabel,"progress");add(viewport,"voices");viewport.setViewedComponent(&lanes,false);viewport.setScrollBarsShown(true,false);
 hookCombo(character,"character",{"Gospel","Kids","Shout","Naat Choir","Soft Emotional","Male Ensemble","Female Ensemble","Mixed Ensemble"});
 hookCombo(microphone,"microphone",{"Clean","Warm Vintage","Modern Studio","Warm Ribbon","Bright Condenser"});
 add(singers,"singers");for(int n:{2,4,6,8,12,16})singers.addItem(String(n)+" singers",n);singers.setSelectedId(int(p.value("voices")),dontSendNotification);singers.onChange=[this]{p.setValue("voices",float(singers.getSelectedId()));};
 hookCombo(key,"key",{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"});hookCombo(scale,"scale",{"Major","Natural Minor","Harmonic Minor","Melodic Minor"});
 hookCombo(output,"stereo",{"Mono","Stereo"});
 const char* partIds[]{"octaveUp","octaveDown","thirdAbove","thirdBelow","fifthAbove","fifthBelow"};const char* partText[]{"OCTAVE UP","OCTAVE DOWN","THIRD ABOVE","THIRD BELOW","FIFTH ABOVE","FIFTH BELOW"};
 for(int i=0;i<6;++i){add(parts[i],partIds[i]);parts[i].setButtonText(partText[i]);parts[i].setClickingTogglesState(true);buttonAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,partIds[i],parts[i]));}
 hookSlider(masterGain,"masterGain",-36,6," dB");hookSlider(lowCut,"lowCut",0,500," Hz");hookSlider(brightness,"brightness",-12,12," dB");hookSlider(reverb,"reverb",0,1,"");hookSlider(variation,"detune",0,20," cents");
 for(auto* s:{&lowCut,&brightness,&reverb}){s->onDragEnd=[this]{if(p.stemCount()>0&&!p.isBusy()){renderedFx=p.fxConfig();p.applyFx();}};s->onValueChange=[this]{fxChangedAt=Time::getMillisecondCounterHiRes();};}
 add(printFx,"printFx");add(floatWav,"floatWav");add(bwf,"bwf");buttonAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,"printFx",printFx));buttonAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,"floatWav",floatWav));buttonAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bwf",bwf));for(auto* b:{&printFx,&floatWav,&bwf})b->onClick=[this]{p.invalidateStems();};
 add(theme,"theme");theme.addItem("Dark Teal",1);theme.addItem("Light Teal",2);theme.setSelectedId(p.darkMode?1:2,dontSendNotification);theme.onChange=[this]{p.darkMode=theme.getSelectedId()==1;applyAppearance();};
 add(scaling,"scaling");for(int i=1;i<=3;++i)scaling.addItem(String(i*100)+"%",i);scaling.setSelectedId(p.uiScale,dontSendNotification);scaling.onChange=[this]{p.uiScale=scaling.getSelectedId();applyAppearance();};
 browse.onClick=[this]{chooseInput();};capture.onClick=[this]{if(p.isCapturing())p.stopCapture();else p.startCapture();};
 sourcePlay.onClick=[this]{if(p.isPlaying()&&p.playingVoice()==-2)p.stop();else p.play(-2);};sourceMute.setClickingTogglesState(true);sourceMute.onClick=[this]{p.sourceMuted=sourceMute.getToggleState();};
 removeSource.onClick=[this]{p.clearSource();showPage(Page::Import);};
 create.getProperties().set("gold",true);playAll.getProperties().set("gold",true);create.onClick=[this]{p.generate();if(p.isBusy())showPage(Page::Processing);};
 cancel.onClick=[this]{p.cancelJob();};playAll.onClick=[this]{if(p.isPlaying()&&p.playingVoice()==-1)p.stop();else p.play();};stopAll.onClick=[this]{p.stop();};
 exportAll.onClick=[this]{chooseExport(-1);};exportMix.onClick=[this]{chooseExport(-2);};openFolder.onClick=[this]{auto files=p.exportedFiles();if(files.isEmpty())files=p.dragFiles();if(!files.isEmpty())File(files[0]).getParentDirectory().startAsProcess();};
 stemsButton->onClick=[this]{if(p.stemsNeedPreparation())p.prepareStems();};
 settingsButton.onClick=[this]{if(page==Page::Settings)showPage(returnPage);else {returnPage=page;showPage(Page::Settings);}};
 back.onClick=[this]{if(page==Page::Settings)showPage(returnPage);else if(page==Page::Results)showPage(Page::Setup);else if(page==Page::Setup){p.clearSource();showPage(Page::Import);}};
 renderedFx=p.fxConfig();page=p.stemCount()>0?Page::Results:p.hasSource()?Page::Setup:Page::Import;
 applyAppearance();rebuildLanes();updateVisibility();timerCallback();startTimerHz(12);
}
ChoirEditor::~ChoirEditor(){stopTimer();chooser.reset();setLookAndFeel(nullptr);viewport.setViewedComponent(nullptr,false);}
void ChoirEditor::hookSlider(Slider& s,const char* id,double low,double high,const String& suffix){canvas->addAndMakeVisible(s);s.setComponentID(id);s.setRange(low,high,.01);s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,106,23);s.setTextValueSuffix(suffix);sliderAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state,id,s));}
void ChoirEditor::hookCombo(ComboBox& box,const char* id,StringArray items){canvas->addAndMakeVisible(box);box.setComponentID(id);int i=1;for(auto& item:items)box.addItem(item,i++);comboAttachments.push_back(std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,id,box));}
void ChoirEditor::paint(Graphics& g){g.fillAll(Colour(0xff082d32));}
void ChoirEditor::applyAppearance(){dark=p.darkMode;scaleFactor=std::clamp(p.uiScale.load(),1,3);look->setColour(Label::textColourId,dark?ink():Colour(0xff173b39));look->setColour(Slider::textBoxTextColourId,dark?ink():Colour(0xff173b39));look->setColour(ToggleButton::textColourId,dark?ink():Colour(0xff173b39));sourceName.setColour(Label::textColourId,dark?ink():Colour(0xff173b39));status.setColour(Label::textColourId,dark?ink():Colour(0xff173b39));progressLabel.setColour(Label::textColourId,dark?gold():Colour(0xff364721));setSize(720*scaleFactor,1000*scaleFactor);resized();sendLookAndFeelChange();repaint();}
void ChoirEditor::resized(){
 canvas->setBounds(0,0,720,1000);canvas->setTransform(AffineTransform::scale(float(scaleFactor)));
 back.setBounds(24,39,104,36);settingsButton.setBounds(608,39,88,36);
 sourcePlay.setBounds(22,137,57,35);sourceMute.setBounds(22,179,57,32);sourceWave->setBounds(90,132,604,83);removeSource.setBounds(652,139,34,28);sourceName.setBounds(88,218,606,30);
 if(page==Page::Import){browse.setBounds(234,620,252,48);capture.setBounds(234,690,252,42);}else {browse.setBounds(90,264,226,40);capture.setBounds(338,264,356,40);}
 character.setBounds(244,340,450,42);microphone.setBounds(244,395,450,42);singers.setBounds(244,450,212,42);output.setBounds(478,450,216,42);
 parts[0].setBounds(244,512,212,40);parts[1].setBounds(478,512,216,40);key.setBounds(244,572,212,40);scale.setBounds(478,572,216,40);
 parts[2].setBounds(244,627,212,40);parts[3].setBounds(478,627,216,40);parts[4].setBounds(244,682,212,40);parts[5].setBounds(478,682,216,40);
 create.setBounds(244,780,450,66);cancel.setBounds(250,715,220,44);progressLabel.setBounds(150,570,420,90);progressLabel.setFont(FontOptions(55.f));progressLabel.setJustificationType(Justification::centred);
 playAll.setBounds(24,140,144,64);stopAll.setBounds(184,140,100,64);masterGain.setBounds(316,126,118,116);viewport.setBounds(18,274,684,438);lanes.setSize(664,int(rows.size())*144);for(int i=0;i<int(rows.size());++i)rows[i]->setBounds(0,i*144,664,144);
 lowCut.setBounds(258,752,120,125);brightness.setBounds(408,752,120,125);reverb.setBounds(558,752,120,125);printFx.setBounds(22,850,245,36);
 exportAll.setBounds(22,913,160,38);exportMix.setBounds(190,913,140,38);openFolder.setBounds(22,953,160,30);stemsButton->setBounds(352,916,342,66);
 theme.setBounds(300,254,370,44);scaling.setBounds(300,326,370,44);floatWav.setBounds(48,416,624,40);bwf.setBounds(48,478,624,40);variation.setBounds(300,545,140,130);
 status.setBounds(24,page==Page::Results?886:912,672,page==Page::Results?25:65);status.setFont(FontOptions(12.f));
}
void ChoirEditor::showPage(Page next){page=next;updateVisibility();resized();canvas->repaint();}
void ChoirEditor::updateVisibility(){for(int i=0;i<canvas->getNumChildComponents();++i)canvas->getChildComponent(i)->setVisible(false);settingsButton.setVisible(true);status.setVisible(true);back.setVisible(page==Page::Setup||page==Page::Results||page==Page::Settings);back.setButtonText(page==Page::Setup?"START OVER":"BACK");
 auto show=[](std::initializer_list<Component*> items){for(auto* c:items)c->setVisible(true);};
 if(page==Page::Import)show({&browse,&capture});
 if(page==Page::Setup){show({&browse,&capture,&sourcePlay,&sourceMute,&removeSource,sourceWave.get(),&sourceName,&character,&microphone,&singers,&key,&scale,&output,&create});for(auto& b:parts)b.setVisible(true);}
 if(page==Page::Processing)show({&cancel,&progressLabel});
 if(page==Page::Results)show({&playAll,&stopAll,&masterGain,&viewport,&lowCut,&brightness,&reverb,&printFx,&exportAll,&exportMix,&openFolder,stemsButton.get()});
 if(page==Page::Settings)show({&theme,&scaling,&floatWav,&bwf,&variation});
}
void ChoirEditor::rebuildLanes(){rows.clear();auto s=p.session();if(s){sourceWave->setAudio(s->source);if(s->source)sourceName.setText(s->source->name+"  |  "+String(s->source->rate,0)+" Hz  |  "+String(s->source->audio.getNumSamples()/s->source->rate,2)+" s",dontSendNotification);for(int i=0;i<int(s->stems.size());++i){auto row=std::make_unique<SingerRow>(p,i,s->stems[i]);row->onExport=[this](int index){chooseExport(index);};lanes.addAndMakeVisible(*row);rows.push_back(std::move(row));}}else sourceWave->setAudio({});lastRevision=p.revision();resized();}
void ChoirEditor::timerCallback(){
 if(p.capturePending()&&!p.isBusy())p.stopCapture();
 if(p.revision()!=lastRevision){rebuildLanes();if(auto s=p.session())renderedFx=s->fx;if(page!=Page::Settings)showPage(p.stemCount()>0?Page::Results:p.hasSource()?Page::Setup:Page::Import);}
 if(page==Page::Processing&&!p.isBusy())showPage(p.stemCount()>0?Page::Results:p.hasSource()?Page::Setup:Page::Import);
 if(dark!=p.darkMode.load()||scaleFactor!=p.uiScale.load()){theme.setSelectedId(p.darkMode?1:2,dontSendNotification);scaling.setSelectedId(p.uiScale,dontSendNotification);applyAppearance();}
 status.setText(p.jobStatus()+(p.isBusy()?"  "+String(int(p.jobProgress()*100))+"%":""),dontSendNotification);progressLabel.setText(String(int(p.jobProgress()*100))+"%",dontSendNotification);
 capture.setButtonText(p.isCapturing()?"STOP CAPTURE  "+String(p.capturedSeconds(),1)+" s":"CAPTURE FROM TRACK");
 for(auto* c:std::initializer_list<Component*>{&browse,&capture,&create,&removeSource,&exportAll,&exportMix,&character,&microphone,&singers,&key,&scale,&output})c->setEnabled(!p.isBusy());
 for(auto* control:std::initializer_list<Component*>{&lowCut,&brightness,&reverb,&printFx,&floatWav,&bwf})control->setEnabled(!p.isBusy());
 back.setEnabled(!p.isBusy()&&!p.isCapturing());create.setEnabled(!p.isBusy()&&!p.isCapturing());
 stemsButton->setEnabled(!p.isBusy()&&p.stemCount()>0);stemsButton->setButtonText(p.stemsNeedPreparation()?"PREPARE STEMS":"STEMS - DRAG TO CUBASE");
 for(auto& row:rows)row->refresh();sourceWave->position=p.isPlaying()&&p.playingVoice()==-2?p.playhead():-1;sourceWave->repaint();
 if(p.stemCount()>0&&!p.isBusy()){
  const auto fx=p.fxConfig();if(fx.lowCut!=renderedFx.lowCut||fx.brightness!=renderedFx.brightness||fx.reverb!=renderedFx.reverb){if(fxChangedAt==0)fxChangedAt=Time::getMillisecondCounterHiRes();if(!lowCut.isMouseButtonDown()&&!brightness.isMouseButtonDown()&&!reverb.isMouseButtonDown()&&Time::getMillisecondCounterHiRes()-fxChangedAt>500){renderedFx=fx;p.applyFx();}}
 }
 canvas->repaint();
}
void ChoirEditor::chooseInput(){if(chooser||p.isBusy()||p.isCapturing())return;chooser=std::make_unique<FileChooser>("Choose vocal WAV or AIFF",File(),"*.wav;*.aif;*.aiff");Component::SafePointer<ChoirEditor> safe(this);chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectFiles,[safe](const FileChooser& fc){if(!safe)return;auto file=fc.getResult();if(file.existsAsFile()){safe->p.importWav(file);safe->showPage(Page::Processing);}safe->chooser.reset();});}
void ChoirEditor::chooseExport(int selected){if(chooser||p.isBusy())return;chooser=std::make_unique<FileChooser>("Choose Cubase project directory",File(),"",true);Component::SafePointer<ChoirEditor> safe(this);chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories,[safe,selected](const FileChooser& fc){if(!safe)return;auto dir=fc.getResult();if(dir.isDirectory())safe->p.exportTo(dir,selected);safe->chooser.reset();});}
bool ChoirEditor::isInterestedInFileDrag(const StringArray& files){return files.size()==1&&File(files[0]).hasFileExtension("wav;aif;aiff")&&!p.isBusy()&&!p.isCapturing();}
void ChoirEditor::filesDropped(const StringArray& files,int,int){if(isInterestedInFileDrag(files)){p.importWav(File(files[0]));showPage(Page::Processing);}}
void ChoirEditor::paintPage(Graphics& g){
 auto foreground=dark?ink():Colour(0xff173b39);Colour top=dark?Colour(0xff197c7d):Colour(0xffb5e3d5),bottom=dark?Colour(0xff104c50):Colour(0xff79b9ae);
 g.setGradientFill(ColourGradient(top,0,0,bottom,720,1000,false));g.fillAll();
 bevel(g,Rectangle<float>(12,12,696,94),Colour(0xff88762b),Colour(0xff51400e),7);
 if(brandLogo)brandLogo->drawWithin(g,Rectangle<float>(139,20,122,72),RectanglePlacement::centred,1.f);
 g.setColour(Colour(0xffffedaa));g.setFont(FontOptions(34.f,Font::bold));g.drawText("NOORI CHOIR",271,30,325,56,Justification::centredLeft);
 auto title=[&](const String& text,int x,int y,int w,int h,float size=18){g.setColour(foreground);g.setFont(FontOptions(size));g.drawFittedText(text,x,y,w,h,Justification::centredLeft,2);};
 auto separator=[&](int y){g.setColour(Colour(0xff061e24));g.fillRect(0,y,720,4);g.setColour(Colours::white.withAlpha(.1f));g.fillRect(0,y+4,720,1);};
 separator(116);
 if(page==Page::Import){g.setColour(foreground);g.setFont(FontOptions(27.f));g.drawFittedText("Transform One Voice Into a Choir",35,155,650,46,Justification::centred,2);g.setFont(FontOptions(16.f));g.drawText("Import a vocal. Choose its parts. Create and audition your choir.",24,207,672,40,Justification::centred);
  g.setColour(Colours::white.withAlpha(.3f));auto r=Rectangle<float>(20,274,680,622);for(int x=28;x<690;x+=16){g.drawLine(float(x),274,float(x+7),274);g.drawLine(float(x),896,float(x+7),896);}for(int y=282;y<890;y+=16){g.drawLine(20,float(y),20,float(y+7));g.drawLine(700,float(y),700,float(y+7));}
  bevel(g,Rectangle<float>(312,492,96,96),Colour(0xffeddb74),Colour(0xff8a6d19),48);g.setColour(Colour(0xff5d3020));Path arrow;arrow.addTriangle(348,522,348,559,377,541);g.fillPath(arrow);
  g.setColour(foreground);g.setFont(FontOptions(25.f));g.drawText("Drop your vocals to start",70,760,580,44,Justification::centred);g.setFont(FontOptions(14.f));g.drawText("WAV / AIFF  |  Mono / Stereo  |  Processed locally",50,829,620,30,Justification::centred);ignoreUnused(r);
 }
 if(page==Page::Setup){separator(318);title("CHOIR MODEL",24,337,208,48);title("DSP character presets",24,375,208,20,12);title("MICROPHONE",24,394,208,44);title("NUMBER OF SINGERS",24,449,208,44);separator(498);title("ADD OCTAVES",24,510,208,44);title("KEY / SCALE",24,570,208,44);title("ADD HARMONIES",24,626,208,44);separator(748);title("GENERATE CHOIR",24,782,208,60,19);title("Main + selected parts; remaining singers become unison voices.",25,864,670,30,14);}
 if(page==Page::Processing){bevel(g,Rectangle<float>(56,328,608,420),top.brighter(.07f),bottom,14);title("Your Choir Is Singing",146,378,470,66,30);auto track=Rectangle<float>(112,518,496,16);g.setColour(Colours::black.withAlpha(.25f));g.fillRoundedRectangle(track,8);track.setWidth(track.getWidth()*p.jobProgress());g.setColour(Colour(0xff51dbc0));g.fillRoundedRectangle(track,8);}
 if(page==Page::Results){title("MASTER GAIN",314,240,150,25,13);title(String(rows.size())+" individual vocal parts",462,160,230,54,19);separator(260);separator(728);title("POST FX",24,750,215,34,24);title("Audition the effects here.\nChoose whether to print them\nto individual exported stems.",24,790,220,58,14);title("LO-CUT",264,733,116,24,13);title("BRIGHTNESS",408,733,120,24,13);title("REVERB",562,733,112,24,13);separator(907);}
 if(page==Page::Settings){title("SETTINGS",40,160,640,58,30);title("APPEARANCE",48,254,240,44);title("INTERFACE SCALE",48,326,240,44);title("PERFORMANCE VARIATION",48,565,235,70);title("Engine: formant-aware DSP. Neural singer models are not installed.\nFile drag into Cubase requires host acceptance testing.\nProject audio is restored from the persistent local cache.\nTiming follows your source performance; no tempo stretching.",48,728,624,146,16);}
}
