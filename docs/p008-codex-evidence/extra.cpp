#define main prior_probe_main
#include "probe.cpp"
#undef main
void nonlinear() {
    constexpr double sr=44100,bpm=147;
    constexpr int frames=60000;
    auto run=[&](bool split){
        Host h(sr);set(h,1,1,1);
        ParameterChanges p(14);
        point(p,100,0,1./6);point(p,101,0,3./6);point(p,102,0,2./6);
        point(p,104,0,.7);point(p,105,0,.625);h.flush(p);
        auto cl=clock(bpm,7,8);
        Block b;std::vector<double> pcm;
        for(int i=0;i<frames;){
            auto size=std::min(frames-i,split?17+i%235:512);
            b.d.processContext=i==0?&cl:nullptr;b.d.numSamples=size;b.run(h);
            pcm.insert(pcm.end(),b.ol,b.ol+size);i+=size;
        }
        return pcm;
    };
    auto pcm=run(false), fragmented=run(true);
    using Real=long double;
    const Real mid=.01L+.98L*.7L,slope=std::sqrt(2.L),bars=.25L,quarters=3.5L;
    const auto endpoint=[&](Real at,Real before,Real after,Real segment){
        return segment*at*(before+(after-before)*std::pow(at,slope)/(1+slope));
    };
    const Real startPart=endpoint(1,2,8,mid);
    const Real full=startPart+endpoint(1,8,4,1-mid);
    double err=0;int compared=0;
    for(int i=0;i<frames;++i){
        Real barPosition=i*bpm/(60.L*sr*quarters*bars);
        Real whole=std::floor(barPosition), x=barPosition-whole;
        Real cycles=quarters*bars/4.L*(whole*full+
          (x<mid?endpoint(x/mid,2,8,mid):startPart+endpoint((x-mid)/(1-mid),8,4,1-mid)));
        Real phase=cycles-std::floor(cycles);
        if(phase<.08L||phase>.92L)continue;
        err=std::max(err,std::abs(pcm[i]-double(phase)));++compared;
    }
    std::cout<<"nonlinear samples compared="<<compared<<" maxError="<<err<<'\n';
    check("nonlinear 7/8 fractional loop PCM and byte-identical partitions",pcm==fragmented&&compared>40000&&err<1e-10);
}
void floatPcm(){
    Host h(96000,1,kSample32);
    float channel[]{std::numeric_limits<float>::max(),-3.25f,std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::infinity(),std::numeric_limits<float>::denorm_min(),-0.f};
    float* addr[]{channel};
    AudioBusBuffers a,b;
    a.numChannels=b.numChannels=1;a.channelBuffers32=b.channelBuffers32=addr;
    ProcessData d;d.numInputs=d.numOutputs=1;d.inputs=&a;d.outputs=&b;d.numSamples=6;
    const auto status=h.proc.process(d);
    check("float32 mono in-place invalid PCM and extremes",status==kResultOk && b.silenceFlags==0 &&
       channel[0]==std::numeric_limits<float>::max()&&channel[1]==-3.25f&&channel[2]==0.f&&channel[3]==0.f&&
       channel[4]==std::numeric_limits<float>::denorm_min()&&std::signbit(channel[5]));
}
void badQueue() {
    Host h;ParameterChanges points(14);
    point(points,110,3,.75);point(points,110,7,std::numeric_limits<double>::quiet_NaN());
    Block b(16);b.d.inputParameterChanges=&points;
    int result=h.proc.process(b.d);auto s=h.save();double mix=0;
    std::memcpy(&mix,s.data()+8+(110-100)*8,8);
    std::cout<<"invalid later lane result="<<result<<" prefix MIX="<<mix<<" PCM="<<b.ol[3]<<'\n';
    check("invalid lane point rejected retaining valid prefix per P003",result!=kResultOk&&mix==.75&&b.ol[3]==1);
}
int main(){std::cout<<std::setprecision(14);nonlinear();floatPcm();badQueue();return failures?1:0;}
