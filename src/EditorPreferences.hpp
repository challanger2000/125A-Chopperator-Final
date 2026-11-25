#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace chopper {
// GUI-thread registry only; no audio callback accesses. Snapshot owns
// each callback during invocation, and removal invalidates its token
// immediately, including callbacks not yet visited by an outer dispatch.
struct EditorPreferences {
    double zoom=1.; bool help=false;
    std::uint64_t recallEpoch=0;
    struct Watch { const void* owner; std::function<void()> update; bool registered=true; };
    class Registry {
        std::vector<std::shared_ptr<Watch>> entries;
    public:
        void push_back(Watch w){entries.push_back(std::make_shared<Watch>(std::move(w)));}
        std::size_t size()const noexcept{return entries.size();}
        void remove(const void* owner){
            for(auto& w:entries)if(w->owner==owner)w->registered=false;
            std::erase_if(entries,[owner](const auto& w){return w->owner==owner;});
        }
        void dispatch(){
            const auto snapshot=entries;
            for(const auto& w:snapshot)if(w->registered&&w->update)w->update();
        }
    };
    Registry watches,uiWatches;
    std::function<void()> markDirty;
    void notify(){watches.dispatch();}
    void notifyUI(){uiWatches.dispatch();}
    void remove(const void* owner){watches.remove(owner);uiWatches.remove(owner);}
    void restore(double z,bool h){zoom=z;help=h;notifyUI();}
    void changeZoom(double z){
        if(zoom==z)return;
        zoom=z;
        if(markDirty)markDirty();
        notifyUI();
    }
    void changeHelp(bool h){
        if(help==h)return;
        help=h;
        if(markDirty)markDirty();
        notifyUI();
    }
};
}
