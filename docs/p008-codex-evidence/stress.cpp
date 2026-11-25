#define main prior_probe_main
#include "probe.cpp"
#undef main
#include <atomic>
#include <latch>
#include <new>
thread_local bool countHeap=false;
thread_local unsigned allocations=0;
void* operator new(std::size_t n) {
    if(countHeap)++allocations;
    if(auto p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void busTest() {
    Host h;
    Block b(16);
    b.run(h);check("active stereo sentinel",b.ol[0]==1 && b.orr[0]==-.375);
    h.stop();require(h.proc.activateBus(kAudio,kInput,0,false)==kResultOk); h.activate();
    b.in[0]=nullptr;b.in[1]=nullptr;
    b.run(h);check("inactive input with null pointers",b.ol[0]==0 && b.orr[0]==0 && b.b.silenceFlags==3);
    b.in[0]=b.l;b.in[1]=b.r;b.run(h);
    std::cout<<"inactive input sentinel "<<b.ol[0]<<" "<<b.orr[0]<<" flags="<<b.b.silenceFlags<<'\n';
    check("inactive input with nonnull pointers renders silence",b.ol[0]==0&&b.orr[0]==0&&b.b.silenceFlags==3);
    h.stop();require(h.proc.activateBus(kAudio,kInput,0,true)==kResultOk);
    require(h.proc.activateBus(kAudio,kOutput,0,false)==kResultOk);h.activate();
    std::fill_n(b.ol,16,9.);std::fill_n(b.orr,16,9.);b.run(h);
    std::cout<<"inactive output sentinel "<<b.ol[0]<<" "<<b.orr[0]<<'\n';
    check("inactive output buffer untouched",b.ol[0]==9&&b.orr[0]==9);
}
void rateTests(){
    Core ordinary,extreme;
    require(ordinary.prepare(48000)&&extreme.prepare(std::numeric_limits<double>::max()));
    Settings s;s.width=0;require(ordinary.configure(s)&&extreme.configure(s));
    ordinary.setActive(true);extreme.setActive(true);
    for(int i=0;i<100;++i){ordinary.tick();extreme.tick();}
    std::cout<<"100 samples normal progress "<<ordinary.progress()<<" DBL_MAX progress "<<extreme.progress()<<'\n';
    check("normal clock progresses",ordinary.progress()>0);
    check("unrepresentable setup rejected or usable",extreme.progress()>0);
}
void concurrency(){
    constexpr int repeats=20000;
    Host a,b,actual;
    set(a,1,.875,.125);
    set(b,4,.625,.75);
    auto sa=a.save(),sb=b.save();
    auto write=[&](std::vector<char>& bytes) {
        MemoryStream s(bytes.data(),static_cast<TSize>(bytes.size()));
        require(actual.proc.setState(&s)==kResultOk);
    };
    write(sa);
    std::latch rendezvous(2);
    std::atomic<int> blocks{0};
    std::atomic<bool> stop{false};
    std::thread audio([&]{
        Block block(64);
        rendezvous.arrive_and_wait();
        do {
            block.run(actual);
            require(std::isfinite(block.ol[63])&&block.orr[63]==-.375*block.ol[63]);
            if(blocks.fetch_add(1)==0)blocks.notify_all();
        } while(!stop.load());
    });
    rendezvous.arrive_and_wait();
    blocks.wait(0);
    int errors=0;
    for(int i=0;i<repeats;++i){
        auto& requested=i%2?sa:sb;
        write(requested);
        errors+=actual.save()!=requested;
    }
    stop.store(true);
    audio.join();
    std::cout<<"recall/save request pairs "<<repeats<<" audio blocks "<<blocks<<" wrong="<<errors<<'\n';
    check("overlapped audio and 20k serialized host recall/saves",errors==0);
}
void deadlines() {
    using C=std::chrono::steady_clock;
    using Us=std::chrono::duration<double,std::micro>;
    std::vector<double> overhead(2000);
    for(auto& v:overhead){const auto begin=C::now();v=Us(C::now()-begin).count();}
    std::sort(overhead.begin(),overhead.end());
    std::cout<<"paired clock read median_us="<<overhead[1000]<<'\n';
    for(auto sr:{48000.,192000.})for(int n:{32,128,512}){
        Host h(sr);set(h,4,.875,.5);
        ParameterChanges init(14);point(init,100,0,0);point(init,102,0,0);point(init,105,0,1);h.flush(init);
        Block b(n);
        for(int i=0;i<n;++i){
            b.l[i]=.5*std::sin(i*.3)+.125;
            b.r[i]=-.375*b.l[i];
        }
        ParameterChanges points[2]{ParameterChanges(14),ParameterChanges(14)};
        for(int a=0;a<2;++a)for(int i=0;i<n;++i)for(auto id:{104u,105u,107u,108u,109u,110u})
            point(points[a],id,i,((i+a)%2?.875:.125));
        constexpr int callbacks=384;
        std::vector<double> times(callbacks);
        const auto period=std::chrono::duration_cast<C::duration>(std::chrono::duration<double>(n/sr));
        auto next=C::now();int missed=0,scheduled=0;
        double sum=0,peak=0;
        allocations=0;
        for(int k=0;k<callbacks;++k){
            std::this_thread::sleep_until(next);
            b.d.inputParameterChanges=&points[k%2];
            countHeap=true;
            const auto begin=C::now();
            auto status=h.proc.process(b.d);
            const auto end=C::now();
            countHeap=false;
            require(status==kResultOk);
            times[k]=Us(end-begin).count();sum+=times[k];
            missed+=end-begin>period;
            next+=period;scheduled+=end>next;
            for(int i=0;i<n;++i){
                require(std::isfinite(b.ol[i])&&std::abs(b.ol[i])<=std::abs(b.l[i])+1e-14);
                peak=std::max(peak,std::abs(b.l[i]-b.ol[i]));
            }
        }
        std::sort(times.begin(),times.end());
        std::cout<<sr<<"/"<<n<<" mean "<<sum/callbacks<<" p95 "<<times[364]<<" p99 "<<times[380]
                 <<" max "<<times.back()<<" deadline "<<Us(period).count()<<" duration misses "<<missed
                 <<" schedule misses "<<scheduled<<" new "<<allocations<<" mod delta "<<peak<<'\n';
        check("bounded PCM, C++ new calls and duration below deadline",missed==0&&allocations==0&&peak>.01);
    }
}
int main(){
    std::cout<<std::setprecision(14);
    busTest();rateTests();concurrency();deadlines();
    return failures?1:0;
}
