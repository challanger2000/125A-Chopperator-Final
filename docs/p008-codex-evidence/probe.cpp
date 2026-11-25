#include "processor.inc"
#include "ValueEntry.hpp"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace chopper;
int failures=0;
void check(const char* name,bool ok) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    failures+=!ok;
}
void require(bool ok) {
    if(!ok) { std::cerr<<"fixture setup failed\n"; std::exit(2); }
}
void point(ParameterChanges& p,ParamID id,int frame,double val) {
    int32 q=0,k=0;
    require(p.addParameterData(id,q)->addPoint(frame,val,k)==kResultOk);
}
struct Host {
    Processor proc;
    int32 channels;
    int32 format;
    double rate;
    Host(double sr=48000,int32 ch=2,int32 fmt=kSample64):channels(ch),format(fmt),rate(sr) {
        require(proc.initialize(nullptr)==kResultOk);
        auto bus=ch==1?SpeakerArr::kMono:SpeakerArr::kStereo;
        require(proc.setBusArrangements(&bus,1,&bus,1)==kResultOk);
        require(proc.activateBus(kAudio,kInput,0,true)==kResultOk);
        require(proc.activateBus(kAudio,kOutput,0,true)==kResultOk);
        activate();
    }
    ~Host() { stop(); proc.terminate(); }
    void activate() {
        ProcessSetup s{kRealtime,format,512,rate};
        require(proc.setupProcessing(s)==kResultOk);
        require(proc.setActive(true)==kResultOk);
        require(proc.setProcessing(true)==kResultOk);
    }
    void stop() {
        require(proc.setProcessing(false)==kResultOk);
        require(proc.setActive(false)==kResultOk);
    }
    void flush(ParameterChanges& points) {
        ProcessData d; d.symbolicSampleSize=format; d.inputParameterChanges=&points;
        require(proc.process(d)==kResultOk);
    }
    std::vector<char> save() {
        MemoryStream s;
        require(proc.getState(&s)==kResultOk && s.getSize()==120);
        return {s.getData(),s.getData()+s.getSize()};
    }
};
struct Block {
    double l[512]{},r[512]{},ol[512]{},orr[512]{};
    double* in[2]{l,r}; double* out[2]{ol,orr};
    AudioBusBuffers a{},b{};
    ProcessData d{};
    explicit Block(int n=512,int channels=2) {
        std::fill_n(l,512,1.);std::fill_n(r,512,-.375);
        a.numChannels=b.numChannels=channels;
        a.channelBuffers64=in;b.channelBuffers64=out;
        d.symbolicSampleSize=kSample64;d.numSamples=n;
        d.numInputs=d.numOutputs=1;d.inputs=&a;d.outputs=&b;
    }
    void run(Host& h) {require(h.proc.process(d)==kResultOk);}
};
ProcessContext clock(double bpm,int n,int d) {
    ProcessContext c{};
    c.state=ProcessContext::kTempoValid|ProcessContext::kTimeSigValid;
    c.tempo=bpm;c.timeSigNumerator=n;c.timeSigDenominator=d;
    return c;
}
void set(Host& h,int shape=0,double mix=1,double width=.5,bool loop=true) {
    ParameterChanges p(14);
    for(auto i:{100u,101u,102u})point(p,i,0,4./6);
    point(p,103,0,0.);point(p,106,0,shape*.25);point(p,107,0,width);
    point(p,108,0,0);point(p,109,0,0);point(p,110,0,mix);
    point(p,111,0,1);point(p,112,0,loop?1:0); h.flush(p);
}
void clockTests() {
    Host baseline,wrong,host;for(auto* h:{&baseline,&wrong,&host})set(*h,1,1,1);
    auto start=clock(120,4,4),isolated=clock(240,4,4),mixed=clock(240,0,4);
    isolated.state=ProcessContext::kTempoValid;
    Block a(512),b(512),c(512);
    a.d.processContext=b.d.processContext=c.d.processContext=&start;
    a.run(baseline);b.run(wrong);c.run(host);
    a.d.processContext=&isolated;b.d.processContext=nullptr;c.d.processContext=&mixed;
    a.run(baseline);b.run(wrong);c.run(host);
    double oracle=(512*8+511*16)/48000.;
    std::cout<<"clock sample 511 expected "<<oracle<<" positive "<<a.ol[511]<<" held "<<b.ol[511]<<" mixed "<<c.ol[511]<<'\n';
    check("tempo-only positive and 120 BPM counterfactual",std::abs(a.ol[511]-oracle)<1e-11 && std::abs(a.ol[511]-b.ol[511])>.08);
    check("valid tempo survives bad meter",std::abs(c.ol[511]-oracle)<1e-11);
}
void automation(bool wrong) {
    double maxError=0;
    for(bool split:{false,true}) {
        Host h;set(h,0,0,0);
        Block b(512);
        for(int i=0;i<512;i+=split?128:512)b.run(h);
        for(int offset=0;offset<256;offset+=split?64:256) {
            ParameterChanges p(14);int size=split?64:256;
            point(p,110,size-1,double(offset+size)/256);
            b.d.numSamples=size;b.d.inputParameterChanges=&p;b.run(h);
            for(int i=0;i<size;++i) {
                const double expected=1-double(offset+i+1)/(wrong?512:256);
                maxError=std::max(maxError,std::abs(b.ol[i]-expected));
                require(b.orr[i]==-.375*b.ol[i]);
            }
            b.d.inputParameterChanges=nullptr;
        }
    }
    std::cout<<"MIX ramp max error "<<maxError<<'\n';
    check("analytic MIX at 256 vs 4x64",maxError<1e-13);
}
void controls() {
    double x=0;
    const bool parsed=parseEntry(Param::length,"64 Bars",x)&&x==1
        &&parseEntry(Param::position,"75,5%",x)&&x==74.5/98
        &&parseEntry(Param::start,"1/64",x)&&x==1
        &&parseEntry(Param::curve,"-25%",x)&&x==.375;
    check("parser canonical/boundaries",parsed);
    bool rejected=true;
    for(auto v:{"NaN","Infinity","101%","1e1","10junk","10,1.2"})
        rejected &= !parseEntry(Param::mix,v,x);
    check("parser rejects invalid percent",rejected);
    check("parser rejects double sign",!parseEntry(Param::curve,"+-25%",x));
    std::cout<<"double sign parsed normalized "<<x<<'\n';
}
class ErrorStream : public MemoryStream {
public:
    using MemoryStream::MemoryStream;
    tresult PLUGIN_API read(void* target,int32 bytes,int32* actual) override {
        MemoryStream::read(target,bytes,actual); return kResultFalse;
    }
    tresult PLUGIN_API write(void* target,int32 bytes,int32* actual) override {
        MemoryStream::write(target,bytes,actual); return kResultFalse;
    }
};
void states() {
    Host h, donor;
    set(donor,4);
    auto wanted=donor.save(),before=h.save();
    unsigned rejected=0;
    for(int len=0;len<120;++len){
        MemoryStream s(wanted.data(),len);
        rejected+=(h.proc.setState(&s)!=kResultOk && h.save()==before);
    }
    check("truncated state 120/120 preserves state",rejected==120);
    ErrorStream failedRead(wanted.data(),static_cast<TSize>(wanted.size()));
    auto readStatus=h.proc.setState(&failedRead);
    auto after=h.save();
    ErrorStream failedWrite;
    auto writeStatus=h.proc.getState(&failedWrite);
    std::cout<<"stream read status="<<readStatus<<" restored="<<(after==wanted)
             <<" write status="<<writeStatus<<" reported bytes="<<failedWrite.getSize()<<'\n';
    check("failing stream read preserves state",readStatus!=kResultOk&&after==before);
    check("failing stream write reports error",writeStatus!=kResultOk);
    MemoryStream s(wanted.data(),static_cast<TSize>(wanted.size()));
    check("positive restore and immediate save",h.proc.setState(&s)==kResultOk&&h.save()==wanted);
    h.stop();h.activate();
    donor.stop();donor.activate();
    Block a,b;a.run(h);b.run(donor);
    check("fresh restore exact positive audio",std::equal(a.ol,a.ol+512,b.ol)&&a.ol[25]<.01);
}
void lifecycle() {
    Host h, ref;set(h,1,1,1);set(ref,1,1,1);
    Block a,b;auto last=clock(240,3,4);
    a.d.processContext=&last;a.run(h);b.run(ref);
    h.stop();h.activate();ref.stop();ref.activate();
    a.d.processContext=nullptr;a.run(h);b.run(ref);
    std::cout<<"reconfigured instance without clock sample 511="<<a.ol[511]
             <<" expected fresh default="<<b.ol[511]<<'\n';
    check("new activation clock fallback fresh default (policy probe)",a.ol[511]==b.ol[511]);
}
void pcm() {
    bool valid=true;double err=0;
    for(double sr:{44100.,48000.,96000.,192000.}) {
        for(int shape=0;shape<5;++shape) {
            Host h(sr); set(h,shape,1,1);
            Block b;
            for(int pos=0;pos<int(sr/4);) {
                int n=std::min(int(sr/4)-pos,17+pos%307);b.d.numSamples=n;b.run(h);
                for(int i=0;i<n;++i) {
                    const int j=pos+i;
                    const double phase=std::fmod(j*8/sr,1.);
                    double expected=1.;
                    if(shape==1)expected=phase;
                    if(shape==2)expected=1-phase;
                    if(shape==3)expected=2*std::min(phase,1-phase);
                    if(shape==4)expected=std::pow(std::sin(std::numbers::pi*phase),2);
                    if(phase>.04 && phase<.96)err=std::max(err,std::abs(b.ol[i]-expected));
                    valid &= std::isfinite(b.ol[i]) && b.ol[i]>=0 && b.ol[i]<=1 && b.orr[i]==-.375*b.ol[i];
                }
                pos+=n;
            }
        }
    }
    std::cout<<"20 waveform combinations max error="<<err<<'\n';
    check("five shape and stereo bounded sample-rate matrix",valid&&err<1e-10);
    Host dry; Block b(5);b.out[0]=b.in[0];b.out[1]=b.in[1];
    b.l[0]=std::numeric_limits<double>::max();b.l[1]=std::numeric_limits<double>::quiet_NaN();
    b.l[2]=std::numeric_limits<double>::infinity();b.l[3]=std::numeric_limits<double>::denorm_min();b.l[4]=-0.;
    std::fill_n(b.r,5,0.); b.run(dry);
    check("64-bit in-place nonfinite/silence/recovery",b.l[0]==std::numeric_limits<double>::max()&&b.l[1]==0&&b.l[2]==0&&b.l[3]==std::numeric_limits<double>::denorm_min()&&std::signbit(b.l[4])&&b.b.silenceFlags==2u);
}
void programme() {
    for(auto name:{"guitar_e2","guitar_b3","pad_c4"}) {
        std::ifstream file(std::string("programme/")+name+".f64le",std::ios::binary);
        for(int shape:{0,4}) {
            file.clear();file.seekg(0);
            Host h;set(h,shape,1,shape==0?.5:1);
            Block b;
            long double energy=0,error=0,dry=0,wet=0;
            for(int offset=0;offset<192000;) {
                int n=std::min(251,192000-offset);b.d.numSamples=n;
                for(int i=0;i<n;++i) {
                    file.read(reinterpret_cast<char*>(&b.l[i]),8);file.read(reinterpret_cast<char*>(&b.r[i]),8);
                }
                require(bool(file)); b.run(h);
                for(int i=0;i<n;++i) {
                    const int frame=offset+i,p=frame%6000;
                    const double g=shape==0?(p<3000?1.:0.):std::pow(std::sin(std::numbers::pi*p/6000),2);
                    for(int ch=0;ch<2;++ch) {
                        const double x=ch?b.r[i]:b.l[i],y=ch?b.orr[i]:b.ol[i];
                        require(std::isfinite(y) && std::abs(y)<=std::abs(x)+1e-14);
                        dry+=x*x;wet+=y*y;
                        if(frame>=6000 && (shape==4 || (p>=32&&p<2999) || p>=3032)) {
                            const double expected=x*g;
                            energy+=expected*expected;error+=(y-expected)*(y-expected);
                        }
                    }
                }
                offset+=n;
            }
            std::cout<<name<<" shape="<<shape<<" residual="<<std::sqrt(error/energy)<<" wet dB="<<10*std::log10(wet/dry)<<'\n';
            check("programme analytically matched stereo",energy>0.1L && std::sqrt(error/energy)<1e-9L && wet/dry<.9L);
        }
    }
}
int main(int argc,char** argv) {
    std::cout<<std::setprecision(14);
    const std::string command=argc>1?argv[1]:"all";
    if(command=="negative")automation(true);
    else {automation(false);clockTests();controls();states();lifecycle();pcm();programme();}
    return failures?1:0;
}
