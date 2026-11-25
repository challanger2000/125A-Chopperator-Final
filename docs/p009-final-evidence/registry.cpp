#include "preferences.hpp"
#include <functional>
#include <iostream>

int main() {
    chopper::EditorPreferences prefs;
    int first=0,second=0;
    prefs.uiWatches.push_back({&first,[&]{++first;}});
    prefs.uiWatches.push_back({&second,[&]{++second;}});
    prefs.restore(1.125,true);
    if(first!=1||second!=1)return 2;

    prefs.remove(&first);
    prefs.remove(&second);
    first=second=0;
    prefs.uiWatches.push_back({&first,[&]{
        ++first;
        prefs.remove(&first);
    }});
    prefs.uiWatches.push_back({&second,[&]{++second;}});
    try {
        prefs.restore(1.25,false);
    } catch (const std::bad_function_call&) {
        std::cout<<"N01 reproduced: bad_function_call\n";
        return 1;
    }
    if(first!=1||second!=1||prefs.uiWatches.size()!=1)return 2;
    prefs.restore(1.5,true);
    if(first!=1||second!=2)return 2;
    std::cout<<"N01 current detach: surviving callback invoked once per restore\n";
}
