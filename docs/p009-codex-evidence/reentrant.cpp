#include <algorithm>
#include <functional>
#include <vector>
#include "preferences.inc"
#include <iostream>

int main(int argc,char**) {
    EditorPreferences prefs;
    int first=0,second=0;
    prefs.uiWatches.push_back({&first,[&]{
        ++first;
        if(argc>1)prefs.remove(&first);
    }});
    prefs.uiWatches.push_back({&second,[&]{++second;}});
    try {
        prefs.restore(1.125,true);
    } catch(const std::bad_function_call&) {
        std::cout<<"FAIL synchronous detach during notification: bad_function_call\n";
        return 1;
    }
    std::cout<<"watch calls="<<first<<","<<second<<" watchers="<<prefs.uiWatches.size()<<'\n';
    return first==1&&second==1 ? 0 : 1;
}
