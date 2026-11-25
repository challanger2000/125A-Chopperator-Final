#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/base/ustring.h"
#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <poll.h>
#endif
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
void require(bool ok,const char* why){if(!ok){std::cerr<<"FAIL "<<why<<'\n';std::exit(1);}}
void point(ParameterChanges& changes,ParamID id,int32 offset,double value){
    int32 qi=0,pi=0;auto* q=changes.addParameterData(id,qi);
    require(q&&q->addPoint(offset,value,pi)==kResultOk,"host fixture queue");
}
struct Native {
    std::unique_ptr<PlugProvider> provider;
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    FUnknownPtr<IAudioProcessor> processor;
    ProcessContext clock{};
    bool running=false;int32 channels=2;double rate=48000;
    Native(const VST3::Hosting::Module::Ptr& module){
        for(const auto& info:module->getFactory().classInfos()){
            if(info.category()==kVstAudioEffectClass){
                require(info.name()=="125A Chopperator","factory product name");
                provider=std::make_unique<PlugProvider>(module->getFactory(),info,true);break;
            }
        }
        require(bool(provider)&&provider->initialize(),"native provider initialize");
        component=provider->getComponentPtr();controller=provider->getControllerPtr();
        processor=FUnknownPtr<IAudioProcessor>(component.get());
        require(component.get()&&controller.get()&&processor.getInterface(),"native interfaces");
        require(controller->getParameterCount()==14,"parameter inventory");
        require(processor->getLatencySamples()==0&&processor->getTailSamples()==0,"latency and tail");
    }
    void start(double sr,int32 fmt,int32 ch){
        rate=sr;channels=ch;auto arrangement=ch==1?SpeakerArr::kMono:SpeakerArr::kStereo;
        require(processor->setBusArrangements(&arrangement,1,&arrangement,1)==kResultOk,"bus setup");
        require(component->activateBus(kAudio,kInput,0,true)==kResultOk,"activate input");
        require(component->activateBus(kAudio,kOutput,0,true)==kResultOk,"activate output");
        ProcessSetup s{};s.processMode=kRealtime;s.symbolicSampleSize=fmt;s.maxSamplesPerBlock=512;s.sampleRate=sr;
        require(processor->setupProcessing(s)==kResultOk,"processing setup");
        require(component->setActive(true)==kResultOk,"setActive");
        require(processor->setProcessing(true)==kResultOk,"setProcessing");
        running=true;clock.state=ProcessContext::kPlaying|ProcessContext::kTempoValid|ProcessContext::kTimeSigValid;
        clock.tempo=120;clock.timeSigNumerator=4;clock.timeSigDenominator=4;clock.sampleRate=sr;
    }
    void stop(){
        if(running){require(processor->setProcessing(false)==kResultOk,"processing stop");
            require(component->setActive(false)==kResultOk,"deactivate");running=false;}
    }
    ~Native(){stop();}
    void flush(ParameterChanges& changes,int32 fmt=kSample32){
        ProcessData d;d.symbolicSampleSize=fmt;d.inputParameterChanges=&changes;
        require(processor->process(d)==kResultOk,"zero-frame automation");changes.clearQueue();
    }
    template<class S> std::vector<S> render(int32 n,ParameterChanges* params=nullptr,bool haveClock=true){
        std::vector<S> l(static_cast<std::size_t>(n),S{1}),r(static_cast<std::size_t>(n),S{-.5});
        std::vector<S> ol(static_cast<std::size_t>(n),S{9}),orr(static_cast<std::size_t>(n),S{9});
        S* in[]{l.data(),r.data()};S* out[]{ol.data(),orr.data()};
        AudioBusBuffers i,o;i.numChannels=o.numChannels=channels;
        ProcessData d;d.numInputs=d.numOutputs=1;d.inputs=&i;d.outputs=&o;d.numSamples=n;
        d.inputParameterChanges=params;d.processContext=haveClock?&clock:nullptr;
        if constexpr(std::is_same_v<S,float>){d.symbolicSampleSize=kSample32;i.channelBuffers32=in;o.channelBuffers32=out;}
        else{d.symbolicSampleSize=kSample64;i.channelBuffers64=in;o.channelBuffers64=out;}
        require(processor->process(d)==kResultOk,"native process");
        bool silent=true;
        for(int32 k=0;k<n;++k){
            require(std::isfinite(ol[k])&&ol[k]>=0&&ol[k]<=1,"finite bounded PCM");
            if(channels==2)require(orr[k]==S{-.5}*ol[k],"shared stereo gain");
            silent&=ol[k]==0;
        }
        require(o.silenceFlags==(silent?(channels==2?3u:1u):0u),"actual output silence flags");
        if(params)params->clearQueue();
        clock.projectTimeSamples+=1000000; // deliberately unrelated project position
        return ol;
    }
    std::vector<char> save(){
        MemoryStream stream;require(component->getState(&stream)==kResultOk,"component save");
        const auto n=stream.getSize();require(n==120,"state size");
        return std::vector<char>(stream.getData(),stream.getData()+n);
    }
    bool load(std::vector<char>& bytes){
        MemoryStream stream(bytes.data(),static_cast<TSize>(bytes.size()));
        return component->setState(&stream)==kResultOk;
    }
};
template<class S> void timing(const VST3::Hosting::Module::Ptr& module,double sr,int32 ch){
    Native n(module);const int32 fmt=std::is_same_v<S,float>?kSample32:kSample64;n.start(sr,fmt,ch);
    ParameterChanges p(14);
    point(p,107,0,0);point(p,108,0,0);point(p,109,0,0);n.flush(p,fmt);
    const int edge=static_cast<int>(sr*.0005);
    const int on=64,off=256;
    point(p,111,on,1);point(p,111,off,0);
    const auto pcm=n.render<S>(512,&p);
    for(int i=0;i<512;++i){
        const double expected=i<on?1:i<off?std::max(0.,1.-double(i-on+1)/edge):std::min(1.,double(i-off+1)/edge);
        require(std::abs(double(pcm[i])-expected)<1e-6,"sample-exact ACTIVE and de-click slope");
    }
    // Continuous host automation from the implicit previous point at -1.
    point(p,110,0,0);point(p,111,0,1);n.flush(p,fmt);
    n.render<S>(512);
    point(p,110,255,1);
    const auto ramp=n.render<S>(256,&p,false);
    for(int i=0;i<256;++i)require(std::abs(double(ramp[i])-(1-double(i+1)/256))<1e-6,"continuous MIX automation");
    // Actual compiled plugin returns dry in bypass with a warm oscillator.
    point(p,113,0,1);n.flush(p,fmt);
    const auto dry=n.render<S>(512);
    for(int i=edge;i<512;++i)require(dry[i]==S{1},"settled exact bypass");
}
std::vector<float> trajectory(Native& n){
    std::vector<float> pcm;
    for(int i=0;i<25024;){const int count=std::min(73+i%311,25024-i);auto a=n.render<float>(count,nullptr,i%3!=0);pcm.insert(pcm.end(),a.begin(),a.end());i+=count;}
    return pcm;
}

constexpr double pi=3.1415926535897932384626433832795;
struct SignalBlock {
    std::array<double,512> l{},r{},ol{},orr{};
    double* in[2]{l.data(),r.data()},*out[2]{ol.data(),orr.data()};
    AudioBusBuffers i,o;
    ProcessData data;
    SignalBlock(Native& n,int32 count){
        i.numChannels=o.numChannels=2;i.channelBuffers64=in;o.channelBuffers64=out;
        data.numInputs=data.numOutputs=1;data.inputs=&i;data.outputs=&o;
        data.numSamples=count;data.symbolicSampleSize=kSample64;data.processContext=&n.clock;
    }
    void process(Native& n){require(n.processor->process(data)==kResultOk,"quality native process");}
};
void setConstant(Native& n,int divisionIndex,int shapeIndex,double width,double attack,double release){
    ParameterChanges p(14);
    for(auto id:{100u,101u,102u})point(p,id,0,double(divisionIndex)/6);
    point(p,103,0,65./65);point(p,106,0,double(shapeIndex)/4);
    point(p,107,0,width);point(p,108,0,attack);point(p,109,0,release);
    point(p,110,0,1);point(p,111,0,1);point(p,112,0,1);
    n.flush(p,kSample64);
}
std::vector<double> tone(Native& n,double hz,int seconds=1){
    const int sr=static_cast<int>(n.rate),frames=sr*seconds;
    std::vector<double> result;result.reserve(frames);
    SignalBlock b(n,256);
    // Warm one second to exclude activation transient. All selected tones and
    // modulation frequencies are integer-Hz, so the window is coherent.
    for(int i=-sr;i<frames;){
        const int count=std::min(256,frames-i);b.data.numSamples=count;
        for(int k=0;k<count;++k){
            b.l[k]=.5*std::sin(2*pi*hz*double(i+k)/sr);
            b.r[k]=-.5*b.l[k];
        }
        b.process(n);
        for(int k=0;k<count;++k){
            require(std::isfinite(b.ol[k])&&std::abs(b.ol[k])<=std::abs(b.l[k])+1e-14,"no audio boost");
            require(b.orr[k]==-.5*b.ol[k],"tone stereo gain");
            if(i+k>=0)result.push_back(b.ol[k]);
        }
        i+=count;
    }
    require(result.size()==static_cast<std::size_t>(frames),"coherent capture");
    return result;
}
double amplitude(const std::vector<double>& x,double hz,double sr){
    double re=0,im=0;
    for(std::size_t k=0;k<x.size();++k){
        const double a=2*pi*hz*double(k)/sr;
        re+=x[k]*std::cos(a);im+=x[k]*std::sin(a);
    }
    return 2*std::hypot(re,im)/double(x.size());
}
void spectral(const VST3::Hosting::Module::Ptr& module,bool negative){
    Native n(module);n.start(48000,kSample64,2);
    // Sinusoidal AM: .5 input amplitude, g=.5-.5*cos(2*pi*8*t).
    // Analytic carrier .25, two sidebands .125, no other tones.
    setConstant(n,4,4,1,0,0);
    const auto x=tone(n,1000);
    const double c=amplitude(x,1000,48000),lo=amplitude(x,992,48000),hi=amplitude(x,1008,48000);
    double error=0;
    for(std::size_t i=0;i<x.size();++i){
        const double t=double(i)/48000;
        const double expected=.5*std::sin(2*pi*1000*t)*(.5-.5*std::cos(2*pi*8*t));
        error+=(x[i]-expected)*(x[i]-expected);
    }
    error=std::sqrt(error/double(x.size()));
    require(std::abs(c-(negative?.5:.25))<1e-6,"SINE carrier oracle");
    require(std::abs(lo-.125)<1e-6&&std::abs(hi-.125)<1e-6&&error<1e-6,"SINE sideband and residual oracle");
    std::cout<<"SINE_AM carrier="<<c<<" lower="<<lo<<" upper="<<hi<<" residual_rms="<<error<<"\n";
    n.stop();
    Native q(module);q.start(48000,kSample64,2);q.clock.tempo=240;
    setConstant(q,6,0,.5,0,0);
    const auto y=tone(q,23000);
    // Upper 17th 64-Hz sideband would be 24088 Hz, folds to 23912 Hz.
    // Reference for an ideal hard 50% duty pulse: coefficient 1/(17*pi).
    // For a 0.5 ms linear slew (32/1000 of period), multiply by sinc.
    const double hard=1/(17*pi);
    const double z=pi*17*.0005*64;
    const double expected=hard*std::abs(std::sin(z)/z);
    const double actual=amplitude(y,23912,48000)/.5;
    require(actual<hard&&std::abs(actual-expected)<.002,"RECT folded sideband vs trapezoid oracle");
    std::cout<<"RECT_64HZ_FOLDED_SIDEBAND frequency=23912 relative_dBc="<<20*std::log10(actual)
             <<" hard_dBc="<<20*std::log10(hard)<<" continuous_reference_dBc="<<20*std::log10(expected)<<"\n";
    // This is one isolated fold, not a full alias-power or audibility metric.
}
void waveformContracts(const VST3::Hosting::Module::Ptr& module){
    for(int shape=0;shape<5;++shape){
        Native n(module);n.start(48000,kSample64,2);
        setConstant(n,2,shape,1,0,0); // quarter notes, 2 Hz, width full
        std::vector<double> x;
        for(int i=0;i<48000;){int count=std::min(512,48000-i);auto a=n.render<double>(count);x.insert(x.end(),a.begin(),a.end());i+=count;}
        for(int k=24000+100;k<48000-100;++k){
            const double u=double(k-24000)/24000;
            double expected=0;
            switch(shape){case 0:expected=1;break;case 1:expected=u;break;case 2:expected=1-u;break;
                case 3:expected=1-std::abs(2*u-1);break;case 4:expected=.5-.5*std::cos(2*pi*u);break;}
            require(std::abs(x[k]-expected)<1e-7,"native waveform oracle");
        }
    }
    std::cout<<"NATIVE_ALL_FIVE_SHAPES_PASS\n";
}
struct PacedOracle {
    double position=.01,curve=1.,width=.5,phase=0.,gain=1.;
    std::size_t frame=0;
    double tick(std::size_t block,int sample){
        const double fraction=(sample+1)/128.;
        const double p=position+((block%2?.01:.99)-position)*fraction;
        const double c=curve+((block%2?0.:1.)-curve)*fraction;
        const double w=width+((block%2?.2:.9)-width)*fraction;
        const double target=phase<w?.5-.5*std::cos(2*pi*phase/w):0.;
        gain+=std::clamp(target-gain,-1./24,1./24);
        // Exact per-sample integral by a separately written primitive,
        // evaluated in long double; phase normalized to fractional cycles.
        const long double m=.01L+.98L*p,exponent=std::pow(2.L,4.L*c-2.L);
        auto primitive=[&](long double x){
            if(x<=m)return x+63*m*std::pow(x/m,exponent+1)/(exponent+1);
            const auto z=x-m;
            return m*(1+63/(exponent+1))+64*z-63*(1-m)*std::pow(z/(1-m),exponent+1)/(exponent+1);
        };
        const long double a=static_cast<long double>(frame)/6144000.L;
        const long double b=static_cast<long double>(frame+1)/6144000.L;
        phase=std::fmod(phase+static_cast<double>(64*(primitive(b)-primitive(a))),1.);
        ++frame;
        if(sample==127){position=block%2?.01:.99;curve=block%2?0.:1.;width=block%2?.2:.9;}
        return gain;
    }
};
void realtimeProbe(const VST3::Hosting::Module::Ptr& module){
    using Clock=std::chrono::steady_clock;
    Native n(module);n.start(48000,kSample64,2);
    setConstant(n,6,4,.5,0,0);
    SignalBlock b(n,128);
    for(int i=0;i<128;++i){b.l[i]=.25*std::sin(2*pi*440*i/48000);b.r[i]=-.5*b.l[i];}
    ParameterChanges p(14);
    point(p,100,0,0);point(p,101,0,1);point(p,102,0,0);
    point(p,104,0,.01);point(p,105,0,1);n.flush(p,kSample64);
    std::array<double,384> micros{};
    double inputEnergy=0,outputEnergy=0,differenceEnergy=0,oracleError=0,constantError=0;
    PacedOracle oracle;
    const auto interval=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(128./48000));
    auto deadline=Clock::now();double sum=0;int over=0;
    for(std::size_t i=0;i<micros.size();++i){
        // Automation setup is host work, intentionally outside callback measurement.
        p.clearQueue();point(p,104,127,i%2?.01:.99);point(p,105,127,i%2?0.:1.);
        point(p,107,127,i%2?.2:.9);b.data.inputParameterChanges=&p;
        const auto begin=Clock::now();
        const auto status=n.processor->process(b.data);
        const auto end=Clock::now();
        require(status==kResultOk,"paced native automation");
        const double us=std::chrono::duration<double,std::micro>(end-begin).count();
        micros[i]=us;sum+=us;over+=us>128./48000*1e6;
        for(int k=0;k<128;++k){
            require(std::isfinite(b.ol[k])&&std::abs(b.ol[k])<=std::abs(b.l[k])+1e-14&&
                    b.orr[k]==-.5*b.ol[k],"paced bounded stereo output");
            inputEnergy+=b.l[k]*b.l[k];outputEnergy+=b.ol[k]*b.ol[k];
            const double delta=b.ol[k]-b.l[k];differenceEnergy+=delta*delta;
            const double expected=b.l[k]*oracle.tick(i,k);
            oracleError+=(b.ol[k]-expected)*(b.ol[k]-expected);
            constantError+=(.5*b.l[k]-expected)*(.5*b.l[k]-expected);
        }
        deadline+=interval;std::this_thread::sleep_until(deadline);
    }
    std::sort(micros.begin(),micros.end());
    std::cout<<"PACED_NATIVE_MEASUREMENT callbacks="<<micros.size()<<" mean_us="<<sum/micros.size()
        <<" p95_us="<<micros[364]<<" p99_us="<<micros[380]<<" max_us="<<micros.back()
        <<" deadline_us="<<128./48000*1e6<<" callback_overruns="<<over<<"\n";
    require(inputEnergy>0&&outputEnergy/inputEnergy>.001&&differenceEnergy/inputEnergy>.001,
        "paced output neither silent nor unmodified");
    require(oracleError/inputEnergy<1e-10&&constantError/inputEnergy>.01,"paced temporal PCM oracle rejects constant gain");
    std::cout<<"PACED_TEMPORAL_ORACLE_PASS relative_squared_error="<<oracleError/inputEnergy
        <<" constant_gain_negative="<<constantError/inputEnergy<<"\n";
    std::cout<<"PACED_NATIVE_SIGNAL_PASS wet_energy_ratio="<<outputEnergy/inputEnergy
        <<" change_energy_ratio="<<differenceEnergy/inputEnergy<<"\n";
    // A measured overrun blocks this gate; attribution to scheduler or
    // processor requires separate evidence. Cloud runner is not ASIO.
    require(over==0,"paced native callback deadline exceeded");
}


void programme(const VST3::Hosting::Module::Ptr& module,const std::filesystem::path& folder){
    for(const std::string name:{"guitar_e2","guitar_b3","pad_c4"}){
        std::ifstream in(folder/(name+".f64le"),std::ios::binary|std::ios::ate);
        require(bool(in),"programme fixture open");
        const auto bytes=static_cast<std::streamoff>(in.tellg());
        require(bytes>=48000*16&&bytes%16==0,"programme size");
        const int frames=static_cast<int>(std::min<std::streamoff>(bytes/16,192000));
        std::vector<double> source(static_cast<std::size_t>(frames)*2);
        in.seekg(0);in.read(reinterpret_cast<char*>(source.data()),static_cast<std::streamsize>(source.size()*sizeof(double)));
        require(bool(in),"programme exact read");
        for(const int shape:{0,4}){
            Native n(module);n.start(48000,kSample64,2);
            setConstant(n,4,shape,shape==0?.5:1.,0,0);
            SignalBlock b(n,256);
            std::ofstream out(folder/(name+(shape==0?"_rect":"_sine")+".f64le"),std::ios::binary);
            require(bool(out),"programme output open");
            double dryPower=0,wetPower=0,deltaPower=0,errorPower=0,referencePower=0;
            int checked=0;
            for(int i=0;i<frames;){
                const int count=std::min(256,frames-i);b.data.numSamples=count;
                for(int k=0;k<count;++k){b.l[k]=source[2*(i+k)];b.r[k]=source[2*(i+k)+1];}
                b.process(n);
                for(int k=0;k<count;++k){
                    const int j=i+k,phase=j%6000;
                    // Independent steady-state analytic envelope. Exclude minimum-slew
                    // transition samples for rectangular shape; quantify separately
                    // against the slope bound. Sine starts at zero after activation.
                    const bool oracle=j>=6000&&(shape==4||(phase>=32&&phase<3000)||(phase>=3032));
                    const double g=shape==0?(phase<3000?1.:0.):.5-.5*std::cos(2*pi*phase/6000.);
                    for(int c=0;c<2;++c){
                        const double x=source[2*j+c],y=c==0?b.ol[k]:b.orr[k];
                        require(std::isfinite(x)&&std::isfinite(y)&&std::abs(y)<=std::abs(x)+1e-13,"programme finite no boost");
                        require(x*y>=0,"programme polarity");
                        dryPower+=x*x;wetPower+=y*y;deltaPower+=(x-y)*(x-y);
                        if(oracle){
                            const double ref=x*g;
                            errorPower+=(y-ref)*(y-ref);referencePower+=x*x;++checked;
                        }
                        out.write(reinterpret_cast<const char*>(&y),sizeof(y));
                    }
                }
                i+=count;
            }
            require(bool(out)&&checked>10000&&referencePower>1e-5&&dryPower>1e-5,"programme meaningful signal");
            require(wetPower>1e-6&&wetPower<.9*dryPower&&deltaPower>.01*dryPower,"programme audible modulation not constant dry or mute");
            const double relative=std::sqrt(errorPower/referencePower);
            require(relative<1e-8,"programme independent PCM envelope");
            std::cout<<"PROGRAMME_PASS "<<name<<" shape="<<shape<<" frames="<<frames
                     <<" relative_error="<<relative<<" wet_dB="<<10*std::log10(wetPower/dryPower)<<"\n";
            // Same fixture through settled hard dry on a separate native instance.
            Native dry(module);dry.start(48000,kSample64,2);
            ParameterChanges p(14);point(p,110,0,0);dry.flush(p,kSample64);
            SignalBlock d(dry,256);
            for(int i=0;i<frames;){
                const int count=std::min(256,frames-i);d.data.numSamples=count;
                for(int k=0;k<count;++k){d.l[k]=source[2*(i+k)];d.r[k]=source[2*(i+k)+1];}
                d.process(dry);
                for(int k=24;k<count;++k)require(d.ol[k]==d.l[k]&&d.orr[k]==d.r[k],"programme settled exact dry");
                i+=count;
            }
        }
    }
}


#ifdef _WIN32
void pumpGui(int ms){
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
    do{MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }while(std::chrono::steady_clock::now()<end);
}
class GuiFrame final:public FObject, public IPlugFrame {
public:
    explicit GuiFrame(HWND parent):parent_(parent){}
    bool deny=false,omitSameSizeAck=false;int requests=0,omittedAcks=0;
    std::function<tresult(IPlugView*,ViewRect*)> intercept;
    tresult PLUGIN_API resizeView(IPlugView* view,ViewRect* r) override{
        if(!view||!r)return kInvalidArgument;
        ++requests;if(deny)return kResultFalse;
        if(intercept){auto callback=std::move(intercept);intercept={};return callback(view,r);}
        ViewRect current{};
        if(omitSameSizeAck&&view->getSize(&current)==kResultOk&&
           current.getWidth()==r->getWidth()&&current.getHeight()==r->getHeight()){
            ++omittedAcks;return kResultOk;
        }
        RECT outer{0,0,r->getWidth(),r->getHeight()};
        AdjustWindowRectEx(&outer,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_TOOLWINDOW);
        SetWindowPos(parent_,nullptr,0,0,outer.right-outer.left,outer.bottom-outer.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        return view->onSize(r);
    }
    OBJ_METHODS(GuiFrame,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IPlugFrame)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
private: HWND parent_;
};
class GuiHandler final:public FObject,public IComponentHandler,public IComponentHandler2{
public:
    std::array<int,14> begins{},edits{},ends{};
    std::array<double,14> last{};
    Native* relay=nullptr;
    int dirtyCalls=0;
    std::function<void()> dirtyIntercept;
    tresult PLUGIN_API setDirty(TBool state) override{
        if(state){++dirtyCalls;if(dirtyIntercept){auto cb=std::move(dirtyIntercept);dirtyIntercept={};cb();}}
        return kResultOk;
    }
    tresult PLUGIN_API requestOpenEditor(FIDString) override{return kResultFalse;}
    tresult PLUGIN_API startGroupEdit() override{return kResultOk;}
    tresult PLUGIN_API finishGroupEdit() override{return kResultOk;}
    tresult PLUGIN_API beginEdit(ParamID id) override{if(id<100||id>113)return kInvalidArgument;++begins[id-100];return kResultOk;}
    tresult PLUGIN_API performEdit(ParamID id,ParamValue value) override{if(id<100||id>113||value<0||value>1)return kInvalidArgument;++edits[id-100];last[id-100]=value;
        // Test-host relay of the actual performEdit value into the real
        // processor's parameter queue; never a direct DSP test shortcut.
        if(relay){ParameterChanges p(14);point(p,id,0,value);relay->flush(p,kSample64);}
        return kResultOk;}
    tresult PLUGIN_API endEdit(ParamID id) override{if(id<100||id>113)return kInvalidArgument;++ends[id-100];return kResultOk;}
    tresult PLUGIN_API restartComponent(int32) override{return kResultOk;}
    OBJ_METHODS(GuiHandler,FObject)
    DEFINE_INTERFACES
        DEF_INTERFACE(IComponentHandler)
        DEF_INTERFACE(IComponentHandler2)
    END_DEFINE_INTERFACES(FObject)
    REFCOUNT_METHODS(FObject)
};
void clickGui(HWND child,int x,int y){
    SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(x,y));
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(x,y));pumpGui(60);
}
void dragGui(HWND child,int x,int y,int dy){
    SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(x,y));
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
    SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,y-dy));
    SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(x,y-dy));pumpGui(60);
}
void wheelGui(HWND child,int x,int y){
    SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(x,y));
    POINT pt{x,y};ClientToScreen(child,&pt);
    SendMessageW(child,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(pt.x,pt.y));
    pumpGui(100);
}


// Full native control chain: independent known gesture mapping, actual
// performEdit -> host queue -> actual VST3 PCM, compared with a separate
// explicit host-automation fixture and a fresh state-recalled instance.
// Existing analytic DSP oracles remain independent of this parity gate.
void guiAudioChain(const VST3::Hosting::Module::Ptr& module,Native& n,GuiHandler& handler,HWND child){
    using Values=std::array<double,14>;
    constexpr Values base{1./6,3./6,2./6,0,.5,.5,0,.5,.02,.05,.8,0,0,0};
    constexpr int count=72000;
    const auto write=[&](Native& dest,const Values& values){
        ParameterChanges p(14);
        for(std::size_t i=0;i<values.size();++i)point(p,static_cast<ParamID>(100+i),0,values[i]);
        dest.flush(p,kSample64);
    };
    const auto activate=[](Native& dest){
        ParameterChanges p(14);point(p,111,0,1);dest.flush(p,kSample64);
    };
    const auto capture=[&](Native& dest){
        std::vector<double> pcm;pcm.reserve(count);
        for(int offset=0;offset<count;){
            int k=std::min(512,count-offset);auto block=dest.render<double>(k);
            pcm.insert(pcm.end(),block.begin(),block.end());offset+=k;
        }
        return pcm;
    };
    const auto direct=[&](const Values& values,bool start){
        Native reference(module);reference.start(48000,kSample64,2);write(reference,values);
        // All fixtures are inactive until the single explicit rising edge.
        reference.render<double>(64);
        if(start)activate(reference);
        return capture(reference);
    };
    const auto dry=direct(base,false),baseline=direct(base,true);
    struct Gesture{ParamID id;int x,y,dy;double expected;};
    constexpr Gesture actions[]{
        {100,138,314,25,2./6},{101,366,314,25,4./6},{102,594,314,25,3./6},
        {103,822,314,3,1./65},{104,100,468,30,.7},{105,252,468,30,.7},
        {106,844,546,0,1.},{107,404,468,-30,.3},{108,556,468,30,.22},
        {109,708,468,30,.25},{110,860,468,-30,.6},{111,650,34,0,1.},{112,920,34,0,1.}
    };
    for(const auto& g:actions){
        n.start(48000,kSample64,2);write(n,base);n.render<double>(64);
        for(std::size_t i=0;i<base.size();++i)
            require(n.controller->setParamNormalized(static_cast<ParamID>(100+i),base[i])==kResultOk,"chain controller baseline");
        pumpGui(80);
        const auto before=handler.edits;const auto starts=handler.begins;const auto stops=handler.ends;
        handler.relay=&n;
        if(g.dy)dragGui(child,g.x,g.y,g.dy);else clickGui(child,g.x,g.y);
        handler.relay=nullptr;
        const auto index=g.id-100;
        require(handler.edits[index]>before[index]&&handler.begins[index]>starts[index]&&
                handler.ends[index]-stops[index]==handler.begins[index]-starts[index],"chain actual balanced host gesture");
        for(std::size_t i=0;i<base.size();++i)if(i!=index)require(handler.edits[i]==before[i],"chain no cross-parameter edit");
        const double actualValue=handler.last[index];
        require(std::abs(actualValue-g.expected)<1e-6&&std::abs(n.controller->getParamNormalized(g.id)-actualValue)<1e-10,"chain expected gesture value");
        Values expected=base;expected[index]=actualValue;
        if(g.id!=111){n.render<double>(64);activate(n);}
        const auto pcm=capture(n);
        // For ACTIVE the direct reference receives its independent rising
        // edge only after warmup; for others the changed value is explicit.
        auto directSettings=expected;directSettings[11]=0;
        const auto reference=direct(directSettings,true);
        require(pcm==reference,"GUI host callback reaches exact expected processor PCM");
        const auto& unchanged=g.id==111?dry:baseline;
        double maximum=0;for(std::size_t i=0;i<pcm.size();++i)maximum=std::max(maximum,std::abs(pcm[i]-unchanged[i]));
        require(maximum>1e-3,"chain negative control rejects unchanged sound");
        auto saved=n.save();n.stop();
        Native fresh(module);require(fresh.load(saved),"chain fresh audio state restore");
        MemoryStream stream(saved.data(),static_cast<TSize>(saved.size()));
        require(fresh.controller->setComponentState(&stream)==kResultOk&&
            std::abs(fresh.controller->getParamNormalized(g.id)-actualValue)<1e-10,"chain fresh controller value restore");
        fresh.start(48000,kSample64,2);
        require(capture(fresh)==pcm,"chain fresh restored exact PCM");
        std::cout<<"NATIVE_GUI_AUDIO_CHAIN_PASS id="<<g.id<<" changed_peak="<<maximum<<"\n";
    }
    std::cout<<"NATIVE_GUI_AUDIO_ALL_13_PASS\n";
}


void valueEntryContract(const VST3::Hosting::Module::Ptr& module,Native& n,GuiHandler& handler,HWND child){
    struct Entry{ParamID id;int x,y;const wchar_t* text;double expected;};
    const Entry entries[]{
        {100,138,387,L"1/32",5./6},{101,366,387,L"8",3./6},{102,594,387,L"1/64",1},
        {103,822,387,L"1/4",0},{103,822,387,L"64 Bars",1},
        {104,100,509,L"75,5%",74.5/98},{105,252,509,L"-25",.375},
        {107,404,509,L"33.3",.333},{108,556,509,L"12.5%",.125},
        {109,708,509,L"7,5",.075},{110,860,509,L"80",.8}
    };
    const auto edit=[&](const Entry& g,UINT key,bool accepted){
        const auto before=n.controller->getParamNormalized(g.id);const auto edits=handler.edits;const auto begins=handler.begins;const auto ends=handler.ends;
        SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(g.x,g.y));
        SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(g.x,g.y));
        SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(g.x,g.y));pumpGui(80);
        HWND field=GetFocus();wchar_t cls[64]{};
        require(field&&field!=child&&GetClassNameW(field,cls,64)&&_wcsicmp(cls,L"Edit")==0,"double click opens real SDK text field");
        require(SetWindowTextW(field,g.text)!=0,"enter actual text");
        require(n.controller->getParamNormalized(g.id)==before,"uncommitted text leaves parameter unchanged");
        SendMessageW(field,WM_KEYDOWN,key,1);pumpGui(120);
        require(!IsWindow(field)||GetFocus()!=field,"text field closes");
        const auto actual=n.controller->getParamNormalized(g.id);
        std::cout<<"NATIVE_VALUE_ENTRY id="<<g.id<<" value="<<actual<<" accepted="<<accepted<<"\n";
        require(std::abs(actual-(accepted?g.expected:before))<1e-9,"typed value normalized");
        for(std::size_t i=0;i<edits.size();++i){
            const int delta=accepted&&i==g.id-100?1:0;
            require(handler.edits[i]-edits[i]==delta&&handler.begins[i]-begins[i]==delta&&handler.ends[i]-ends[i]==delta,"typed value balanced/no spurious automation");
        }
    };
    for(const auto& g:entries)edit(g,VK_RETURN,true);
    edit({100,138,387,L"1/3",0},VK_RETURN,false);
    edit({103,822,387,L"65",0},VK_RETURN,false);
    edit({104,100,509,L"100",0},VK_RETURN,false);
    edit({107,404,509,L"25oops",0},VK_RETURN,false);
    edit({105,252,509,L"+-25%",0},VK_RETURN,false);
    edit({110,860,509,L"5",0},VK_ESCAPE,false);
    for(bool commit:{true,false}){
        require(n.controller->setParamNormalized(107,.5)==kResultOk,"text echo baseline");pumpGui(80);
        SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(404,509));
        SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(404,509));pumpGui(80);
        HWND field=GetFocus();wchar_t cls[64]{};
        require(field&&field!=child&&GetClassNameW(field,cls,64)&&_wcsicmp(cls,L"Edit")==0,"echo field opens");
        require(SetWindowTextW(field,L"37.25")!=0,"echo local buffer");
        SendMessageW(field,EM_SETSEL,2,2);
        const auto edits=handler.edits;
        require(n.controller->setParamNormalized(107,.8)==kResultOk,"external automation during edit");pumpGui(80);
        wchar_t local[32]{};GetWindowTextW(field,local,32);
        DWORD from=0,to=0;SendMessageW(field,EM_GETSEL,reinterpret_cast<WPARAM>(&from),reinterpret_cast<LPARAM>(&to));
        require(std::wstring(local)==L"37.25"&&from==2&&to==2,"automation preserves local text and caret");
        SendMessageW(field,WM_KEYDOWN,commit?VK_RETURN:VK_ESCAPE,1);pumpGui(100);
        require(std::abs(n.controller->getParamNormalized(107)-(commit?.3725:.8))<1e-9&&
                handler.edits[7]-edits[7]==(commit?1:0),"text commit/cancel after automation");
    }
    for(int mode=0;mode<4;++mode){
        n.start(48000,kSample64,2);
        setConstant(n,6,0,.5,0,0);
        require(n.controller->setParamNormalized(107,.5)==kResultOk,"recall text baseline");pumpGui(80);
        SendMessageW(child,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(404,509));
        SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(404,509));pumpGui(80);
        HWND field=GetFocus();wchar_t cls[64]{};
        require(field&&field!=child&&GetClassNameW(field,cls,64)&&_wcsicmp(cls,L"Edit")==0,"recall edit field");
        require(SetWindowTextW(field,L"37.25")!=0,"recall local text");
        auto state=n.save();const double width=.8;std::memcpy(state.data()+8+7*8,&width,8);
        auto expected=state;
        if(mode==3){
            const double typed=.3725;std::memcpy(expected.data()+8+7*8,&typed,8);state.resize(119);
        }
        require(n.load(state)==(mode!=3),"processor recall while text open");
        MemoryStream stream(state.data(),static_cast<TSize>(state.size()));
        const auto edits=handler.edits,begins=handler.begins,ends=handler.ends;
        require((n.controller->setComponentState(&stream)==kResultOk)==(mode!=3),"recall while editing");
        pumpGui(80);
        handler.relay=&n;
        if(mode==2)SetFocus(child);
        else SendMessageW(field,WM_KEYDOWN,mode==1?VK_ESCAPE:VK_RETURN,1);
        pumpGui(100);handler.relay=nullptr;
        require(!IsWindow(field)||GetFocus()!=field,"recall text closes");
        require(std::abs(n.controller->getParamNormalized(107)-(mode==3?.3725:.8))<1e-9,"stale recall text never commits");
        for(std::size_t i=0;i<edits.size();++i){
            const int delta=mode==3&&i==7?1:0;
            require(handler.edits[i]-edits[i]==delta&&handler.begins[i]-begins[i]==delta&&handler.ends[i]-ends[i]==delta,"recall text edit gestures");
        }
        require(n.save()==expected,"recall text actual processor state");
        // Restart both processors at the same saved state, then require
        // exact audible modulation rather than a trivially dry comparison.
        n.stop();n.start(48000,kSample64,2);
        Native reference(module);require(reference.load(expected),"recall reference state");
        reference.start(48000,kSample64,2);
        bool wet=false,dry=false;
        for(int i=0;i<8192;i+=512){
            const auto pcm=n.render<double>(512),ref=reference.render<double>(512);
            require(pcm==ref,"recall text exact PCM");
            for(double x:pcm){wet|=x<.1;dry|=x>.9;}
        }
        require(wet&&dry,"recall PCM nontrivial modulation");n.stop();
    }
    std::cout<<"NATIVE_P009_RECALL_TEXT_PCM_PASS\n";
    std::cout<<"NATIVE_P009_RECALL_TEXT_EPOCH_PASS\n";
    std::cout<<"NATIVE_P009_TEXT_AUTOMATION_PASS\n";
    std::cout<<"NATIVE_DOUBLE_CLICK_ALL_10_READOUTS_PASS\n";
}

using PreviewSettings=std::array<double,7>;
// Independent oracle: closed-form phase integration on a four-quarter bar.
// No inclusion of the product's Core/Parameters/Preview implementation.
bool previewExpected(const PreviewSettings& p,double x){
    const auto division=[](double v){return std::pow(2.,std::round(v*6));};
    const double a=division(p[0]),b=division(p[1]),c=division(p[2]);
    const int index=static_cast<int>(std::round(p[3]*65));
    const double bars=index==0?.25:index==1?.5:index-1.;
    const double m=.01+.98*p[4],k=std::pow(2.,4*p[5]-2);
    double integrated=0;
    if(x<=m){
        const double u=x/m;
        integrated=a*x+(b-a)*m*std::pow(u,k+1)/(k+1);
    }else{
        const double z=x-m;
        integrated=m*(a+(b-a)/(k+1))+b*z+(c-b)*(1-m)*std::pow(z/(1-m),k+1)/(k+1);
    }
    const double phase=bars*integrated;
    return phase-std::floor(phase)<p[6];
}
std::vector<bool> previewRow(HWND child,int y,bool desktop=false){
    constexpr int first=52,last=908;
    // Synchronize the compositor without forcing a product redraw.
    if(HMODULE dwm=LoadLibraryW(L"dwmapi.dll")){
        using Flush=HRESULT(WINAPI*)();
        if(auto flush=reinterpret_cast<Flush>(GetProcAddress(dwm,"DwmFlush")))flush();
        FreeLibrary(dwm);
    }
    POINT origin{0,y};if(desktop)require(ClientToScreen(child,&origin)!=0,"desktop capture coordinates");
    HWND source=desktop?nullptr:child;
    HDC screen=GetDC(source);require(screen!=nullptr,"preview screen DC");
    HDC dc=CreateCompatibleDC(screen);require(dc!=nullptr,"preview memory DC");
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=960;bi.bmiHeader.biHeight=-1;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(bitmap&&pixels,"preview scanline DIB");
    auto prev=SelectObject(dc,bitmap);
    require(BitBlt(dc,0,0,960,1,screen,origin.x,origin.y,desktop?(SRCCOPY|CAPTUREBLT):SRCCOPY)!=0,"preview scanline capture");GdiFlush();
    const auto* data=static_cast<const std::uint32_t*>(pixels);
    std::vector<bool> result;result.reserve(last-first+1);
    for(int x=first;x<=last;++x){
        const auto v=data[x];const int r=(v>>16)&255,g=(v>>8)&255,b=v&255;
        result.push_back(r>170&&g>100&&g<205&&b<100);
    }
    SelectObject(dc,prev);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(source,screen);
    return result;
}
bool matchesPreview(const std::vector<bool>& actual,const PreviewSettings& p){
    constexpr double origin=24+912.*10/340,span=912.*320/340;
    int stable=0,errors=0,open=0,closed=0;
    for(std::size_t i=0;i<actual.size();++i){
        const double x=52+double(i)+.5;
        const bool expected=previewExpected(p,(x-origin)/span);
        // Ignore the antialiased transition boundary, not the gate interior.
        if(previewExpected(p,(x-1.5-origin)/span)!=expected||
           previewExpected(p,(x+1.5-origin)/span)!=expected)continue;
        ++stable;open+=expected;closed+=!expected;errors+=actual[i]!=expected;
    }
    std::cout<<"PREVIEW_ORACLE stable="<<stable<<" errors="<<errors<<" open="<<open<<" closed="<<closed<<"\n";
    return stable>500&&errors<=2;
}
void assertPreview(const std::vector<bool>& actual,const PreviewSettings& p){require(matchesPreview(actual,p),"rendered timing diagram matches independent phase oracle");}
// Capture actual composed preview pixels, including fractional pulse
// density and label glyphs (the binary orange oracle is not sufficient
// when thousands of gates occupy fewer physical pixels).
std::vector<std::uint32_t> previewPixels(HWND child,int x,int y,int w,int h,bool desktop){
    if(HMODULE dwm=LoadLibraryW(L"dwmapi.dll")){
        using Flush=HRESULT(WINAPI*)();
        if(auto flush=reinterpret_cast<Flush>(GetProcAddress(dwm,"DwmFlush")))flush();
        FreeLibrary(dwm);
    }
    POINT origin{x,y};if(desktop)require(ClientToScreen(child,&origin)!=0,"preview region coords");
    HWND source=desktop?nullptr:child;HDC screen=GetDC(source),dc=CreateCompatibleDC(screen);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;
    bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(screen&&dc&&bitmap&&pixels,"preview region bitmap");
    auto prev=SelectObject(dc,bitmap);
    require(BitBlt(dc,0,0,w,h,screen,origin.x,origin.y,desktop?(SRCCOPY|CAPTUREBLT):SRCCOPY)!=0,"preview region capture");GdiFlush();
    const auto* data=static_cast<const std::uint32_t*>(pixels);
    std::vector<std::uint32_t> result(data,data+w*h);
    SelectObject(dc,prev);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(source,screen);return result;
}
void previewContract(Native& n,HWND child){
    constexpr std::array<ParamID,7> ids{100,101,102,103,104,105,107};
    constexpr PreviewSettings baseline{4./6,1./6,4./6,5./65,.5,.5,.5};
    const auto apply=[&](const PreviewSettings& p){
        for(std::size_t i=0;i<ids.size();++i)
            require(n.controller->setParamNormalized(ids[i],p[i])==kResultOk,"preview set host parameter");
        pumpGui(180);
    };
    apply(baseline);
    const auto childBase=previewRow(child,125);
    const auto desktopBase=previewRow(child,125,true);
    const bool desktop=matchesPreview(desktopBase,baseline);
    std::cout<<"PREVIEW_CAPTURE_SOURCE "<<(desktop?"desktop composed pixels":"child DC pixels")<<"\n";
    const auto base=desktop?desktopBase:childBase;
    assertPreview(base,baseline);
    require(previewRow(child,110,desktop)==base&&previewRow(child,145,desktop)==base,"equal height timing rectangles");
    // Every knob changes the actual rendered pixels by left-button drag.
    // Check the mathematically expected new pattern, not merely a repaint.
    constexpr std::array<std::array<int,3>,7> gestures{{{{138,314,-25}},{{366,314,25}},{{594,314,-25}},
        {{822,314,3}},{{100,468,30}},{{252,468,30}},{{404,468,-30}}}};
    constexpr PreviewSettings expected{3./6,2./6,3./6,6./65,.7,.7,.3};
    for(std::size_t i=0;i<ids.size();++i){
        apply(baseline);
        const auto& g=gestures[i];
        dragGui(child,g[0],g[1],g[2]);pumpGui(180);
        auto changed=baseline;changed[i]=expected[i];
        require(std::abs(n.controller->getParamNormalized(ids[i])-changed[i])<1e-6,"preview drag parameter");
        const auto actual=previewRow(child,125,desktop);
        int delta=0;for(std::size_t x=0;x<base.size();++x)delta+=actual[x]!=base[x];
        const auto alternate=previewRow(child,125,!desktop);
        const auto& altBase=desktop?childBase:desktopBase;
        int altDelta=0;for(std::size_t x=0;x<base.size();++x)altDelta+=alternate[x]!=altBase[x];
        std::cout<<"PREVIEW_PIXEL_DIAGNOSTIC id="<<ids[i]<<" delta="<<delta<<" alternate_delta="<<altDelta<<"\n";
        if(delta<=12){
            // Diagnostic only: forced repaint may distinguish capture/paint
            // delivery from the phase algorithm. Do not accept it as a pass.
            RedrawWindow(child,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);pumpGui(120);
            const auto forced=previewRow(child,125,desktop);int forcedDelta=0;
            for(std::size_t x=0;x<base.size();++x)forcedDelta+=forced[x]!=base[x];
            std::cout<<"PREVIEW_FORCED_PAINT_DIAGNOSTIC delta="<<forcedDelta<<"\n";
        }
        assertPreview(actual,changed);
        require(delta>12,"preview changed rather than static reference");
        std::cout<<"NATIVE_PREVIEW_REACTIVE_PASS id="<<ids[i]<<" changed_pixels="<<delta<<"\n";
    }
    // SHAPE/ATTACK/RELEASE/MIX do not alter the timing geometry.
    apply(baseline);
    for(const auto id:{106u,108u,109u,110u}){
        require(n.controller->setParamNormalized(id,id==110?0.:1.)==kResultOk,"non timing parameter");
        pumpGui(120);require(previewRow(child,125,desktop)==base,"timing geometry independent of gain shape");
    }
    auto edge=baseline;edge[6]=0;apply(edge);const auto empty=previewRow(child,125,desktop);
    require(std::none_of(empty.begin(),empty.end(),[](bool v){return v;}),"width zero empty preview");
    edge[6]=1;apply(edge);assertPreview(previewRow(child,125,desktop),edge);
    // Extreme label positions: identify real START/END glyph columns
    // from the center-position capture, then require those pixels to
    // remain identical when MID moves to either extreme.
    apply(baseline);
    constexpr int labelW=912,labelH=25;
    const auto labels=previewPixels(child,24,170,labelW,labelH,desktop);
    const auto glyph=[](std::uint32_t px){
        const int r=(px>>16)&255,g=(px>>8)&255,b=px&255;
        return r>100&&g>100&&b>100&&std::abs(r-g)<12&&std::abs(g-b)<12;
    };
    int startRight=-1,endLeft=labelW,centerCount=0;
    for(int yy=0;yy<labelH;++yy)for(int xx=0;xx<labelW;++xx){
        if(!glyph(labels[yy*labelW+xx]))continue;
        if(xx<labelW/3)startRight=std::max(startRight,xx);
        else if(xx>2*labelW/3)endLeft=std::min(endLeft,xx);
        else ++centerCount;
    }
    require(startRight>25&&startRight<120&&endLeft>labelW-120&&endLeft<labelW-25&&centerCount>10,"three reference label glyphs");
    for(double pos:{0.,1.}){
        auto extreme=baseline;extreme[4]=pos;apply(extreme);
        assertPreview(previewRow(child,125,desktop),extreme);
        const auto actual=previewPixels(child,24,170,labelW,labelH,desktop);
        int midPixels=0;
        for(int yy=0;yy<labelH;++yy)for(int xx=0;xx<labelW;++xx){
            const int k=yy*labelW+xx;
            if(xx<=startRight+2||xx>=endLeft-2)require(glyph(actual[k])==glyph(labels[k]),"extreme label preserves START and END glyphs");
            else midPixels+=glyph(actual[k]);
        }
        require(midPixels>centerCount/2&&midPixels<centerCount*2,"extreme MID remains readable");
    }
    // 4096 impulses: check the continuous color coverage instead of
    // requiring unresolved subpixel gaps to have a binary screen width.
    PreviewSettings dense{1,1,1,1,.5,.5,0};
    const auto redSum=[&](){
        const auto pixels=previewPixels(child,52,110,856,35,desktop);
        double sum=0;for(auto px:pixels)sum+=double((px>>16)&255);return sum;
    };
    apply(dense);const double off=redSum();
    const auto denseOff=previewRow(child,125,desktop);
    require(std::none_of(denseOff.begin(),denseOff.end(),[](bool v){return v;}),"dense zero width");
    dense[6]=1;apply(dense);const double full=redSum();
    require(full>off+1000000,"dense full width positive reference");
    dense[6]=.5;apply(dense);const double half=redSum();
    const double coverage=(half-off)/(full-off);
    std::cout<<"PREVIEW_DENSE_COVERAGE ratio="<<coverage<<"\n";
    require(coverage>.15&&coverage<.85,"dense half width distinct from both extremes");
    apply(baseline);require(previewRow(child,125,desktop)==base,"preview host restore exact");
    std::cout<<"NATIVE_P009_PREVIEW_EXTREMES_PASS\n";
    std::cout<<"NATIVE_TIMING_DISPLAY_ALL_7_REACTIVE_PASS\n";
}


// VSTGUI's pinned Win32 backend exposes real tooltip common controls.
// Inspect only the live window for the requested tool on this UI thread.
// Capture the actual composed desktop, not the stale child-window DC.
// A shown help panel has the dedicated background color and rendered text.
// Check two distinct contextual panels, the disabled state and disappearance.
struct HelpPixels{
    std::vector<std::uint32_t> data;
};
HelpPixels helpPixels(HWND child){
    if(HMODULE dwm=LoadLibraryW(L"dwmapi.dll")){
        using Flush=HRESULT(WINAPI*)();
        if(auto flush=reinterpret_cast<Flush>(GetProcAddress(dwm,"DwmFlush")))flush();
        FreeLibrary(dwm);
    }
    constexpr int w=912,h=18;
    POINT p{24,52};require(ClientToScreen(child,&p)!=0,"help capture coordinates");
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(screen&&dc&&bitmap&&pixels,"help desktop bitmap");
    auto prev=SelectObject(dc,bitmap);
    require(BitBlt(dc,0,0,w,h,screen,p.x,p.y,SRCCOPY|CAPTUREBLT)!=0,"help desktop capture");GdiFlush();
    HelpPixels result;const auto* data=static_cast<const std::uint32_t*>(pixels);
    result.data.reserve(w*h);for(int k=0;k<w*h;++k)result.data.push_back(data[k]&0xffffffu);
    SelectObject(dc,prev);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);return result;
}
void checkHelpText(const HelpPixels& base,const HelpPixels& shown,const char* why){
    constexpr int w=912,h=18;
    int count=0,left=w,right=-1,top=h,bottom=-1;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const auto k=y*w+x;
        if(base.data[k]==shown.data[k])continue;
        ++count;left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
    }
    std::cout<<"HELP_TEXT_PIXELS changed="<<count<<" bounds="<<left<<","<<top<<".."<<right<<","<<bottom<<"\n";
    // Thin glyphs, not a filled rectangle; symmetric text bounds.
    require(count>60&&count<2500&&right-left>80&&right-left<650&&std::abs((left+right)*.5-(w-1)*.5)<8&&bottom-top<16,why);
}
void helpContract(HWND host,HWND child){
    SetWindowPos(host,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
    SetForegroundWindow(host);pumpGui(100);
    POINT old{};require(GetCursorPos(&old)!=0,"help save cursor");
    const auto hover=[&](int x,int y){
        POINT p{x,y};require(ClientToScreen(child,&p)!=0&&SetCursorPos(p.x,p.y)!=0,"help hover real cursor");pumpGui(200);
    };
    const auto click=[&](){
        INPUT press[2]{};press[0].type=press[1].type=INPUT_MOUSE;press[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;press[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        require(SendInput(2,press,sizeof(INPUT))==2,"help real mouse click");pumpGui(100);
    };
    hover(24,60);const auto base=helpPixels(child);
    hover(138,314);require(helpPixels(child).data==base.data,"help default off hides mouseover");
    hover(508,32);click();hover(138,314);
    const auto first=helpPixels(child);checkHelpText(base,first,"START centered text only");
    hover(24,60);require(helpPixels(child).data==base.data,"help vanishes on leave");
    hover(404,468);const auto second=helpPixels(child);checkHelpText(base,second,"WIDTH centered text only");
    require(second.data!=first.data,"distinct contextual text");
    hover(508,32);click();hover(138,314);require(helpPixels(child).data==base.data,"help disabled hides mouseover");
    SetCursorPos(old.x,old.y);pumpGui(80);
    std::cout<<"NATIVE_HELP_MOUSEOVER_ON_OFF_PASS\n";
}

void guiContract(const VST3::Hosting::Module::Ptr& module,const std::filesystem::path& folder,bool previewOnly=false){
    Native n(module);auto handler=owned(new GuiHandler);
    require(n.controller->setComponentHandler(handler.get())==kResultOk,"gui handler");
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"ChopperatorNativeTest";
    require(RegisterClassW(&wc)!=0,"gui host class");
    std::filesystem::create_directories(folder);
    for(int cycle=0;cycle<2;++cycle){
        auto view=owned(n.controller->createView(ViewType::kEditor));
        require(view.get()!=nullptr,"native editor present");
        require(view->isPlatformTypeSupported(kPlatformTypeHWND)==kResultTrue,"native HWND supported");
        ViewRect size{};require(view->getSize(&size)==kResultOk&&size.getWidth()==960&&size.getHeight()==576,"initial native editor dimensions");
        RECT outer{0,0,size.getWidth(),size.getHeight()};AdjustWindowRectEx(&outer,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_TOOLWINDOW);
        HWND host=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Chopperator editor QA",WS_OVERLAPPEDWINDOW,
            0,0,outer.right-outer.left,outer.bottom-outer.top,nullptr,nullptr,inst,nullptr);
        require(host!=nullptr,"gui host window");auto frame=owned(new GuiFrame(host));
        require(view->setFrame(frame.get())==kResultOk,"gui frame");
        require(view->attached(host,kPlatformTypeHWND)==kResultOk,"editor attach");
        ShowWindow(host,SW_SHOWNA);UpdateWindow(host);pumpGui(150);
        HWND child=GetWindow(host,GW_CHILD);require(child!=nullptr,"native editor child");
        if(previewOnly){
            // Independent fresh editor, no unrelated resize/other QA state.
            SetWindowPos(host,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            pumpGui(150);previewContract(n,child);
            require(view->removed()==kResultOk&&view->setFrame(nullptr)==kResultOk,"preview editor removal");
            DestroyWindow(host);pumpGui(50);break;
        }
        FUnknownPtr<IParameterFinder> finder(view.get());
        require(finder.getInterface()!=nullptr,"native parameter finder");
        const std::array<std::array<int,3>,13> hits{{{{138,314,100}},{{366,314,101}},{{594,314,102}},{{822,314,103}},
            {{100,468,104}},{{252,468,105}},{{120,546,106}},{{404,468,107}},{{556,468,108}},{{708,468,109}},
            {{860,468,110}},{{610,34,111}},{{820,34,112}}}};
        for(const auto& hit:hits){ParamID id=0;require(finder->findParameter(hit[0],hit[1],id)==kResultTrue&&id==static_cast<ParamID>(hit[2]),"native parameter hit mapping");}
        if(cycle==0){guiAudioChain(module,n,*handler,child);valueEntryContract(module,n,*handler,child);}
        require(n.controller->setParamNormalized(111,0)==kResultOk,"host active off");
        require(n.controller->setParamNormalized(106,0)==kResultOk,"host rect");
        require(n.controller->setParamNormalized(112,0)==kResultOk,"host one shot");pumpGui(80);
        clickGui(child,650,34);clickGui(child,920,34);clickGui(child,844,546);
        require(n.controller->getParamNormalized(111)==1&&n.controller->getParamNormalized(112)==1&&n.controller->getParamNormalized(106)==1,"native segmented controls");
        for(int id:{106,111,112})require(handler->begins[id-100]>0&&handler->edits[id-100]>0&&handler->ends[id-100]==handler->begins[id-100],"balanced edit notifications");
        require(n.controller->setParamNormalized(100,1./6)==kResultOk,"host start 1/2");
        require(n.controller->setParamNormalized(103,3./65)==kResultOk,"host length 2 bars");
        pumpGui(100);
        wheelGui(child,138,314);wheelGui(child,822,314);
        require(std::abs(n.controller->getParamNormalized(100)-2./6)<1e-8,"one wheel notch next note division");
        require(std::abs(n.controller->getParamNormalized(103)-4./65)<1e-8,"one wheel notch next length");
        pumpGui(700);
        // The owner has no mouse wheel: prove every LENGTH value can be
        // reached by left-button movement alone, then reverse the gesture.
        require(n.controller->setParamNormalized(103,0)==kResultOk,"host length minimum");
        pumpGui(80);
        SendMessageW(child,WM_MOUSEMOVE,0,MAKELPARAM(822,314));
        SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(822,314));
        for(int i=1;i<=65;++i){
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(822,314-3*i));
            require(std::abs(n.controller->getParamNormalized(103)-double(i)/65)<1e-8,"left drag reaches next length");
        }
        for(int i=64;i>=0;--i){
            SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(822,314-3*i));
            require(std::abs(n.controller->getParamNormalized(103)-double(i)/65)<1e-8,"left drag reverses every length");
        }
        SendMessageW(child,WM_LBUTTONUP,0,MAKELPARAM(822,314));pumpGui(80);
        require(n.controller->setParamNormalized(100,1./6)==kResultOk,"host start before drag");
        pumpGui(80);dragGui(child,138,314,25);
        require(std::abs(n.controller->getParamNormalized(100)-2./6)<1e-8,"left drag next note");
        require(n.controller->setParamNormalized(107,.5)==kResultOk,"host width before drag");
        pumpGui(80);dragGui(child,404,468,15);
        require(std::abs(n.controller->getParamNormalized(107)-.6)<1e-5,"left drag continuous width");
        std::cout<<"NATIVE_LEFT_DRAG_ALL_66_LENGTHS_PASS\n";
        // Reset via genuine Windows modifier state; WM_LBUTTONDOWN wParam alone
        // is insufficient for frameworks that query GetKeyState(VK_CONTROL).
        const std::array<std::array<double,4>,10> knobs{{{{138,314,100,1./6}},{{366,314,101,4./6}},{{594,314,102,1./6}},
            {{822,314,103,3./65}},{{100,468,104,.5}},{{252,468,105,.5}},{{404,468,107,.5}},{{556,468,108,.02}},
            {{708,468,109,.05}},{{860,468,110,1}}}};
        for(const auto& knob:knobs){
            const auto id=static_cast<ParamID>(knob[2]);const int x=static_cast<int>(knob[0]),y=static_cast<int>(knob[1]);
            const double def=knob[3],other=def>.5?0.:1.;
            require(n.controller->setParamNormalized(id,other)==kResultOk,"host nondefault before reset");pumpGui(40);
            const int previous=handler->edits[id-100];
            BYTE keys[256]{};require(GetKeyboardState(keys)!=0,"read keyboard state");
            BYTE modified[256]{};std::copy(std::begin(keys),std::end(keys),std::begin(modified));
            modified[VK_CONTROL]|=0x80;modified[VK_LCONTROL]|=0x80;
            require(SetKeyboardState(modified)!=0,"set ctrl key state");
            SendMessageW(child,WM_MOUSEMOVE,MK_CONTROL,MAKELPARAM(x,y));
            SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,MAKELPARAM(x,y));
            SendMessageW(child,WM_LBUTTONUP,MK_CONTROL,MAKELPARAM(x,y));
            require(SetKeyboardState(keys)!=0,"restore keyboard state");pumpGui(40);
            require(std::abs(n.controller->getParamNormalized(id)-def)<1e-6,"native Ctrl+left default reset");
            require(handler->edits[id-100]>previous&&std::abs(handler->last[id-100]-def)<1e-6,"reset host edit value");
        }
        std::cout<<"NATIVE_DEFAULT_RESET_ALL_10_KNOBS_PASS\n";
        // Actual editor resize negotiation; no claim of Studio Pro support.
        require(view->canResize()==kResultTrue,"editor resizable");
        ViewRect scaled{0,0,720,432};require(view->checkSizeConstraint(&scaled)==kResultOk,"resize constraint");
        require(scaled.getWidth()==720&&scaled.getHeight()==432,"aspect resize 75%");
        require(frame->resizeView(view.get(),&scaled)==kResultOk,"resize editor");pumpGui(100);
        require(view->getSize(&size)==kResultOk,"query resized geometry");
        RECT childRect{};require(GetClientRect(child,&childRect)!=0,"query resized HWND");
        std::cout<<"NATIVE_RESIZE actual="<<size.getWidth()<<"x"<<size.getHeight()<<" child="<<childRect.right<<"x"<<childRect.bottom<<"\n";
        require(size.getWidth()==720&&size.getHeight()==432&&childRect.right==720&&childRect.bottom==432,"actual resized geometry");
        ViewRect original{0,0,960,576};require(view->checkSizeConstraint(&original)==kResultOk&&frame->resizeView(view.get(),&original)==kResultOk,"resize restore");pumpGui(100);
        require(view->getSize(&size)==kResultOk&&size.getWidth()==960&&size.getHeight()==576,"restored geometry");
        require(GetClientRect(child,&childRect)!=0&&childRect.right==960&&childRect.bottom==576,"restored child geometry");
        // Use a real desktop cursor and real button state for the grip.
        // A posted/sent WM_MOUSEMOVE does NOT update the Windows cursor:
        // after resizing, Windows generates a move at the real location.
        SetWindowPos(host,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
        SetForegroundWindow(host);pumpGui(100);
        POINT oldCursor{};require(GetCursorPos(&oldCursor)!=0,"save desktop cursor");
        const auto moveCursor=[&](int x,int y){
            POINT p{x,y};require(ClientToScreen(child,&p)!=0,"grip screen coordinates");
            require(SetCursorPos(p.x,p.y)!=0,"real desktop pointer movement");
            pumpGui(100);
            POINT actual{};require(GetCursorPos(&actual)!=0&&actual.x==p.x&&actual.y==p.y,"desktop cursor reached requested point");
        };
        const auto button=[&](DWORD flags){
            INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;
            require(SendInput(1,&input,sizeof(input))==1,"real desktop mouse button input");pumpGui(80);
        };
        const auto sizeIs=[&](int w,int h,const char* why){
            require(view->getSize(&size)==kResultOk&&GetClientRect(child,&childRect)!=0,"corner geometry query");
            std::cout<<"NATIVE_GRIP reported="<<size.getWidth()<<"x"<<size.getHeight()<<" child="<<childRect.right<<"x"<<childRect.bottom<<" expected="<<w<<"x"<<h<<"\n";
            if(size.getWidth()!=w||size.getHeight()!=h||childRect.right!=w||childRect.bottom!=h){
                INPUT release{};release.type=INPUT_MOUSE;release.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&release,sizeof(release));
                SetCursorPos(oldCursor.x,oldCursor.y);require(false,why);
            }
        };
        // Shrink first, then grow through two non-preset sizes in the same
        // gesture. Stay inside even a 1024x768 CI desktop.
        moveCursor(950,566);require(WindowFromPoint([](HWND w){POINT p{950,566};ClientToScreen(w,&p);return p;}(child))==child,"corner is unobscured");
        button(MOUSEEVENTF_LEFTDOWN);
        moveCursor(805,479);sizeIs(815,489,"corner drag intermediate shrink");
        moveCursor(890,530);sizeIs(900,540,"corner drag intermediate grow");
        moveCursor(950,566);sizeIs(960,576,"corner drag returns to original size");
        button(MOUSEEVENTF_LEFTUP);
        SetCursorPos(oldCursor.x,oldCursor.y);pumpGui(60);
        // Larger host-accepted geometry and scaled hit mapping are tested
        // separately, independent of the desktop's physical dimensions.
        ViewRect larger{0,0,1200,720};
        require(view->checkSizeConstraint(&larger)==kResultOk&&frame->resizeView(view.get(),&larger)==kResultOk,"host larger geometry");pumpGui(80);
        sizeIs(1200,720,"larger native geometry");
        ParamID scaledId=0;require(finder->findParameter(173,393,scaledId)==kResultTrue&&scaledId==100,"scaled knob hit mapping");
        require(view->checkSizeConstraint(&original)==kResultOk&&frame->resizeView(view.get(),&original)==kResultOk,"corner zoom restore");pumpGui(100);
        std::cout<<"NATIVE_CORNER_CONTINUOUS_ZOOM_PASS\n";
        // Capture the owner reference fast-slow-fast RECT trajectory,
        // not the unrelated short SINE settings left behind by earlier QA.
        const std::array<double,13> previewDefaults{4./6,1./6,4./6,5./65,.5,.5,0,.5,.02,.05,1,0,0};
        for(std::size_t i=0;i<previewDefaults.size();++i)
            require(n.controller->setParamNormalized(static_cast<ParamID>(100+i),previewDefaults[i])==kResultOk,"default preview state");
        pumpGui(180);
        // Optional actual screen evidence; capture failure is not silently called a screenshot.
        if(cycle==0){
            RECT r{};GetClientRect(child,&r);
            const int width=r.right,height=r.bottom;
            HDC screen=GetDC(child),dc=CreateCompatibleDC(screen);
            BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
            void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);
            if(bitmap&&pixels&&width>0&&height>0){
                auto prev=SelectObject(dc,bitmap);BOOL copied=BitBlt(dc,0,0,width,height,screen,0,0,SRCCOPY);GdiFlush();
                auto* data=static_cast<std::uint32_t*>(pixels);
                std::size_t bright=0,orange=0;
                for(int i=0;i<width*height;++i){auto v=data[i];int r=(v>>16)&255,g=(v>>8)&255,b=v&255;
                    bright+=r>150&&g>150&&b>150;orange+=r>180&&g>70&&g<190&&b<110;}
                if(copied&&bright>100&&orange>100){
                    BITMAPFILEHEADER h{};h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(BITMAPINFOHEADER);h.bfSize=h.bfOffBits+width*height*4;
                    std::ofstream f(folder/"native-editor.bmp",std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&bi.bmiHeader),sizeof(bi.bmiHeader));f.write(static_cast<char*>(pixels),width*height*4);
                    if(f)std::cout<<"NATIVE_EDITOR_CAPTURE "<<width<<"x"<<height<<"\n";
                }else std::cout<<"NATIVE_EDITOR_CAPTURE_UNAVAILABLE bright="<<bright<<" orange="<<orange<<"\n";
                SelectObject(dc,prev);
            }
            if(bitmap)DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(child,screen);
        }
        if(cycle==1){
            helpContract(host,child);
            // Save a nondefault UI state through the actual editor and VST3
            // controller, then prove a fresh instance recalls it pre-attach.
            clickGui(child,508,32);
            ViewRect savedSize{0,0,1200,720};
            require(view->checkSizeConstraint(&savedSize)==kResultOk&&frame->resizeView(view.get(),&savedSize)==kResultOk,"save nondefault UI size");
            pumpGui(80);
            MemoryStream savedUI;require(n.controller->getState(&savedUI)==kResultOk&&savedUI.getSize()==20,"controller UI state save");
            std::vector<char> bytes(savedUI.getData(),savedUI.getData()+savedUI.getSize());
            double savedZoom=0;std::uint32_t savedHelp=0;std::memcpy(&savedZoom,bytes.data()+8,8);std::memcpy(&savedHelp,bytes.data()+16,4);
            require(savedZoom==1.25&&savedHelp==1,"saved actual zoom and help switch values");
            Native fresh(module);
            MemoryStream input(bytes.data(),static_cast<TSize>(bytes.size()));
            require(fresh.controller->setState(&input)==kResultOk,"fresh controller UI restore");
            auto recalled=owned(fresh.controller->createView(ViewType::kEditor));ViewRect recalledSize{};
            require(recalled&&recalled->getSize(&recalledSize)==kResultOk&&recalledSize.getWidth()==1200&&recalledSize.getHeight()==720,"fresh recalled editor dimensions");
            auto preset=n.save();MemoryStream presetStream(preset.data(),static_cast<TSize>(preset.size()));
            require(fresh.controller->setComponentState(&presetStream)==kResultOk,"preset component state");
            MemoryStream afterPreset;require(fresh.controller->getState(&afterPreset)==kResultOk&&afterPreset.getSize()==20&&std::memcmp(afterPreset.getData(),bytes.data(),20)==0,"preset leaves UI preferences unchanged");
            for(std::size_t i=0;i<bytes.size();++i){
                MemoryStream truncated(bytes.data(),static_cast<TSize>(i));
                require(fresh.controller->setState(&truncated)!=kResultOk,"truncated UI state rejected");
                MemoryStream after;require(fresh.controller->getState(&after)==kResultOk&&after.getSize()==20&&std::memcmp(after.getData(),bytes.data(),20)==0,"invalid UI state atomic rollback");
            }
            auto sameInstance=owned(n.controller->createView(ViewType::kEditor));
            require(sameInstance&&sameInstance->getSize(&recalledSize)==kResultOk&&recalledSize.getWidth()==1200,"editor reopen retains zoom");
            // Prove the restored editor opens at the persisted size, not
            // merely that getSize reports the requested dimensions.
            RECT recalledOuter{0,0,1200,720};AdjustWindowRectEx(&recalledOuter,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_TOOLWINDOW);
            HWND recalledHost=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Chopperator recall QA",WS_OVERLAPPEDWINDOW,
                0,0,recalledOuter.right-recalledOuter.left,recalledOuter.bottom-recalledOuter.top,nullptr,nullptr,inst,nullptr);
            require(recalledHost!=nullptr,"fresh recall host HWND");
            auto recalledFrame=owned(new GuiFrame(recalledHost));
            require(recalled->setFrame(recalledFrame.get())==kResultOk&&recalled->attached(recalledHost,kPlatformTypeHWND)==kResultOk,"fresh recalled editor attach");
            ShowWindow(recalledHost,SW_SHOWNA);UpdateWindow(recalledHost);pumpGui(150);
            HWND recalledChild=GetWindow(recalledHost,GW_CHILD);RECT actualRecall{};
            require(recalledChild&&GetClientRect(recalledChild,&actualRecall)!=0&&actualRecall.right==1200&&actualRecall.bottom==720,"fresh recalled native dimensions");
            FUnknownPtr<IParameterFinder> recalledFinder(recalled.get());ParamID recalledId=0;
            require(recalledFinder.getInterface()&&recalledFinder->findParameter(173,393,recalledId)==kResultTrue&&recalledId==100,"fresh recalled scaled control mapping");
            require(recalled->removed()==kResultOk&&recalled->setFrame(nullptr)==kResultOk,"fresh recalled editor detach");
            DestroyWindow(recalledHost);pumpGui(50);
            std::cout<<"NATIVE_UI_STATE_RECALL_PASS\n";
            std::cout<<"NATIVE_UI_RECALLED_WINDOW_PASS\n";
        }
        require(view->removed()==kResultOk&&view->setFrame(nullptr)==kResultOk,"editor removal");DestroyWindow(host);pumpGui(50);
    }
    require(n.controller->setComponentHandler(nullptr)==kResultOk,"clear handler");
    UnregisterClassW(wc.lpszClassName,inst);
    if(!previewOnly)std::cout<<"NATIVE_EDITOR_CONTRACT_PASS two attach cycles, 13 hit mappings, three segment edits, discrete wheel steps, resize roundtrip\n";
}

// Two simultaneous native views, shared recall, real host geometry and DPI.
void liveUIAudit(const VST3::Hosting::Module::Ptr& module){
    Native n(module);auto handler=owned(new GuiHandler);
    require(n.controller->setComponentHandler(handler.get())==kResultOk,"live UI handler");
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"ChopperatorLiveAudit";
    require(RegisterClassW(&wc)!=0,"live UI class");
    auto a=owned(n.controller->createView(ViewType::kEditor));
    auto b=owned(n.controller->createView(ViewType::kEditor));
    FUnknownPtr<IPlugViewContentScaleSupport> dpi(a.get());
    require(a&&b&&dpi.getInterface(),"two real editors and DPI interface");
    const auto makeHost=[&](){
        RECT r{0,0,960,576};AdjustWindowRectEx(&r,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_TOOLWINDOW);
        HWND w=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Live audit",WS_OVERLAPPEDWINDOW,
            0,0,r.right-r.left,r.bottom-r.top,nullptr,nullptr,inst,nullptr);
        require(w!=nullptr,"live HWND");return w;
    };
    HWND ha=makeHost(),hb=makeHost();auto fa=owned(new GuiFrame(ha)),fb=owned(new GuiFrame(hb));
    require(a->setFrame(fa.get())==kResultOk&&a->attached(ha,kPlatformTypeHWND)==kResultOk&&
        b->setFrame(fb.get())==kResultOk&&b->attached(hb,kPlatformTypeHWND)==kResultOk,"two simultaneous attaches");
    ShowWindow(ha,SW_SHOWNA);ShowWindow(hb,SW_SHOWNA);pumpGui(150);
    HWND ca=GetWindow(ha,GW_CHILD),cb=GetWindow(hb,GW_CHILD);
    require(ca&&cb&&ca!=cb,"distinct native children");
    const auto geom=[](IPlugView* v,HWND child,int w,int h){
        ViewRect vr{};RECT cr{};
        require(v->getSize(&vr)==kResultOk&&GetClientRect(child,&cr)!=0&&vr.getWidth()==w&&
            vr.getHeight()==h&&cr.right==w&&cr.bottom==h,"live UI actual geometry");
    };
    const auto load=[&](double zoom,std::uint32_t help,bool expected=true){
        MemoryStream state;std::uint32_t tag=0x31554943u,version=1;int32 count=0;int64 position=-1;
        require(state.write(&tag,4,&count)==kResultOk&&state.write(&version,4,&count)==kResultOk&&
            state.write(&zoom,8,&count)==kResultOk&&state.write(&help,4,&count)==kResultOk&&
            state.seek(0,IBStream::kIBSeekSet,&position)==kResultOk&&position==0,"live UI fixture");
        require((n.controller->setState(&state)==kResultOk)==expected,"live UI recall status");pumpGui(100);
    };
    const auto saved=[&](double zoom,std::uint32_t help){
        MemoryStream state;require(n.controller->getState(&state)==kResultOk&&state.getSize()==20,"live UI saved");
        double z=0;std::uint32_t h=0;std::memcpy(&z,state.getData()+8,8);std::memcpy(&h,state.getData()+16,4);
        require(z==zoom&&h==help,"live UI authoritative saved preferences");
    };
    geom(a.get(),ca,960,576);geom(b.get(),cb,960,576);
    load(1.25,1);geom(a.get(),ca,1200,720);geom(b.get(),cb,1200,720);saved(1.25,1);
    require(handler->dirtyCalls==0,"recall does not dirty project");
    load(1.75,0,false);geom(a.get(),ca,1200,720);geom(b.get(),cb,1200,720);saved(1.25,1);
    require(dpi->setContentScaleFactor(1.5f)==kResultOk,"live DPI 150");pumpGui(100);
    geom(a.get(),ca,1800,1080);geom(b.get(),cb,1200,720);saved(1.25,1);
    FUnknownPtr<IParameterFinder> finder(a.get());ParamID id=0;
    require(finder.getInterface()&&finder->findParameter(259,589,id)==kResultTrue&&id==100,"DPI scaled hit mapping");
    require(dpi->setContentScaleFactor(1.f)==kResultOk,"DPI return");pumpGui(100);geom(a.get(),ca,1200,720);
    fa->deny=true;const auto denied=fa->requests;
    require(dpi->setContentScaleFactor(1.5f)!=kResultOk&&fa->requests>denied,"DPI host rejection");
    pumpGui(100);geom(a.get(),ca,1200,720);saved(1.25,1);
    fa->deny=false;require(dpi->setContentScaleFactor(1.5f)==kResultOk,"DPI recovers");pumpGui(80);
    geom(a.get(),ca,1800,1080);
    require(dpi->setContentScaleFactor(1.f)==kResultOk,"DPI reset");pumpGui(80);geom(a.get(),ca,1200,720);
    fb->deny=true;load(1.,0);geom(a.get(),ca,960,576);geom(b.get(),cb,1200,720);saved(1.,0);
    fb->deny=false;load(1.,0);geom(b.get(),cb,960,576);
    require(handler->dirtyCalls==0,"no dirty on recall/DPI/rejection");
    clickGui(cb,508,32);saved(1.,1);require(handler->dirtyCalls==1,"peer help dirty notification");
    clickGui(ca,508,32);saved(1.,0);require(handler->dirtyCalls==2,"shared peer help off");
    ViewRect zoom{0,0,1200,720};
    require(fa->resizeView(a.get(),&zoom)==kResultOk,"user resize");pumpGui(100);
    geom(a.get(),ca,1200,720);geom(b.get(),cb,1200,720);saved(1.25,0);
    require(handler->dirtyCalls==3,"user zoom dirty exactly once");
    // Noninteger DPI using the actual SDK pixel rounding; retain user zoom.
    for(double z:{.75,1.,1.5}){
        load(z,0);
        require(dpi->setContentScaleFactor(1.28f)==kResultOk,"fractional DPI accepted");pumpGui(80);
        const int width=static_cast<int>(std::floor(960*z*double(1.28f)));
        const int height=static_cast<int>(std::floor(576*z*double(1.28f)));
        geom(a.get(),ca,width,height);saved(z,0);
        fa->deny=true;
        require(dpi->setContentScaleFactor(1.37f)!=kResultOk,"fractional DPI rejected");pumpGui(60);
        geom(a.get(),ca,width,height);saved(z,0);fa->deny=false;
        require(dpi->setContentScaleFactor(1.f)==kResultOk,"fractional DPI reset");pumpGui(80);
        geom(a.get(),ca,int(960*z),int(576*z));saved(z,0);
    }
    // M01: recall while fractional DPI is already active, including
    // endpoint 150%, exact logical zoom, and reverse direction.
    load(1.,0);
    require(dpi->setContentScaleFactor(1.37f)==kResultOk,"M01 fractional DPI accepted");pumpGui(80);
    geom(a.get(),ca,1315,789);saved(1.,0);
    load(1.5,0);geom(a.get(),ca,1972,1183);geom(b.get(),cb,1440,864);saved(1.5,0);
    load(.75,0);geom(a.get(),ca,986,591);geom(b.get(),cb,720,432);saved(.75,0);
    load(1.,0);geom(a.get(),ca,1315,789);saved(1.,0);
    require(dpi->setContentScaleFactor(1.f)==kResultOk,"M01 fractional DPI reset");pumpGui(80);
    geom(a.get(),ca,960,576);saved(1.,0);
    require(handler->dirtyCalls==3,"M01 no dirty");
    std::cout<<"NATIVE_P009_FRACTIONAL_RECALL_M01_PASS\n";
    // The host is allowed to omit onSize for a same-pixel recall.
    // Both views must adopt the exact zoom, even without any host request.
    fa->omitSameSizeAck=true;fb->omitSameSizeAck=true;
    const auto sameRequestsA=fa->requests,sameRequestsB=fb->requests;
    load(1.0008,0);geom(a.get(),ca,960,576);geom(b.get(),cb,960,576);saved(1.0008,0);
    require(fa->requests==sameRequestsA&&fb->requests==sameRequestsB,"same-pixel recall is local");
    // Verify both independently via their next physical DPI request.
    FUnknownPtr<IPlugViewContentScaleSupport> peerDpi(b.get());
    require(peerDpi.getInterface()!=nullptr,"same-pixel peer DPI interface");
    require(dpi->setContentScaleFactor(2.f)==kResultOk&&
        peerDpi->setContentScaleFactor(2.f)==kResultOk,"same-pixel recall next DPI");
    pumpGui(80);geom(a.get(),ca,1921,1152);geom(b.get(),cb,1921,1152);saved(1.0008,0);
    // Reverse same-pixel recall at 100% DPI and verify zoom becomes 1.
    require(dpi->setContentScaleFactor(1.f)==kResultOk&&
        peerDpi->setContentScaleFactor(1.f)==kResultOk,"same-pixel DPI return");
    pumpGui(80);load(1.,0);
    require(dpi->setContentScaleFactor(2.f)==kResultOk&&
        peerDpi->setContentScaleFactor(2.f)==kResultOk,"same-pixel reverse next DPI");
    pumpGui(80);geom(a.get(),ca,1920,1152);geom(b.get(),cb,1920,1152);saved(1.,0);
    require(dpi->setContentScaleFactor(1.f)==kResultOk&&
        peerDpi->setContentScaleFactor(1.f)==kResultOk,"same-pixel final DPI return");
    pumpGui(80);geom(a.get(),ca,960,576);geom(b.get(),cb,960,576);
    fa->omitSameSizeAck=false;fb->omitSameSizeAck=false;
    require(handler->dirtyCalls==3,"same-pixel recall not dirty");
    std::cout<<"NATIVE_P009_SAME_PIXEL_RECALL_NO_ACK_PASS\n";
    load(1.25,0);
    const auto reopenPeer=[&](){
        DestroyWindow(hb);hb=makeHost();fb=owned(new GuiFrame(hb));
        b=owned(n.controller->createView(ViewType::kEditor));
        require(b&&b->setFrame(fb.get())==kResultOk&&b->attached(hb,kPlatformTypeHWND)==kResultOk,"reopen peer");
        ShowWindow(hb,SW_SHOWNA);pumpGui(100);cb=GetWindow(hb,GW_CHILD);require(cb!=nullptr,"reopened peer child");
        geom(b.get(),cb,960,576);
    };
    const auto oldPeerRequests=fb->requests;
    fa->intercept=[&](IPlugView* v,ViewRect* r){
        require(v==a.get()&&b->removed()==kResultOk&&b->setFrame(nullptr)==kResultOk,"reentrant later view detach");
        return fa->resizeView(v,r);
    };
    load(1.,0);
    require(!IsWindow(cb)&&fb->requests==oldPeerRequests,"removed later peer skipped");
    geom(a.get(),ca,960,576);reopenPeer();
    handler->dirtyIntercept=[&]{
        require(b->removed()==kResultOk&&b->setFrame(nullptr)==kResultOk,"dirty notification peer detach");
    };
    clickGui(ca,508,32);
    require(!IsWindow(cb)&&handler->dirtyCalls==4,"dirty detach exactly once");
    saved(1.,1);reopenPeer();
    std::cout<<"NATIVE_P009_LATER_VIEW_DIRTY_DETACH_PASS\n";
    load(1.25,0);
    // Synchronous host detach of the first view during its own resize
    // notification. Later registered peer must still receive the recall.
    const auto beforePeer=fb->requests;
    fa->intercept=[&](IPlugView* v,ViewRect*){
        require(v==a.get()&&v->removed()==kResultOk&&v->setFrame(nullptr)==kResultOk,"reentrant current view detach");
        return kResultFalse;
    };
    load(1.,1);
    require(!IsWindow(ca)&&fb->requests>beforePeer,"reentrant detach preserves later peer");
    geom(b.get(),cb,960,576);saved(1.,1);
    std::cout<<"NATIVE_P009_FRACTIONAL_DPI_REENTRANT_DETACH_PASS\n";
    DestroyWindow(ha);pumpGui(80);require(!IsWindow(ca)&&IsWindow(cb),"only first child closed");
    // Fresh current view for M03. Host detaches from the actual DPI
    // resize callback, not a preferences notification.
    ha=makeHost();fa=owned(new GuiFrame(ha));
    a=owned(n.controller->createView(ViewType::kEditor));
    FUnknownPtr<IPlugViewContentScaleSupport> detachDpi(a.get());
    require(a&&detachDpi.getInterface()&&a->setFrame(fa.get())==kResultOk&&
        a->attached(ha,kPlatformTypeHWND)==kResultOk,"M03 fresh current editor");
    ShowWindow(ha,SW_SHOWNA);pumpGui(80);ca=GetWindow(ha,GW_CHILD);
    require(ca!=nullptr,"M03 native child");geom(a.get(),ca,960,576);
    bool detached=false;
    fa->intercept=[&](IPlugView* v,ViewRect*){
        require(v==a.get()&&v->removed()==kResultOk,"M03 synchronous DPI detach");
        require(!IsWindow(ca)&&IsWindow(ha),"M03 child gone at removed return");
        require(v->setFrame(nullptr)==kResultOk,"M03 host frame cleared");
        require(DestroyWindow(ha)!=0&&!IsWindow(ha),"M03 parent destroyed inside host resize");
        detached=true;
        return kResultFalse;
    };
    require(detachDpi->setContentScaleFactor(1.37f)!=kResultOk&&detached,"M03 DPI returns rejection after detach");
    pumpGui(80);require(!IsWindow(ca)&&!IsWindow(ha)&&IsWindow(cb),"M03 parent and child gone, peer survives");
    load(1.25,1);geom(b.get(),cb,1200,720);saved(1.25,1);
    load(1.,1);geom(b.get(),cb,960,576);saved(1.,1);
    require(handler->dirtyCalls==4,"surviving view recall not dirty");
    std::cout<<"NATIVE_P009_DPI_DETACH_M03_PASS\n";
    require(b->removed()==kResultOk&&b->setFrame(nullptr)==kResultOk,"second close");DestroyWindow(hb);
    require(n.controller->setComponentHandler(nullptr)==kResultOk,"clear live handler");
    UnregisterClassW(wc.lpszClassName,inst);
    std::cout<<"NATIVE_P009_LIVE_RECALL_PEER_DPI_DIRTY_PASS\n";
}

#endif
// P009 native counterexamples: use actual loaded processor/controller.
class FaultStream final:public MemoryStream {
public:
    FaultStream(void* bytes,TSize size,int fail):MemoryStream(bytes,size),failAt(fail){}
    explicit FaultStream(int fail):failAt(fail){}
    tresult PLUGIN_API read(void* buffer,int32 count,int32* actual) override{
        const auto result=MemoryStream::read(buffer,count,actual);
        return call++==failAt?kResultFalse:result;
    }
    tresult PLUGIN_API write(void* buffer,int32 count,int32* actual) override{
        const auto result=MemoryStream::write(buffer,count,actual);
        return call++==failAt?kResultFalse:result;
    }
private:
    int failAt,call=0;
};
void auditRegressions(const VST3::Hosting::Module::Ptr& module){
    // Tempo changes while a malformed meter is advertised; compare an
    // otherwise identical valid-flag render and an unchanged-tempo control.
    const auto tempoRun=[&](bool update,bool badMeter){
        Native n(module);n.start(48000,kSample64,2);setConstant(n,4,1,1,0,0);
        n.render<double>(512);n.clock.tempo=update?240:120;
        if(badMeter)n.clock.timeSigDenominator=0;
        return n.render<double>(512);
    };
    const auto validTempo=tempoRun(true,false),oldTempo=tempoRun(false,false);
    require(validTempo!=oldTempo&&tempoRun(true,true)==validTempo,"P009 independent tempo flag");
    const auto meterRun=[&](bool update,bool badTempo){
        Native n(module);n.start(48000,kSample64,2);
        ParameterChanges p(14);point(p,103,0,0);point(p,107,0,0);
        point(p,108,0,0);point(p,109,0,0);point(p,111,0,1);n.flush(p,kSample64);
        n.clock.timeSigNumerator=update?3:4;
        if(badTempo)n.clock.tempo=std::numeric_limits<double>::quiet_NaN();
        std::vector<double> pcm;
        for(int i=0;i<25088;i+=512){auto b=n.render<double>(512);pcm.insert(pcm.end(),b.begin(),b.end());}
        return pcm;
    };
    const auto validMeter=meterRun(true,false),oldMeter=meterRun(false,false);
    require(validMeter[18100]==1&&oldMeter[18100]==0&&meterRun(true,true)==validMeter,"P009 independent meter flag");
    Native n(module);
    auto baseline=n.save(),changed=baseline;const double newWidth=.25;
    std::memcpy(changed.data()+8+7*8,&newWidth,8);
    for(int i=0;i<16;++i){
        FaultStream read(changed.data(),static_cast<TSize>(changed.size()),i);
        require(n.component->setState(&read)!=kResultOk&&n.save()==baseline,"P009 component read error rollback");
        FaultStream echo(changed.data(),static_cast<TSize>(changed.size()),i);
        require(n.controller->setComponentState(&echo)!=kResultOk&&n.controller->getParamNormalized(107)==.5,"P009 controller read error rollback");
        FaultStream write(i);
        require(n.component->getState(&write)!=kResultOk,"P009 component write error");
    }
    MemoryStream ui;require(n.controller->getState(&ui)==kResultOk&&ui.getSize()==20,"P009 UI baseline");
    std::vector<char> uiBytes(ui.getData(),ui.getData()+20),newUI=uiBytes;
    const double zoom=1.25;std::memcpy(newUI.data()+8,&zoom,8);
    for(int i=0;i<4;++i){
        FaultStream read(newUI.data(),static_cast<TSize>(newUI.size()),i);
        require(n.controller->setState(&read)!=kResultOk,"P009 UI read error");
        MemoryStream after;require(n.controller->getState(&after)==kResultOk&&after.getSize()==20&&
            std::memcmp(after.getData(),uiBytes.data(),20)==0,"P009 UI read rollback");
        FaultStream write(i);require(n.controller->getState(&write)!=kResultOk,"P009 UI write error");
    }
    require(n.load(changed)&&n.save()==changed,"P009 positive state roundtrip");
    n.start(48000,kSample64,2);
    SignalBlock b(n,32);b.l.fill(.75);b.r.fill(-.25);
    b.process(n);require(b.ol[0]==.75&&b.orr[0]==-.25,"P009 positive active buses");
    const auto restart=[&](bool in,bool out){
        n.stop();require(n.component->activateBus(kAudio,kInput,0,in)==kResultOk&&
            n.component->activateBus(kAudio,kOutput,0,out)==kResultOk,"P009 configure active buses");
        require(n.component->setActive(true)==kResultOk&&n.processor->setProcessing(true)==kResultOk,"P009 bus restart");
        n.running=true;
    };
    restart(false,true);b.process(n);
    require(b.o.silenceFlags==3&&std::all_of(b.ol.begin(),b.ol.begin()+32,[](double x){return x==0;})&&
        std::all_of(b.orr.begin(),b.orr.begin()+32,[](double x){return x==0;}),"P009 inactive input sentinel");
    restart(true,false);b.ol.fill(9);b.orr.fill(-9);b.o.silenceFlags=1;b.process(n);
    require(b.o.silenceFlags==1&&std::all_of(b.ol.begin(),b.ol.end(),[](double x){return x==9;})&&
        std::all_of(b.orr.begin(),b.orr.end(),[](double x){return x==-9;}),"P009 inactive output untouched");
    restart(true,true);b.process(n);
    require(b.ol[0]==.75&&b.orr[0]==-.25&&b.o.silenceFlags==0,"P009 bus reactivation");
    n.stop();
    ProcessSetup invalid{};invalid.processMode=kRealtime;invalid.symbolicSampleSize=kSample64;
    invalid.maxSamplesPerBlock=512;invalid.sampleRate=std::numeric_limits<double>::max();
    require(n.processor->setupProcessing(invalid)!=kResultOk,"P009 extreme sample rate rejected");
    require(n.component->setActive(true)==kResultOk&&n.processor->setProcessing(true)==kResultOk,"P009 previous setup preserved");
    n.running=true;b.process(n);require(b.ol[0]==.75&&b.orr[0]==-.25,"P009 post-rejection PCM");
    std::cout<<"NATIVE_P009_CLOCK_STREAM_BUS_RATE_PASS\n";
}

// Port the archived independent Codex malformed-prefix and lifecycle probes
// to the actual compiled module. Contract: one instance retains the last
// valid clock across missing context, stop/start and re-setup; a new
// instance starts at 120 BPM 4/4. Phase resets on reactivation only.
void clockLifecycleAndQueue(const VST3::Hosting::Module::Ptr& module){
    Native n(module);n.start(48000,kSample64,2);
    setConstant(n,2,1,1,0,0);n.clock.tempo=240;n.clock.timeSigNumerator=3;
    auto first=n.render<double>(512);
    require(std::abs(first[511]-511.*4/48000)<1e-9,"clock analytic 240 BPM");
    require(n.processor->setProcessing(false)==kResultOk&&n.processor->setProcessing(true)==kResultOk,"clock transport pause");
    auto continued=n.render<double>(512,nullptr,false);
    require(std::abs(continued[511]-1023.*4/48000)<1e-9,"clock phase survives processing pause");
    n.stop();n.start(48000,kSample64,2);
    auto retained=n.render<double>(512,nullptr,false);
    require(retained==first,"clock survives reactivation without context, phase restarts");
    auto updated=n.render<double>(512);
    require(std::abs(updated[511]-(512.*4+511.*2)/48000)<1e-9,"valid new clock changes phase rate");
    Native fresh(module);fresh.start(48000,kSample64,2);setConstant(fresh,2,1,1,0,0);
    auto defaultClock=fresh.render<double>(512,nullptr,false);
    require(std::abs(defaultClock[511]-511.*2/48000)<1e-9&&defaultClock!=retained,"fresh instance default clock");
    // Reproduce exact valid-prefix policy from Codex extra.cpp.
    Native q(module);q.start(48000,kSample64,2);
    ParameterChanges points(14);
    point(points,110,3,.75);point(points,110,7,std::numeric_limits<double>::quiet_NaN());
    SignalBlock b(q,16);b.l.fill(1);b.r.fill(-.5);b.data.inputParameterChanges=&points;
    require(q.processor->process(b.data)!=kResultOk,"malformed later queue reports error");
    auto state=q.save();double mix=0;std::memcpy(&mix,state.data()+8+10*8,8);
    require(mix==.75&&b.ol[3]==1,"malformed later queue retains valid prefix");
    b.data.inputParameterChanges=nullptr;b.process(q);
    require(b.ol[0]==1&&b.orr[0]==-.5,"subsequent valid process recovers");
    std::cout<<"NATIVE_P009_CLOCK_LIFECYCLE_MALFORMED_QUEUE_PASS\n";
}

#ifdef _WIN32
// Actual module attaches to a native HWND, then detaches repeatedly.
// A separate process with a deliberately damaged resource must reject
// attach; no alteration of the staged release candidate.
void resourceAndLifecycle(const VST3::Hosting::Module::Ptr& module,bool expectFailure){
    Native n(module);auto handler=owned(new GuiHandler);
    require(n.controller->setComponentHandler(handler.get())==kResultOk,"resource handler");
    const auto inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=inst;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"ChopperatorResourceAudit";
    require(RegisterClassW(&wc)!=0,"resource host class");
    const auto before=n.save();
    for(int cycle=0;cycle<(expectFailure?1:20);++cycle){
        HWND host=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Resource audit",WS_OVERLAPPEDWINDOW,
            0,0,990,640,nullptr,nullptr,inst,nullptr);
        require(host!=nullptr,"resource host");
        auto frame=owned(new GuiFrame(host));auto view=owned(n.controller->createView(ViewType::kEditor));
        require(view&&view->setFrame(frame.get())==kResultOk,"resource editor");
        const auto result=view->attached(host,kPlatformTypeHWND);
        if(expectFailure){
            require(result!=kResultOk,"corrupt resource must reject editor attach");
            require(GetWindow(host,GW_CHILD)==nullptr&&frame->requests==0,"rejected resource no child or resize");
        }
        else {
            require(result==kResultOk&&GetWindow(host,GW_CHILD)!=nullptr,"lifecycle native attach");
            pumpGui(25);
            require(view->removed()==kResultOk,"lifecycle native detach");
            pumpGui(20);
            require(GetWindow(host,GW_CHILD)==nullptr,"native child destroyed");
        }
        require(view->setFrame(nullptr)==kResultOk,"resource clear frame");
        DestroyWindow(host);
        require(n.save()==before,"resource/lifecycle component state unchanged");
    }
    require(n.controller->setComponentHandler(nullptr)==kResultOk,"resource clear handler");
    UnregisterClassW(wc.lpszClassName,inst);
    std::cout<<(expectFailure?"NATIVE_P009_CORRUPT_RESOURCE_REJECT_PASS":"NATIVE_P009_LIFECYCLE_20_PASS")<<"\n";
}

#else
#include "LinuxEditorProbe.hpp"
#endif

int main(int argc,char** argv){
    require(argc==2||argc==3||argc==4,"bundle argument");
#ifdef _WIN32
    struct ComScope{HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);~ComScope(){if(SUCCEEDED(hr))CoUninitialize();}} com;
    require(SUCCEEDED(com.hr),"COM apartment");
#endif
    std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);
    if(!module){std::cerr<<error<<'\n';return 1;}
    HostApplication host;PluginContextFactory::instance().setPluginContext(&host);
    module->getFactory().setHostContext(&host);
    #ifdef _WIN32
    if(argc==4){if(std::string(argv[2])=="--editor"||std::string(argv[2])=="--preview"){guiContract(module,argv[3],std::string(argv[2])=="--preview");if(std::string(argv[2])=="--editor")liveUIAudit(module);return 0;}require(std::string(argv[2])=="--programme","programme argument");programme(module,argv[3]);return 0;}
    if(argc==3){
        const std::string mode=argv[2];
        if(mode=="--resource-positive"||mode=="--resource-negative"){resourceAndLifecycle(module,mode=="--resource-negative");return 0;}
        require(mode=="--negative-spectrum","negative control argument");spectral(module,true);return 2;
    }
    #else
    if(argc==4){
        const std::string mode=argv[2];
        if(mode=="--editor"||mode=="--resource-negative"){linuxEditorProbe(module,argv[3],mode=="--resource-negative");return 0;}
        require(mode=="--programme","programme argument");programme(module,argv[3]);return 0;
    }
    if(argc==3){require(std::string(argv[2])=="--negative-spectrum","negative control argument");spectral(module,true);return 2;}
    #endif
    auditRegressions(module);clockLifecycleAndQueue(module);
    waveformContracts(module);spectral(module,false);realtimeProbe(module);
    timing<float>(module,48000,2);timing<double>(module,48000,1);
    timing<float>(module,96000,1);timing<double>(module,192000,2);
    std::cout<<"NATIVE_ACTIVE_MIX_BYPASS_4X_PASS\n";
    Native n(module);n.start(48000,kSample32,2);
    ParameterChanges p(14);
    point(p,103,0,0);point(p,108,0,0);point(p,109,0,0);point(p,111,0,1);
    n.flush(p);
    auto state=n.save();
    const auto pcm=trajectory(n);
    int open=0,closed=0;
    for(int i=0;i<24000;++i){
        const double x=double(i)/24000;
        const double integral=x<=.5?2*x+14*x*x:4.5+16*(x-.5)-14*(x-.5)*(x-.5);
        const double cycles=.25*integral,phase=cycles-std::floor(cycles);
        if(phase>.01&&phase<.49){require(pcm[i]==1,"trajectory open");++open;}
        if(phase>.51&&phase<.99){require(pcm[i]==0,"trajectory closed");++closed;}
    }
    require(open>1000&&closed>1000,"trajectory nontrivial");
    for(int i=24024;i<25024;++i)require(pcm[i]==1,"one-shot completion dry");
    n.stop();
    require(n.load(state),"restore saved valid state");
    for(std::size_t size=0;size<state.size();++size){
        std::vector<char> bad(state.begin(),state.begin()+static_cast<std::ptrdiff_t>(size));
        require(!n.load(bad)&&n.save()==state,"truncated state rollback");
    }
    auto invalid=state;const double nan=std::numeric_limits<double>::quiet_NaN();
    std::memcpy(invalid.data()+8,&nan,sizeof(nan));
    require(!n.load(invalid)&&n.save()==state,"nonfinite state rollback");
    Native fresh(module);require(fresh.load(state),"fresh state load");
    MemoryStream controllerState(state.data(),static_cast<TSize>(state.size()));
    require(fresh.controller->setComponentState(&controllerState)==kResultOk,"controller component state");
    require(fresh.controller->getParamNormalized(103)==0&&fresh.controller->getParamNormalized(111)==1,"controller recall");
    String128 name{};char text[128]{};
    require(fresh.controller->getParamStringByValue(103,1,name)==kResultOk,"length display");
    UString(name,128).toAscii(text,128);require(std::string(text)=="64 Bars","length endpoint text");
    fresh.start(48000,kSample32,2);
    require(trajectory(fresh)==pcm,"fresh native instance exact recall PCM");
    fresh.stop();std::cout<<"NATIVE_TRAJECTORY_STATE_RECALL_PASS open="<<open<<" closed="<<closed<<"\n";
}
