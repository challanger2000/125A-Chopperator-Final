#include "controller.inc"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <iostream>

using namespace Steinberg;
using namespace Steinberg::Vst;

int main() {
    HostApplication host;
    chopper::Processor engine;
    chopper::Controller controller;
    if(controller.initialize(&host)!=kResultOk)return 1;
    auto view=owned(controller.createView(ViewType::kEditor));
    if(!view)return 2;
    auto& prefs=static_cast<chopper::Editor&>(*view.get()).prefs;
    std::size_t notifications=0;
    std::uint64_t observedEpoch=0;
    double observedWidth=0;
    prefs.watches.push_back({&notifications,[&]{
        ++notifications;
        observedEpoch=prefs.recallEpoch;
        observedWidth=controller.getParamNormalized(107);
    }});
    if(controller.setParamNormalized(107,.2)!=kResultOk||
        notifications!=1||observedEpoch!=0||observedWidth!=.2)return 3;

    // Set only WIDTH in the genuine 120-byte v1 payload. Its zero-based
    // index is 7, following the two 4-byte header fields.
    MemoryStream state;
    if(engine.getState(&state)!=kResultOk||state.getSize()!=120)return 4;
    std::array<char,120> loaded{};
    std::memcpy(loaded.data(),state.getData(),loaded.size());
    const auto width=std::bit_cast<std::uint64_t>(.8);
    for(std::size_t i=0;i<8;++i)loaded[8+7*8+i]=static_cast<char>(width>>(8*i));
    for(const auto size:{119,120}){
        MemoryStream stream(loaded.data(),size);
        const auto result=controller.setComponentState(&stream);
        std::cout<<"N03 state bytes="<<size<<" accepted="<<(result==kResultOk)
                 <<" notifications="<<notifications<<" observedEpoch="<<observedEpoch
                 <<" observedWidth="<<observedWidth<<'\n';
        if(size==119){
            if(result==kResultOk||prefs.recallEpoch!=0||notifications!=1||
               controller.getParamNormalized(107)!=.2)return 5;
        }else if(result!=kResultOk||prefs.recallEpoch!=1||notifications!=2||
                 observedEpoch!=1||observedWidth!=.8)return 6;
    }
    return controller.terminate()==kResultOk?0:7;
}
