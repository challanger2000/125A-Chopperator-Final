#include "EditorGeometry.hpp"
#include "EditorPreferences.hpp"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

#define PLUGIN_API
namespace Steinberg {
using tresult = int;
constexpr int kResultTrue=0, kResultFalse=1, kInvalidArgument=2;
struct ViewRect {
    int left,top,right,bottom;
    int getWidth() const {return right-left;}
    int getHeight() const {return bottom-top;}
};
struct IPlugView {int references=1;};
template<class T> struct IPtr {
    explicit IPtr(T* p):target(p){++target->references;}
    ~IPtr(){--target->references;}
    T* target;
};
}

namespace VSTGUI {
struct CRect {};
struct CPoint {double x,y;};
struct Control {
    virtual ~Control()=default;
    int getTag(){return 0;}
};
using CControl=Control;
struct Hint {
    void invalid(){}
    void show(int, CRect){}
    void hide(){}
};
struct Hover : Control {
    CRect getViewSize(){return {};}
};
class VST3Editor : public Steinberg::IPlugView {
public:
    using ViewRect=Steinberg::ViewRect;
    using ScaleFactor=float;
    struct Frame {
        VST3Editor& editor;
        int zoomCalls=0;
        bool inZoom=false;
        double canvasScale=1.;
        bool setZoom(double factor){
            ++zoomCalls;
            inZoom=true;
            const bool accepted=editor.beforeSizeChange({},{});
            if(accepted)canvasScale=factor;
            inZoom=false;
            return accepted;
        }
    };
    virtual ~VST3Editor()=default;
    Frame* getFrame(){return childPresent?&frame:nullptr;}
    double getZoomFactor() const {return zoom;}
    double getContentScaleFactor() const {return contentScaleFactor;}
    void setZoomFactor(double z){
        if(zoom==z)return;
        zoom=z;
        if(auto* f=getFrame())f->setZoom(zoom*contentScaleFactor);
    }
    bool requestResize(CPoint p){
        ++requests;
        ViewRect request{0,0,int(std::floor(p.x)),int(std::floor(p.y))};
        return host(request)==Steinberg::kResultTrue;
    }
    virtual void close(){
        ++closeCalls;
        if(frame.inZoom)closeDuringZoom=true;
        childPresent=false;
    }
    virtual Steinberg::tresult setContentScaleFactor(ScaleFactor)=0;
    virtual bool beforeSizeChange(const CRect&,const CRect&){return true;}
    virtual Steinberg::tresult onSize(ViewRect* r){rect=*r;return Steinberg::kResultTrue;}
    virtual Steinberg::tresult checkSizeConstraint(ViewRect*)=0;

    ViewRect rect{0,0,960,576};
    Frame frame{*this};
    int requests=0,closeCalls=0;
    bool childPresent=true,closeDuringZoom=false;
    std::function<Steinberg::tresult(ViewRect&)> host;

protected:
    double zoom=1.;
    ScaleFactor contentScaleFactor=1.f;
};
struct AspectRatioVST3Editor : VST3Editor {};
}

namespace chopper {
struct Fixture final : VSTGUI::AspectRatioVST3Editor {
    explicit Fixture(EditorPreferences& p):preferences(p){
        preferences.uiWatches.push_back({this,[this]{syncPreferences();}});
    }
    ~Fixture(){preferences.remove(this);}
    Steinberg::tresult removed(){
        close();
        return Steinberg::kResultTrue;
    }

#include "editor-methods.inc"

    EditorPreferences& preferences;
    VSTGUI::Hint* helpOverlay=nullptr;
    VSTGUI::Hint* helpButton=nullptr;
    VSTGUI::Hover* hovered=nullptr;
    bool opened=true,resizing=false,syncing=false;
    bool dpiInFlight=false,dpiAcknowledged=false,deferredClose=false;
    double syncTargetZoom=1.;
};
}

using namespace Steinberg;
using chopper::EditorPreferences;
using chopper::Fixture;

void require(bool ok,std::string_view what){
    if(ok)return;
    std::cerr<<"Fixture assertion failed: "<<what<<'\n';
    std::exit(1);
}

bool dimensions(const Fixture& view,int w,int h){
    return view.rect.getWidth()==w&&view.rect.getHeight()==h;
}

void echo(Fixture& view){
    view.host=[&view](ViewRect& r){return view.onSize(&r);};
}

void recallEndpoints(){
    EditorPreferences prefs;
    Fixture view(prefs),peer(prefs);
    echo(peer);
    int calls=0;
    view.host=[&](ViewRect& r){
        ++calls;
        if(calls==1)require(dimensions(view,960,576),"getSize is old until DPI onSize");
        const auto original=r;
        require(view.checkSizeConstraint(&r)==kResultTrue,"valid constrained size");
        require(original.getWidth()==r.getWidth()&&original.getHeight()==r.getHeight(),
                "constraint preserves canonical requested pixels");
        return view.onSize(&r);
    };
    require(view.setContentScaleFactor(1.37f)==kResultTrue&&dimensions(view,1315,789),
            "fractional DPI accepted");
    prefs.restore(1.5,false);
    require(dimensions(view,1972,1183)&&view.getZoomFactor()==1.5&&dimensions(peer,1440,864),
            "150% recall at fractional DPI");
    prefs.restore(.75,false);
    require(dimensions(view,986,591)&&view.getZoomFactor()==.75&&dimensions(peer,720,432),
            "75% recall at fractional DPI");
    prefs.restore(1.,false);
    require(dimensions(view,1315,789)&&view.getZoomFactor()==1.&&calls==4&&view.requests==4,
            "inverse recall and no nested host resize");
    std::cout<<"M01 canonical acknowledgements: 1315x789 -> 1972x1183 -> 986x591 -> 1315x789\n";
}

void dpiWithoutHostResize(){
    EditorPreferences prefs;
    Fixture opened(prefs);
    require(opened.setContentScaleFactor(1.0008f)==kResultTrue&&dimensions(opened,960,576)&&
            opened.frame.canvasScale==double(1.0008f)&&opened.requests==0,
            "same-size DPI applies transform without host resize");
    Fixture unattached(prefs);
    unattached.childPresent=false;
    unattached.opened=false;
    require(unattached.setContentScaleFactor(1.37f)==kResultTrue&&dimensions(unattached,1315,789)&&
            unattached.requests==0,"pre-attach DPI sets reported dimensions without host resize");
    require(unattached.setContentScaleFactor(0.f)==kInvalidArgument&&dimensions(unattached,1315,789),
            "invalid DPI leaves pre-attach geometry");
    std::cout<<"DPI without resize: equal-size transform applied; pre-attach getSize=1315x789\n";
}

void dpiFailureAndDetach(){
    EditorPreferences prefs;
    Fixture view(prefs),peer(prefs);
    echo(peer);
    view.host=[&](ViewRect& r){
        auto wrong=r;
        ++wrong.right;
        require(view.onSize(&wrong)==kResultFalse,"inexact DPI ack rejected");
        return kResultTrue;
    };
    require(view.setContentScaleFactor(1.37f)==kResultFalse&&view.getContentScaleFactor()==1.f&&
            view.frame.zoomCalls==0&&dimensions(view,960,576),"inexact ack leaves old size");
    view.host=[&](ViewRect& r){
        require(view.onSize(&r)==kResultTrue,"contradictory host initially acks");
        return kResultFalse;
    };
    require(view.setContentScaleFactor(1.37f)==kResultFalse&&dimensions(view,960,576)&&
            view.getContentScaleFactor()==1.f&&view.frame.canvasScale==1.,
            "ack then reject rolls back canonical starting size");
    const int before=view.frame.zoomCalls;
    bool childAfterRemoved=false;
    view.host=[&](ViewRect&){
        require(view.references==2&&view.frame.zoomCalls==before&&!view.frame.inZoom,
                "external DPI negotiation precedes SDK scaling and pins the view");
        require(view.removed()==kResultTrue,"synchronous removed returned success");
        childAfterRemoved=view.childPresent;
        return kResultFalse;
    };
    require(view.setContentScaleFactor(1.37f)==kResultFalse&&view.closeCalls==1&&
            !view.childPresent&&!view.closeDuringZoom,"deferred child close exactly once");
    prefs.restore(1.25,false);
    require(dimensions(peer,1200,720)&&view.frame.zoomCalls==before,"closed view skipped, peer updates");
    require(childAfterRemoved,"expect to detect live child at removed return");
    std::cout<<"M03 detach: child immediately after removed()="<<childAfterRemoved
             <<", child after DPI return="<<view.childPresent
             <<", peer width="<<peer.rect.getWidth()<<'\n';
}

void recallWithoutChangedPixels(){
    EditorPreferences prefs;
    Fixture noEcho(prefs),echoControl(prefs);
    echo(echoControl);
    int skipped=0;
    noEcho.host=[&](ViewRect& r){
        if(dimensions(noEcho,r.getWidth(),r.getHeight())){
            ++skipped;
            return kResultTrue;
        }
        return noEcho.onSize(&r);
    };
    constexpr double zoom=1.0008;
    const auto base=chopper::editorPixels(zoom,1.);
    const auto scaled=chopper::editorPixels(zoom,2.);
    require(base.width==960&&base.height==576&&scaled.width==1921&&scaled.height==1152,
            "fractional fixture identity and distinct high-DPI geometry");
    prefs.restore(zoom,false);
    require(prefs.zoom==zoom&&skipped==1&&noEcho.getZoomFactor()==1.&&echoControl.getZoomFactor()==zoom,
            "same-size ack omission leaves stale active zoom");
    require(noEcho.setContentScaleFactor(2.f)==kResultTrue&&echoControl.setContentScaleFactor(2.f)==kResultTrue,
            "both later DPI changes accepted");
    require(dimensions(noEcho,1920,1152)&&dimensions(echoControl,1921,1152),
            "stale zoom leaks to subsequent pixel size");
    std::cout<<"M01 same-size recall: saved="<<prefs.zoom<<", no-echo width after 200% DPI="
             <<noEcho.rect.getWidth()<<", echo width="<<echoControl.rect.getWidth()<<'\n';
}

int main(){
    recallEndpoints();
    dpiWithoutHostResize();
    dpiFailureAndDetach();
    recallWithoutChangedPixels();
}
