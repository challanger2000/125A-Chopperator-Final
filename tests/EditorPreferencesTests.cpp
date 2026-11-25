#include "EditorPreferences.hpp"
#include "EditorGeometry.hpp"
#include <cstdlib>
#include <iostream>
using chopper::EditorPreferences;
static void check(bool ok,const char* what){if(!ok){std::cerr<<what<<'\n';std::exit(1);}}
int main(){
    for(bool ui:{false,true}){
        EditorPreferences p;auto& r=ui?p.uiWatches:p.watches;
        auto dispatch=[&]{if(ui)p.notifyUI();else p.notify();};
        int a=0,b=0,c=0;
        r.push_back({&a,[&]{++a;p.remove(&a);}});
        r.push_back({&b,[&]{++b;}});
        dispatch();check(a==1&&b==1&&r.size()==1,"self detach invalidates callback");
        dispatch();check(a==1&&b==2,"removed self invoked again");
        p.remove(&b);
        r.push_back({&a,[&]{++a;p.remove(&b);r.push_back({&c,[&]{++c;}});}});
        r.push_back({&b,[&]{++b;}});
        dispatch();check(a==2&&b==2&&c==0,"later detach or newly registered entry invoked");
        p.remove(&a);dispatch();check(c==1,"new entry absent next dispatch");
        p.remove(&c);
        r.push_back({&a,[&]{++a;p.remove(&a);dispatch();}});
        r.push_back({&b,[&]{++b;p.remove(&b);}});
        dispatch();check(a==3&&b==3&&r.size()==0,"nested dispatch invokes stale callback");
    }
    EditorPreferences p;int calls=0;int owner=0;
    p.uiWatches.push_back({&owner,[&]{++calls;}});
    p.markDirty=[&]{p.remove(&owner);};
    p.changeHelp(true);check(calls==0&&p.help,"dirty callback detach");
    using namespace chopper;
    const auto m=editorPixels(1.5,double(1.37f));
    check(m.width==1972&&m.height==1183,"M01 exact fractional recall pixels");
    check(!editorSizeValid(1973,1183,double(1.37f)),"M01 old rounded width rejected");
    for(float dpi:{.8f,1.f,1.125f,1.25f,1.28f,1.37f,1.5f,1.75f,2.f,3.f}){
        const double scale=dpi;
        for(int i=0;i<=750;++i){
            const double zoom=.75+i*.001;
            const auto sz=editorPixels(zoom,scale);
            check(editorSizeValid(sz.width,sz.height,scale),"canonical request rejected");
        }
        check(editorZoomForWidth(editorPixels(.75,scale).width,scale)==.75,"minimum zoom exact");
        check(editorZoomForWidth(editorPixels(1.5,scale).width,scale)==1.5,"maximum zoom exact");
    }
    std::cout<<"P009_REENTRANT_REGISTRY_AND_GEOMETRY_PASS\n";
}
