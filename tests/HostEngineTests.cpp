#include "HostEngine.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>
using namespace chopper;
void check(bool yes,const char* name){if(!yes){std::cerr<<name<<'\n';std::exit(1);}}
int main(){
    HostEngine h,ref;Parameters p;
    p.set(Param::start,.5);p.set(Param::mid,.5);p.set(Param::end,.5);
    p.set(Param::attack,0);p.set(Param::release,0);p.set(Param::active,1);
    h.restore(p);ref.restore(p);
    check(h.save().active(),"stopped pending restore");
    check(h.prepare(48000)&&ref.prepare(48000),"prepare");
    for(int i=0;i<20000;++i){
        if(i==8000)h.set(Param::bypass,1);
        if(i==10000)h.set(Param::bypass,0);
        const double a=h.tick(),b=ref.tick();
        if(i>=8024&&i<10000)check(a==1,"settled bypass");
        if(i>=10024)check(a==b,"bypass keeps wet phase advancing");
    }
    h.publish();check(!h.save().bypass(),"audio changes saved");
    p.set(Param::active,0);h.restore(p);
    check(!h.save().active(),"new pending restore wins old audio state");
    h.adopt();for(int i=0;i<25;++i)h.tick();h.publish();
    check(!h.save().active()&&h.tick()==1,"restored inactive dry");
    // Concurrent one-writer/one-reader: every complete tuple agrees.
    struct Tuple{std::uint64_t a{},b{},c{};};
    Latest<Tuple> box;std::atomic<bool> finished{false};
    std::thread writer([&]{for(std::uint64_t n=1;n<=500000;++n)box.publish({n,n*3,n*7});finished.store(true,std::memory_order_release);});
    std::uint64_t last=0;
    do{const auto x=box.read();check(x.a>=last&&x.b==x.a*3&&x.c==x.a*7,"coherent monotonic snapshot");last=x.a;}while(!finished.load(std::memory_order_acquire));
    writer.join();check(box.read().a==500000,"latest snapshot");
    std::cout<<"HostEngine bypass, pending recall and coherent snapshot PASS\n";
}
