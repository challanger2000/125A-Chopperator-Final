#include "host-model.inc"

void samePixels(){
    EditorPreferences prefs;
    Fixture first(prefs),second(prefs);
    int dirty=0;
    prefs.markDirty=[&]{++dirty;};
    for(auto* view:{&first,&second}){
        view->host=[view](ViewRect& request){
            if(dimensions(*view,request.getWidth(),request.getHeight()))
                return kResultTrue; // This host omits onSize for unchanged pixels.
            return view->onSize(&request);
        };
    }
    constexpr double zoom=1.0008;
    prefs.restore(zoom,false);
    for(auto* view:{&first,&second}){
        require(dimensions(*view,960,576)&&view->requests==0,"no host request for same-size recall");
        require(view->getZoomFactor()==zoom&&view->frame.canvasScale==zoom,
                "same-size recall applies exact zoom and transform locally");
        require(view->setContentScaleFactor(2.f)==kResultTrue&&dimensions(*view,1921,1152),
                "next DPI reflects recalled zoom");
        require(view->setContentScaleFactor(1.f)==kResultTrue&&dimensions(*view,960,576),
                "return to same-pixel geometry");
    }
    const int before=first.requests+second.requests;
    prefs.restore(1.,false);
    require(first.requests+second.requests==before&&prefs.zoom==1.&&dirty==0,
            "inverse recall is local and does not mark project dirty");
    for(auto* view:{&first,&second}){
        require(view->getZoomFactor()==1.&&view->frame.canvasScale==1.,
                "inverse recall applies exact zoom and transform");
        require(view->setContentScaleFactor(2.f)==kResultTrue&&dimensions(*view,1920,1152),
                "inverse recall restores original size at 200% DPI");
        require(view->baseSizeChanges==0,"scaled acknowledgement does not use the SDK resize callback");
    }
    std::cout<<"M01: both views, no resize request on same-pixel recalls; "
                "at 200% DPI 1921x1152 then 1920x1152; dirty=0\n";
}

void immediateDpiDetach(bool acknowledgeFirst){
    EditorPreferences prefs;
    Fixture view(prefs),peer(prefs);
    echo(peer);
    bool parentPresent=true;
    int zoomCallsAtDetach=-1;
    view.host=[&](ViewRect& target){
        require(view.references==2&&!view.frame.inZoom&&view.frame.zoomCalls==0,
                "DPI negotiates outside frame scaling with retained view");
        if(acknowledgeFirst)
            require(view.onSize(&target)==kResultTrue&&dimensions(view,1315,789),
                    "host acknowledges before detaching");
        require(!view.frame.inZoom,"acknowledgement fully unwound before close");
        zoomCallsAtDetach=view.frame.zoomCalls;
        require(view.removed()==kResultTrue,"host removes view");
        require(!view.childPresent&&parentPresent&&view.closeCalls==1,
                "child already released while parent is still valid");
        parentPresent=false;
        require(view.onSize(&target)==kResultFalse,"late acknowledgement rejected after removal");
        // In the second case the host returns success despite having closed
        // the view. Even an earlier acknowledgement must not trigger rollback.
        return acknowledgeFirst?kResultTrue:kResultFalse;
    };
    require(view.setContentScaleFactor(1.37f)==kResultFalse,"outer DPI returns failure after detach");
    require(!view.childPresent&&!parentPresent&&!view.opened&&view.references==1&&
            view.closeCalls==1&&!view.closeDuringZoom&&view.frame.zoomCalls==zoomCallsAtDetach&&
            view.baseSizeChanges==0&&view.getContentScaleFactor()==1.f,
            "no second close, frame work, or leaked requested scale");
    prefs.restore(1.25,false);
    require(view.requests==1&&peer.childPresent&&dimensions(peer,1200,720),"peer survives and follows recall");
    std::cout<<"M03: child gone at removed() return; parent released inside callback; "
             <<"DPI rejected, no later frame work; prior acknowledgement="<<acknowledgeFirst<<'\n';
}

int main(){
    samePixels();
    immediateDpiDetach(false);
    immediateDpiDetach(true);
}
