#define main archived_p008_main
#include "../p008-codex-evidence/probe.cpp"
#undef main

class InjectedStream : public MemoryStream {
public:
    enum class Fault { status, count };
    InjectedStream(void* buffer,int size,int field,Fault fault)
    :MemoryStream(buffer,size),field_(field),fault_(fault){}
    InjectedStream(int field,Fault fault) :field_(field),fault_(fault){}
    tresult PLUGIN_API read(void* buffer,int32 n,int32* count) override {
        return inject(MemoryStream::read(buffer,n,count),count);
    }
    tresult PLUGIN_API write(void* buffer,int32 n,int32* count) override {
        return inject(MemoryStream::write(buffer,n,count),count);
    }
private:
    tresult inject(tresult result,int32* count) {
        if(call_++!=field_)return result;
        require(result==kResultOk && count && *count>0);
        if(fault_==Fault::status)return kResultFalse;
        --*count;
        return kResultOk;
    }
    int field_,call_=0;
    Fault fault_;
};

void wireAndRollback() {
    Host h;
    const auto initial=h.save();
    auto wanted=initial;
    auto put=[&](std::size_t offset,std::uint64_t bits,int bytes){
        for(int i=0;i<bytes;++i) wanted[offset+i]=static_cast<char>(bits>>(8*i));
    };
    put(0,0x31504843u,4);
    put(4,1,4);
    put(64,std::bit_cast<std::uint64_t>(.2197265625),8);
    put(96,std::bit_cast<std::uint64_t>(1.),8);
    MemoryStream positive(wanted.data(),120);
    check("C02 valid v1 bytes round trip exactly",
          h.proc.setState(&positive)==kResultOk && h.save()==wanted);
    for(auto fault:{InjectedStream::Fault::status,InjectedStream::Fault::count}){
        int reads=0,writes=0;
        for(int field=0;field<16;++field){
            InjectedStream read(const_cast<char*>(initial.data()),120,field,fault);
            const bool rejected=h.proc.setState(&read)!=kResultOk;
            reads+=rejected && h.save()==wanted;
            MemoryStream reseed(wanted.data(),120);
            require(h.proc.setState(&reseed)==kResultOk);
            InjectedStream write(field,fault);
            writes+=h.proc.getState(&write)!=kResultOk;
        }
        bool status=fault==InjectedStream::Fault::status;
        std::cout<<"C02 "<<(status?"full-count failure":"successful short count")<<" reads="<<reads
                 <<"/16 writes="<<writes<<"/16\n";
        check("C02 stream failures reject without state mutation",reads==16&&writes==16);
    }
}

template<class Sample> void busMatrix(){
    int passed=0;
    constexpr int format=std::is_same_v<Sample,float>?kSample32:kSample64;
    for(int channels:{1,2})for(bool alias:{false,true})for(bool inActive:{false,true})for(bool outActive:{false,true}){
        Host h(48000,channels,format);
        h.stop();
        require(h.proc.activateBus(kAudio,kInput,0,inActive)==kResultOk);
        require(h.proc.activateBus(kAudio,kOutput,0,outActive)==kResultOk);
        h.activate();
        Sample l[]{.75,.875},r[]{-.125,-.5},ol[]{9,9},orr[]{-9,-9};
        Sample* in[]{l,r};Sample* out[]{alias?l:ol,alias?r:orr};
        AudioBusBuffers a,b;a.numChannels=b.numChannels=channels;b.silenceFlags=8;
        if constexpr(format==kSample32){a.channelBuffers32=in;b.channelBuffers32=out;}
        else {a.channelBuffers64=in;b.channelBuffers64=out;}
        ProcessData d;d.symbolicSampleSize=format;d.numSamples=2;
        d.numInputs=d.numOutputs=1;d.inputs=&a;d.outputs=&b;
        require(h.proc.process(d)==kResultOk);
        const auto flags=!outActive?8u:!inActive?(channels==1?1u:3u):0u;
        bool correct=b.silenceFlags==flags;
        for(int ch=0;ch<channels;++ch){
            for(int i=0;i<2;++i){
                const Sample original=ch?(i?-.5:-.125):(i?.875:.75);
                const Sample expected=outActive?(inActive?original:0):(alias?original:(ch?-9:9));
                correct &= out[ch][i]==expected;
            }
        }
        passed+=correct;
    }
    std::cout<<"C03 format "<<format<<" cell count="<<passed<<"/16\n";
    check("C03 mono/stereo in-place/out-of-place activation matrix",passed==16);
}

void ratesAndParser(){
    double parsed=.42;
    check("C07 doubled signs denied with rollback",
        !parseEntry(Param::curve,"+-25%",parsed)&&parsed==.42&&
        !parseEntry(Param::curve,"++25%",parsed)&&parsed==.42);
    check("C07 signed controls accepted",
        parseEntry(Param::curve,"-25%",parsed)&&parsed==.375&&
        parseEntry(Param::curve,"+25%",parsed)&&parsed==.625);
    Core good,ref;
    good.setActive(true);ref.setActive(true);
    for(int i=0;i<113;++i){require(good.tick()==ref.tick());}
    const bool rejected=!good.prepare(std::numeric_limits<double>::max());
    bool equal=good.running()&&good.phase()==ref.phase()&&good.progress()==ref.progress();
    for(int i=0;i<5000;++i){equal &= good.tick()==ref.tick()&&good.phase()==ref.phase();}
    check("C08 extreme setup rejected with exact continuing phase and gain",rejected&&equal);
}

void retentionAndMeter(){
    Host reference,subject;
    set(reference,1,1,1);set(subject,1,1,1);
    auto hostClock=clock(240,3,4);Block ref,active;
    active.d.processContext=ref.d.processContext=&hostClock;
    ref.run(reference);active.run(subject);
    const double freshPhase=511.*16/48000;
    active.d.processContext=nullptr;
    subject.stop();subject.activate();active.run(subject);
    check("C09 per-instance clock persists, restarted phase",
          std::abs(active.ol[511]-freshPhase)<1e-11 && std::equal(active.ol,active.ol+512,ref.ol));
    for(int variant=0;variant<3;++variant) {
        Host h;set(h,0,1,0,false);
        auto patch=clock(120,3,4);
        if(variant==0)patch.state=ProcessContext::kTimeSigValid;
        if(variant==1)patch.state=0;
        if(variant==2)patch.tempo=std::numeric_limits<double>::quiet_NaN();
        Block b;double value=-1;
        for(int offset=0;offset<=18024;){
            int count=std::min(512,18025-offset);
            b.d.numSamples=count;b.d.processContext=offset==0?&patch:nullptr;
            b.run(h);value=b.ol[count-1];offset+=count;
        }
        std::cout<<"C01 meter variant "<<variant<<" output at sample 18024="<<value<<'\n';
        check("C01 meter-only and both flags vs. unchanged counterfactual",value==(variant==1?0.:1.));
    }
}

int main() {
    std::cout<<std::setprecision(14);
    clockTests();retentionAndMeter();wireAndRollback();
    busMatrix<float>();busMatrix<double>();ratesAndParser();
    automation(false);pcm();
    return failures?1:0;
}
