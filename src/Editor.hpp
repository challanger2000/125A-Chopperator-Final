#pragma once
#include "Parameters.hpp"
#include "EditorPreferences.hpp"
#include "EditorGeometry.hpp"
#include "vstgui/plugin-bindings/vst3editor.h"
#if SMTG_OS_LINUX
#include "vstgui/lib/platform/linux/linuxfactory.h"
#include "public.sdk/source/vst/vstgui_linux_runloop_support.h"
#endif
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/cvstguitimer.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "ValueEntry.hpp"
#include <charconv>
#include <string>
#include "vstgui/uidescription/uiattributes.h"
#include <array>

namespace chopper {
// SDK-native CTextEdit opens its platform field only on double click.
// Keep the raw user entry intact through the SDK's valueChanged callback;
// compact formatting applies to canonical controller feedback only.
class ValueReadout final:public VSTGUI::CTextEdit{
public:
    explicit ValueReadout(Steinberg::Vst::EditController* c,EditorPreferences& p):CTextEdit(VSTGUI::CRect(0,0,1,1),nullptr,-1,nullptr,nullptr,kDoubleClickStyle),controller(c),prefs(p){
        setImmediateTextChange(false);
    }
    void setText(const VSTGUI::UTF8String& txt) override{
        // Controller automation may update the canonical value while the
        // platform field is open. The unfinished user string wins until
        // commit/cancel. looseFocus refreshes from the latest controller.
        if(getPlatformTextEdit())return;
        if(rawNext){rawNext=false;CTextEdit::setText(txt);return;}
        if(getTag()<104){CTextEdit::setText(txt);return;}
        auto text=txt.getString();
        for(auto& c:text)if(c==',')c='.';
        auto s=trimEntry(text);if(s.ends_with("%"))s.remove_suffix(1);
        double value=0;
        if(!entryNumber(s,value)){CTextEdit::setText(txt);return;}
        if(std::abs(value)<.05)value=0;
        char buffer[32]{};
        const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value,std::chars_format::fixed,1);
        if(result.ec!=std::errc{}){CTextEdit::setText(txt);return;}
        std::string display(buffer,result.ptr);
        if(display.ends_with(".0"))display.resize(display.size()-2);
        CTextEdit::setText(VSTGUI::UTF8String(display+"%"));
    }
    void takeFocus() override{
        if(!getPlatformTextEdit())editEpoch=prefs.recallEpoch;
        CTextEdit::takeFocus();
    }
    void looseFocus() override{
        VSTGUI::CBaseObjectGuard keep(this);
        auto field=getPlatformTextEdit();
        if(!field)return;
        double value=0;
        const auto input=field->getText();
        // Invalid input or Escape never emits an edit or changes the sound.
        if(editEpoch!=prefs.recallEpoch||!parseEntry(static_cast<Param>(getTag()),input.getString(),value))
            field->setText(getText());
        rawNext=field->getText()!=getText();
        CTextEdit::looseFocus();
        rawNext=false;
        Steinberg::Vst::String128 canonical{};
        const auto id=static_cast<Steinberg::Vst::ParamID>(getTag());
        if(controller->getParamStringByValue(id,controller->getParamNormalized(id),canonical)==Steinberg::kResultOk){
            std::string ascii;
            for(auto c:canonical){if(!c)break;if(c>127)return;ascii+=static_cast<char>(c);}
            setText(VSTGUI::UTF8String(ascii));invalid();
        }
    }
protected:
    void platformOnKeyboardEvent(VSTGUI::KeyboardEvent& event) override{
        if(event.type==VSTGUI::EventType::KeyDown&&event.virt==VSTGUI::VirtualKey::Escape)
            if(auto field=getPlatformTextEdit())field->setText(getText());
        CTextEdit::platformOnKeyboardEvent(event);
    }
private:
    Steinberg::Vst::EditController* controller;
    EditorPreferences& prefs;
    std::uint64_t editEpoch=0;
    bool rawNext=false;
};

// Static, vector-drawn faceplate only. Keep all existing controls, their
// hit geometry, original branded assets and timing-diagram colors intact.
// Background layer lies behind every interactive view.
class GlassFaceplate final:public VSTGUI::CView{
public:
    GlassFaceplate():CView(VSTGUI::CRect(0,0,960,576)){setMouseEnabled(false);setTransparency(false);}
    void draw(VSTGUI::CDrawContext* c) override{
        using namespace VSTGUI;
        const auto gradient=[&](const CRect& r,double radius,CColor a,CColor b,CPoint start,CPoint end){
            auto path=owned(c->createRoundRectGraphicsPath(r,radius));
            auto colors=owned(CGradient::create(0.,1.,a,b));
            if(path&&colors)c->fillLinearGradient(path.get(),*colors,start,end);
        };
        const auto edge=[&](const CRect& r,double radius,CColor color){
            auto path=owned(c->createRoundRectGraphicsPath(r,radius));
            if(path){c->setFrameColor(color);c->setLineWidth(1);c->drawGraphicsPath(path.get(),CDrawContext::kPathStroked);}
        };
        // Satin graphite chassis. No lacquer reflection or specular sweep.
        c->setFillColor(CColor(23,25,27,255));c->drawRect(CRect(0,0,960,576),kDrawFilled);
        gradient({1,1,959,575},7,{35,37,40,255},{23,25,27,255},{0,0},{0,576});
        edge({1.5,1.5,958.5,574.5},7,{68,71,75,255});
        c->setFrameColor(CColor(78,81,85,255));c->setLineWidth(1);
        c->drawLine({12,3},{948,3});
        c->setFrameColor(CColor(12,14,16,255));c->drawLine({12,573},{948,573});
        for(const CRect r:{CRect(24,224,936,400),CRect(24,414,936,518)}){
            gradient(r,5,{39,41,44,255},{31,33,36,255},{r.left,r.top},{r.left,r.bottom});
            edge(r,5,{51,54,57,255});
            c->setFrameColor(CColor(55,58,61,255));c->drawLine({r.left+7,r.top+1},{r.right-7,r.top+1});
            c->setFrameColor(CColor(19,21,23,255));c->drawLine({r.left+7,r.bottom-1},{r.right-7,r.bottom-1});
        }
        // Thin glass-display bezel outside the verified orange-pulse canvas.
        gradient({21,69,939,213},5,{146,153,161,255},{7,9,12,255},{21,69},{21,213});
        edge({21.5,69.5,938.5,212.5},5,{77,81,86,255});
        setDirty(false);
    }
};

class Preview final:public VSTGUI::CView {
    static constexpr std::array<Param,7> ids{Param::start,Param::mid,Param::end,Param::length,Param::position,Param::curve,Param::width};
public:
    Preview(const VSTGUI::CRect& r,Steinberg::Vst::EditController* c,EditorPreferences& p):CView(r),controller(c),prefs(p){
        setMouseEnabled(false);setTransparency(false);refresh(false);
    }
    bool attached(VSTGUI::CView* parent) override{
        if(!CView::attached(parent))return false;
        prefs.watches.push_back({this,[this]{pending=true;}});
        // Use a dedicated GUI timer rather than the SDK global idle updater.
        // A dirty bit coalesces rapid drags/automation to one render per frame.
        timer=VSTGUI::owned(new VSTGUI::CVSTGUITimer([this](VSTGUI::CVSTGUITimer*){
            if(refresh(false)||pending){pending=false;repaint();}
        },33,true));
        refresh(false);return true;
    }
    bool removed(VSTGUI::CView* parent) override{
        if(timer){timer->stop();timer=nullptr;}
        prefs.remove(this);
        return CView::removed(parent);
    }
    void draw(VSTGUI::CDrawContext* c) override {
        refresh(false);
        if(trace&&lastDrawn!=previous){
            lastDrawn=previous;
            std::fprintf(stderr,"PREVIEW_RENDER_TRACE start=%.8f mid=%.8f end=%.8f length=%.8f position=%.8f curve=%.8f width=%.8f pulses=%zu first_end=%.9f\n",previous[0],previous[1],previous[2],previous[3],previous[4],previous[5],previous[6],count,count?ends[0]:0.);
        }
        using namespace VSTGUI;
        const auto r=getViewSize();
        // Owner's Unbenannt.svg: viewBox 340 x 110. This is a timing
        // diagram, not an audio oscilloscope or gain-envelope plot.
        const auto x=[&](double v){return r.left+r.getWidth()*v/340;};
        const auto y=[&](double v){return r.top+r.getHeight()*v/110;};
        c->setFillColor(CColor(16,17,19,255));c->drawRect(r,kDrawFilled);
        c->setFrameColor(CColor(69,69,69,255));c->setLineWidth(1);
        c->drawRect(CRect(x(1),y(1),x(339),y(109)),kDrawStroked);
        c->setFrameColor(CColor(52,52,52,255));
        for(int i=0;i<=4;++i){
            const double xx=x(10+80*i);
            for(int v=8;v<100;v+=6)c->drawLine({xx,y(v)},{xx,y(std::min(100,v+3))});
        }
        c->setFillColor(CColor(232,155,64,255));
        for(std::size_t i=0;i<count;++i){
            const double left=x(10+320*starts[i]),right=x(10+320*ends[i]);
            if(right>left)c->drawRect(CRect(left,y(16),right,y(69)),kDrawFilled);
        }
        c->setFont(kNormalFontSmall);c->setFontColor(CColor(170,170,170,255));
        c->drawString("START",CRect(x(10),y(78),x(70),y(98)),kLeftText);
        // Keep the actual timing position unchanged. Only the label is
        // shifted at the extreme ends, where glyphs would otherwise collide.
        const double halfMid=c->getStringWidth("MID")*.5;
        const double labelMin=x(10)+c->getStringWidth("START")+6+halfMid;
        const double labelMax=x(330)-c->getStringWidth("END")-6-halfMid;
        const double midX=std::clamp(x(10+320*position),labelMin,labelMax);
        c->drawString("MID",CRect(midX-28*r.getWidth()/340,y(78),midX+28*r.getWidth()/340,y(98)),kCenterText);
        c->drawString("END",CRect(x(270),y(78),x(330),y(98)),kRightText);
        setDirty(false);
    }
private:
    void repaint(){
        // Invalidate the whole logical panel via its parent, including the
        // preview area. Avoid partial-composition clipping/stale regions.
        if(auto* parent=getParentView())parent->invalid();
    }
    bool refresh(bool redraw){
        Parameters p;
        std::array<double,ids.size()> now{};
        for(std::size_t i=0;i<ids.size();++i){
            now[i]=controller->getParamNormalized(static_cast<Steinberg::Vst::ParamID>(ids[i]));
            p.set(ids[i],now[i]);
        }
        if(initialized&&now==previous)return false;
        previous=now;initialized=true;
        const auto s=p.settings();position=s.position;
        // In 4/4 one bar contains one whole note. Invert the same
        // integrated phase law used by Core for each gate edge.
        // No sample-drop or fictitious gaps; WIDTH defines the open fraction
        // in phase units, not an approximate screen-space percentage.
        const double total=s.bars*integral(s,1);
        const auto inverse=[&](double phase){
            if(phase<=0)return 0.;
            if(phase>=total)return 1.;
            double lo=0,hi=1;
            for(int k=0;k<28;++k){
                const double mid=(lo+hi)*.5;
                if(s.bars*integral(s,mid)<phase)lo=mid;else hi=mid;
            }
            return (lo+hi)*.5;
        };
        count=0;
        if(s.width>0){
            for(std::size_t cycle=0;cycle<starts.size()&&double(cycle)<total;++cycle){
                starts[count]=inverse(double(cycle));
                ends[count]=inverse(double(cycle)+s.width);
                ++count;
            }
        }
        if(trace)std::fprintf(stderr,"PREVIEW_MODEL_TRACE start=%.8f pulses=%zu first_end=%.9f\n",now[0],count,count?ends[0]:0.);
        if(redraw)repaint();return true;
    }
    Steinberg::Vst::EditController* controller;
    EditorPreferences& prefs;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
    bool pending=false;
    const bool trace=std::getenv("CHOPPER_GUI_TRACE")!=nullptr;
    std::array<double,7> previous{},lastDrawn{};
    std::array<double,4096> starts{},ends{};
    std::size_t count=0;
    double position=.5;
    bool initialized=false;
};

// Editor-only controls: no VST3 parameter, automation or DSP mutation.
class HelpButton final:public VSTGUI::CView{
public:
    HelpButton(EditorPreferences& p):CView(VSTGUI::CRect(496,20,520,44)),prefs(p){}
    void draw(VSTGUI::CDrawContext* c) override{
        using namespace VSTGUI;
        auto r=getViewSize();r.inset(2,2);
        c->setFillColor(prefs.help?CColor(232,155,64,255):CColor(36,39,42,255));
        c->setFrameColor(CColor(109,109,109,255));c->setLineWidth(1);c->drawEllipse(r,kDrawFilledAndStroked);
        c->setFont(kNormalFont);c->setFontColor(prefs.help?CColor(16,17,19,255):CColor(226,227,228,255));
        c->drawString("?",r,kCenterText);setDirty(false);
    }
    void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override{
        VSTGUI::CBaseObjectGuard keep(this);
        if(e.buttonState.isLeft()){prefs.changeHelp(!prefs.help);invalid();e.consumed=true;}
    }
private:
    EditorPreferences& prefs;
};
class ResizeGrip final:public VSTGUI::CView{
public:
    ResizeGrip(std::function<double()> s,std::function<void(double)> cb):
        CView(VSTGUI::CRect(940,556,960,576)),scale(std::move(s)),resize(std::move(cb)){setMouseableArea(getViewSize());setMouseEnabled(true);}
    void draw(VSTGUI::CDrawContext* c) override{
        using namespace VSTGUI;auto r=getViewSize();
        c->setFrameColor(CColor(168,170,172,255));c->setLineWidth(1.2);
        for(int i:{5,10,15})c->drawLine({r.right-i-2,r.bottom-3},{r.right-3,r.bottom-i-2});
        setDirty(false);
    }
    void onMouseEnterEvent(VSTGUI::MouseEnterEvent&) override{if(getFrame())getFrame()->setCursor(VSTGUI::kCursorNWSESize);}
    void onMouseExitEvent(VSTGUI::MouseExitEvent&) override{if(getFrame()&&!dragging)getFrame()->setCursor(VSTGUI::kCursorDefault);}
    void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override{
        if(!e.buttonState.isLeft())return;
        dragging=true;initialWidth=960*scale();anchor=e.mousePosition.x*scale();e.consumed=true;
        if(std::getenv("CHOPPER_GUI_TRACE"))std::fprintf(stderr,"GRIP_DOWN x=%.8f scale=%.8f width=%.8f anchor=%.8f\n",e.mousePosition.x,scale(),initialWidth,anchor);
    }
    void onMouseMoveEvent(VSTGUI::MouseMoveEvent& e) override{
        if(!dragging)return;
        // Events arrive in the scaled logical view coordinates. Convert to
        // physical width with the current scale to avoid compounded deltas.
        const double width=initialWidth+e.mousePosition.x*scale()-anchor;
        if(std::getenv("CHOPPER_GUI_TRACE"))std::fprintf(stderr,"GRIP_MOVE x=%.8f scale=%.8f width=%.8f\n",e.mousePosition.x,scale(),width);
        resize(width);e.consumed=true;
    }
    void onMouseUpEvent(VSTGUI::MouseUpEvent& e) override{
        if(dragging){dragging=false;if(getFrame())getFrame()->setCursor(VSTGUI::kCursorDefault);e.consumed=true;}
    }
    void onMouseCancelEvent(VSTGUI::MouseCancelEvent& e) override{dragging=false;if(getFrame())getFrame()->setCursor(VSTGUI::kCursorDefault);e.consumed=true;}
private:
    std::function<double()> scale;
    std::function<void(double)> resize;
    double initialWidth=960,anchor=0;
    bool dragging=false;
};


// Draw contextual help inside the scaled plugin canvas, not in a separate
// Win32 tooltip HWND whose stacking/focus is controlled by the DAW.
// The overlay never participates in mouse hit testing.
class HelpOverlay final:public VSTGUI::CView{
public:
    HelpOverlay():CView(VSTGUI::CRect(0,0,960,576)){setMouseEnabled(false);setTransparency(true);}
    void show(int tag,const VSTGUI::CRect&){
        static constexpr const char* tips[]{
            "START: initial rhythmic note division.",
            "MID: note division at the middle point.",
            "END: final rhythmic note division.",
            "LENGTH: duration of the pattern in bars.",
            "MID POSITION: location of MID within the pattern.",
            "CURVE: shape of the rate transition. 0% is linear.",
            "SHAPE: waveform of each rhythmic impulse.",
            "WIDTH: open fraction of each impulse. 50% = half open.",
            "ATTACK: fade-in time relative to the impulse period.",
            "RELEASE: fade-out time relative to the impulse period.",
            "MIX: dry/wet balance. 0% is dry.",
            "ACTIVE: starts the pattern on an OFF to ON transition.",
            "MODE: play once or repeat continuously."
        };
        if(tag<100||tag>112){hide();return;}
        tip=tips[tag-100];invalidatePanel();
    }
    void hide(){if(tip){tip=nullptr;invalidatePanel();}}
    void draw(VSTGUI::CDrawContext* c) override{
        using namespace VSTGUI;
        if(tip){
            // Quiet, centered text in the existing free header strip.
            // No colored panel, outline, or overlap with other controls.
            c->setFont(kNormalFontSmall);c->setFontColor(CColor(191,193,196,255));
            c->drawString(tip,CRect(24,52,936,70),kCenterText);
        }
        setDirty(false);
    }
private:
    void invalidatePanel(){if(auto* p=getParentView())p->invalid();}
    const char* tip=nullptr;
};

class Editor final:public VSTGUI::AspectRatioVST3Editor{
public:
    explicit Editor(Steinberg::Vst::EditController* c,EditorPreferences& p):AspectRatioVST3Editor(c,"view","chopper.uidesc"),preferences(p){
        setMinZoomFactor(.75);enableTooltips(false);
        setZoomFactor(p.zoom);
        const auto pixels=editorPixels(p.zoom,1.);
        rect.right=pixels.width;rect.bottom=pixels.height;
    }
    // Linear drag is the primary input, independent of the host preference.
    int32_t getKnobMode() const override{return VSTGUI::kLinearMode;}
    Steinberg::tresult PLUGIN_API attached(void* parent,Steinberg::FIDString type) override{
        if(!parent||!type||opened||dpiInFlight||isPlatformTypeSupported(type)!=Steinberg::kResultTrue)
            return Steinberg::kResultFalse;
        // The pinned SDK's VSTGUIEditor::attached returns EditorView::attached
        // even when virtual open() returns false. Reject failed resource
        // preflight before entering that SDK path; never report a blank editor.
#if SMTG_OS_LINUX
        // The pinned VSTGUI X11 Frame calls RunLoop::init before installing
        // its frame-local runloop. A host may supply IRunLoop on IPlugFrame
        // instead of the factory context. Initialize before that dereference.
        auto* factory=VSTGUI::getPlatformFactory().asLinuxFactory();
        if(!factory)return Steinberg::kResultFalse;
        if(!factory->getRunLoop() &&
           (!plugFrame || !Steinberg::Linux::setupVSTGUIRunloop(plugFrame)))
            return Steinberg::kResultFalse;
#endif
        if(!resourcesValid())return Steinberg::kResultFalse;
        const auto result=AspectRatioVST3Editor::attached(parent,type);
        if(result==Steinberg::kResultTrue&&!opened){
            // Undo the base attachment bookkeeping, without closing an editor
            // that never opened or decrementing its global idle-update count.
            Steinberg::Vst::EditorView::removed();
            return Steinberg::kResultFalse;
        }
        return result;
    }
protected:
    VSTGUI::CView* verifyView(VSTGUI::CView* view,const VSTGUI::UIAttributes& attrs,const VSTGUI::IUIDescription* desc) override{
        auto* result=AspectRatioVST3Editor::verifyView(view,attrs,desc);
        if(auto* knob=dynamic_cast<VSTGUI::CKnobBase*>(result)){
            // A full LENGTH sweep takes 195 logical pixels: three per step.
            // The three short note lists and continuous controls use 150 px.
            knob->setKnobRange(knob->getTag()==103?195.f:150.f);
            knob->setZoomFactor(5.f);
            knob->setWantsFocus(true);
        }
        return result;
    }
    void onMouseEntered(VSTGUI::CView* view,VSTGUI::CFrame*) override{
        if(!helpOverlay)return;
        if(auto* control=dynamic_cast<VSTGUI::CControl*>(view)){
            const auto tag=control->getTag();
            if(tag>=100&&tag<=112){hovered=view;if(preferences.help)helpOverlay->show(tag,view->getViewSize());}
        }
    }
    void onMouseExited(VSTGUI::CView* view,VSTGUI::CFrame*) override{
        if(view==hovered){hovered=nullptr;if(helpOverlay)helpOverlay->hide();}
    }
    void onMouseEvent(VSTGUI::MouseEvent& event,VSTGUI::CFrame* frame) override{
        if(event.type==VSTGUI::EventType::MouseDown&&helpOverlay)helpOverlay->hide();
        AspectRatioVST3Editor::onMouseEvent(event,frame);
    }
    void onViewRemoved(VSTGUI::CFrame* frame,VSTGUI::CView* view) override{
        if(view==hovered){hovered=nullptr;if(helpOverlay)helpOverlay->hide();}
        if(view==helpOverlay)helpOverlay=nullptr;
        if(view==helpButton)helpButton=nullptr;
        AspectRatioVST3Editor::onViewRemoved(frame,view);
    }
    bool resourcesValid(){
        // Fail explicitly rather than presenting a blank or low-resolution editor.
        auto* desc=getUIDescription();
        if(!desc)return false;
        for(const char* name:{"knobL","knobS"}){
            auto* b=dynamic_cast<VSTGUI::CMultiFrameBitmap*>(desc->getBitmap(name));
            if(!b||!b->isLoaded()||b->getNumFrames()!=128||b->getNumFramesPerRow()!=16)return false;
            for(double scale:{.5,.75,1.,1.5}){
                auto selected=b->getBestPlatformBitmapForScaleFactor(scale);
                if(!selected||selected->getScaleFactor()!=scale||
                   selected->getSize().x!=b->getWidth()*scale||selected->getSize().y!=b->getHeight()*scale){
                    std::fprintf(stderr,"EDITOR_RESOURCE_REJECT %s scale=%.2f\n",name,scale);
                    return false;
                }
            }
        }
        auto* logo=desc->getBitmap("logo");
        if(!logo||!logo->isLoaded()||logo->getWidth()!=84||logo->getHeight()!=42)return false;
        auto logo3=logo->getBestPlatformBitmapForScaleFactor(3.);
        if(!logo3||logo3->getScaleFactor()!=3.||logo3->getSize().x!=252||logo3->getSize().y!=126){
            std::fprintf(stderr,"EDITOR_RESOURCE_REJECT logo scale=3\n");
            return false;
        }
        return true;
    }
    bool PLUGIN_API open(void* parent,const VSTGUI::PlatformType& type) override{
        if(!resourcesValid())return false;
        setZoomFactor(preferences.zoom);
        const bool ok=AspectRatioVST3Editor::open(parent,type);
        opened=ok;
        if(ok)preferences.uiWatches.push_back({this,[this]{syncPreferences();}});
        return ok;
    }
    void PLUGIN_API close() override{
        preferences.remove(this);opened=false;hovered=nullptr;helpOverlay=nullptr;helpButton=nullptr;
        // The outer DPI operation holds the view, but no frame operation
        // spans the host resizeView call. Complete the detach synchronously:
        // the host may destroy the parent as soon as removed returns.
        AspectRatioVST3Editor::close();
    }
#ifdef VST3_CONTENT_SCALE_SUPPORT
    Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override{
        if(!std::isfinite(factor)||factor<=0||factor>10000)return Steinberg::kInvalidArgument;
        if(dpiInFlight||resizing||syncing)return Steinberg::kResultFalse;
        Steinberg::IPtr<Steinberg::IPlugView> keep(this);
        const auto oldScale=getContentScaleFactor();
        if(factor==oldScale)return Steinberg::kResultTrue;
        const auto userZoom=getZoomFactor();
        const auto pixels=editorPixels(userZoom,factor);
        if(!getFrame()){
            contentScaleFactor=factor;
            rect.right=rect.left+pixels.width;rect.bottom=rect.top+pixels.height;
            return Steinberg::kResultTrue;
        }
        // Negotiate the canonical pixel size BEFORE entering CFrame::setZoom.
        // The host can close the view synchronously; no SDK frame operation
        // is suspended across that external call. onSize applies the scale
        // only after an exact acknowledgement, with resize recursion disabled.
        dpiInFlight=true;dpiAcknowledged=false;
        syncing=true;syncTargetZoom=userZoom;contentScaleFactor=factor;
        Steinberg::ViewRect requested{0,0,pixels.width,pixels.height};
        const bool accepted=(rect.getWidth()==pixels.width&&rect.getHeight()==pixels.height)
            ?onSize(&requested)==Steinberg::kResultTrue
            :requestResize(VSTGUI::CPoint(pixels.width,pixels.height));
        // The host may already have destroyed the child AND parent.
        // Never start a frame operation after synchronous removal.
        const bool success=accepted&&dpiAcknowledged&&opened;
        if(!success){
            contentScaleFactor=oldScale;
            // A host that acknowledged and then rejected is contradictory,
            // but restore our own frame/rect without another host request.
            if(dpiAcknowledged&&opened){
                const auto prior=editorPixels(userZoom,oldScale);
                Steinberg::ViewRect previous{0,0,prior.width,prior.height};
                resizing=true;
                if(auto* frame=getFrame())frame->setZoom(userZoom*oldScale);
                VSTGUI::VST3Editor::onSize(&previous);
                resizing=false;
            }
        }
        dpiInFlight=false;syncing=false;
        return success?Steinberg::kResultTrue:Steinberg::kResultFalse;
    }
#endif
    bool beforeSizeChange(const VSTGUI::CRect& newSize,const VSTGUI::CRect& oldSize) override{
        // The host already accepted this size via onSize. Do not ask it
        // again while applying its acknowledgement to the native frame.
        if(dpiInFlight&&!opened)return false;
        if(resizing)return true;
        return VSTGUI::VST3Editor::beforeSizeChange(newSize,oldSize);
    }
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) override{
        if(!r)return Steinberg::kInvalidArgument;
        if(dpiInFlight&&!opened)return Steinberg::kResultFalse;
        // On initial attachment and reentrant host acknowledgement, use
        // the SDK base path without overwriting the saved editor preferences.
        if(!opened||resizing)return VSTGUI::VST3Editor::onSize(r);
        Steinberg::IPtr<Steinberg::IPlugView> keep(this);
        const auto oldZoom=getZoomFactor();
        const auto scale=getContentScaleFactor();
        const auto target=editorPixels(syncTargetZoom,scale);
        const bool matchesTarget=syncing&&r->getWidth()==target.width&&r->getHeight()==target.height;
        if(dpiInFlight&&!matchesTarget)return Steinberg::kResultFalse;
        if(!editorSizeValid(r->getWidth(),r->getHeight(),scale))return Steinberg::kResultFalse;
        // Preserve exact logical zoom for our own canonical requests; do
        // not reconstruct it from a quantized physical pixel width.
        const auto factor=matchesTarget?syncTargetZoom:editorZoomForWidth(r->getWidth(),scale);
        if(std::getenv("CHOPPER_GUI_TRACE"))std::fprintf(stderr,"EDITOR_ACCEPT_SIZE %d x %d zoom=%.9f\n",r->getWidth(),r->getHeight(),factor);
        resizing=true;
        setZoomFactor(factor);
        // On DPI-only changes the logical zoom is unchanged, so explicitly
        // apply the new transform here, not on the outer host callback stack.
        if(dpiInFlight){
            auto* frame=getFrame();
            if(!frame||!frame->setZoom(factor*scale)){
                resizing=false;return Steinberg::kResultFalse;
            }
        }
        const auto result=VSTGUI::VST3Editor::onSize(r);
        if(result!=Steinberg::kResultTrue)setZoomFactor(oldZoom);
        resizing=false;
        if(dpiInFlight&&result==Steinberg::kResultTrue)dpiAcknowledged=true;
        if(result==Steinberg::kResultTrue&&!syncing&&opened)preferences.changeZoom(factor);
        return result;
    }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* r) override{
        if(!r)return Steinberg::kInvalidArgument;
        const auto scale=getContentScaleFactor();
        const auto lo=editorPixels(.75,scale),hi=editorPixels(1.5,scale);
        const auto w=std::clamp(r->getWidth(),lo.width,hi.width);
        const auto target=editorPixels(syncTargetZoom,scale);
        const auto h=syncing&&w==target.width?target.height:
            w==lo.width?lo.height:w==hi.width?hi.height:static_cast<int>(std::floor(w*.6));
        r->right=r->left+w;r->bottom=r->top+h;
        return Steinberg::kResultTrue;
    }
    VSTGUI::CView* createView(const VSTGUI::UIAttributes& attrs,const VSTGUI::IUIDescription* desc) override{
        const auto* name=attrs.getAttributeValue("custom-view-name");
        if(name&&*name=="GlassFaceplate")return new GlassFaceplate();
        if(name&&*name=="HelpButton"){helpButton=new HelpButton(preferences);return helpButton;}
        if(name&&*name=="HelpOverlay"){helpOverlay=new HelpOverlay();return helpOverlay;}
        if(name&&*name=="ResizeGrip")return new ResizeGrip([this]{return getZoomFactor()*getContentScaleFactor();},[this](double width){
            Steinberg::ViewRect r{0,0,static_cast<Steinberg::int32>(std::lround(std::clamp(width,720*getContentScaleFactor(),1440*getContentScaleFactor()))),0};
            r.bottom=static_cast<Steinberg::int32>(std::floor(r.right*.6));
            if(checkSizeConstraint(&r)==Steinberg::kResultTrue){
                const bool accepted=requestResize(VSTGUI::CPoint(r.getWidth(),r.getHeight()));
                if(std::getenv("CHOPPER_GUI_TRACE"))std::fprintf(stderr,"GRIP_REQUEST %d x %d accepted=%d\n",r.getWidth(),r.getHeight(),int(accepted));
            }
        });
        if(name&&*name=="ValueReadout")return new ValueReadout(getController(),preferences);
        if(name&&*name=="ChopperPreview")return new Preview(VSTGUI::CRect(24,72,936,210),getController(),preferences);
        return AspectRatioVST3Editor::createView(attrs,desc);
    }
private:
    void syncPreferences(){
        Steinberg::IPtr<Steinberg::IPlugView> keep(this);
        if(!opened)return;
        if(helpButton)helpButton->invalid();
        if(helpOverlay){
            if(preferences.help&&hovered){
                if(auto* control=dynamic_cast<VSTGUI::CControl*>(hovered))
                    helpOverlay->show(control->getTag(),hovered->getViewSize());
            }else helpOverlay->hide();
        }
        if(syncing||getZoomFactor()==preferences.zoom)return;
        syncTargetZoom=preferences.zoom;
        const auto pixels=editorPixels(syncTargetZoom,getContentScaleFactor());
        // A peer view may be denied by its own host. Do not overwrite the
        // shared desired zoom or claim that the rejected geometry applied.
        syncing=true;
        if(rect.getWidth()==pixels.width&&rect.getHeight()==pixels.height){
            // VST3 permits the host to omit onSize for unchanged dimensions.
            // Commit logical zoom and transform locally via the same
            // exact-size acknowledgement path, without notifying the host.
            Steinberg::ViewRect same=rect;
            onSize(&same);
        }else{
            requestResize(VSTGUI::CPoint(pixels.width,pixels.height));
        }
        syncing=false;
    }
    EditorPreferences& preferences;
    HelpOverlay* helpOverlay=nullptr;
    HelpButton* helpButton=nullptr;
    VSTGUI::CView* hovered=nullptr;
    bool opened=false,resizing=false,syncing=false;
    bool dpiInFlight=false,dpiAcknowledged=false;
    double syncTargetZoom=1.;
};
}
