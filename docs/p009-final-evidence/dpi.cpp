#include <algorithm>
#include <cmath>
#include <iostream>

struct Rect {
    int right=0,bottom=0;
    int getWidth() const {return right;}
    int getHeight() const {return bottom;}
};
struct Point {double x,y;};

struct GeometryProbe {
    double scale,viewZoom;
    double desired;
    bool syncing=false,opened=true;
    Rect actual;
    int requested=0;
    bool accepted=false;

    // Use equal inputs and the guards copied verbatim from the candidate
    // at the call boundary. The HWND and SDK lifetime are not simulated.
    double getZoomFactor() const {return viewZoom;}
    double getContentScaleFactor() const {return scale;}
    bool requestResize(Point p);
    bool resizeWithPreviousGuard(Rect* r);

#include "dpi-sync.inc"
};

bool GeometryProbe::requestResize(Point p) {
    Rect size{static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y))};
    auto* r=&size;
    requested=r->getWidth();
    const auto oldZoom=getZoomFactor();
#include "dpi-guard.inc"
    viewZoom=factor;
    actual=*r;
    accepted=true;
    return true;
}

bool GeometryProbe::resizeWithPreviousGuard(Rect* r) {
#include "old-dpi-guard.inc"
    viewZoom=factor;
    actual=*r;
    return true;
}

int main() {
    const float dpiScale=1.28f;
    const Point sdkRequest{960.*.75*dpiScale,576.*.75*dpiScale};
    Rect sdkRect{static_cast<int>(std::floor(sdkRequest.x)),static_cast<int>(std::floor(sdkRequest.y))};
    GeometryProbe oldProbe{dpiScale,.75,.75,true,true,{},0,false};
    GeometryProbe currentProbe=oldProbe;
    const auto oldAccepted=oldProbe.resizeWithPreviousGuard(&sdkRect);
    const auto currentAccepted=currentProbe.requestResize(sdkRequest);
    std::cout<<"N02 75% at 1.28 DPI request="<<sdkRect.right
             <<" oldAccepted="<<oldAccepted<<" currentAccepted="<<currentAccepted
             <<" currentZoom="<<currentProbe.viewZoom<<'\n';
    if(sdkRect.right!=921||oldAccepted||!currentAccepted||currentProbe.viewZoom!=.75)return 1;

    for(float dpi:{1.f,1.28f,1.37f}) {
        GeometryProbe probe{dpi,1.,1.5,false,true,{},0,false};
        probe.actual={static_cast<int>(std::floor(960.*dpi)),static_cast<int>(std::floor(576.*dpi))};
        const auto max=static_cast<int>(std::floor(1440.*dpi));
        probe.sync();
        std::cout<<"DPI="<<dpi<<" recall=1.5 request="<<probe.requested
                 <<" allowedMax="<<max<<" accepted="<<probe.accepted
                 <<" actualWidth="<<probe.actual.right<<'\n';
        if(dpi==1.37f){
            if(probe.requested!=1973||max!=1972||probe.accepted||probe.actual.right!=1315)return 2;
        }else if(!probe.accepted||probe.actual.right!=max)return 2;
    }
}
