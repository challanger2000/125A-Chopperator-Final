#include "Parameters.hpp"
#include "ValueEntry.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace chopper;
void check(bool yes,const char* label){if(!yes){std::cerr<<label<<'\n';std::exit(1);}}
bool near(double a,double b,double tol=1e-7){return std::abs(a-b)<tol;}
int main(){
    Parameters p;
    check(valid(p.settings())&&p.settings().bars==2&&p.settings().mid==16&&!p.active(),"defaults");
    for(int i=0;i<66;++i){check(p.set(Param::length,double(i)/65),"length setter");
        const double expected=i==0?.25:i==1?.5:double(i-1);
        check(p.settings().bars==expected,"all 66 lengths");}
    for(auto id:{Param::start,Param::mid,Param::end})for(int i=0;i<7;++i){
        check(p.set(id,double(i)/6),"rate setter");const auto s=p.settings();
        check((id==Param::start?s.start:id==Param::mid?s.mid:s.end)==(1<<i),"all divisions");}
    check(p.set(Param::shape,.375)&&p.settings().shape==Shape::sawDown,"discrete midpoint rounding");
    check(!p.set(Param::width,std::numeric_limits<double>::infinity())&&p.settings().width==.5,"invalid parameter rollback");
    check(!p.set(static_cast<Param>(99),.5),"invalid ID");
    // 120 BPM, quarter note cycle = 0.5 seconds. 20% attack = 100 ms;
    // 10% release = 50 ms. At 50% width both slopes reach their endpoints.
    for(double sr:{44100.,48000.,96000.,192000.}){
        Settings s;s.start=s.mid=s.end=4;s.bars=4;s.attack=.2;s.release=.1;
        Core c;check(c.prepare(sr)&&c.configure(s)&&c.setClock(120,4,4),"envelope setup");c.setActive(true);
        const int n=static_cast<int>(sr*.5),fall=static_cast<int>(sr*.05),rise=static_cast<int>(sr*.1);
        for(int i=0;i<n*2;++i){
            const double g=c.tick();
            // +/- one sample for floating-point phase at waveform edge.
            if(i==n/2+fall/2)check(near(g,.5,2.0/fall),"release slope");
            if(i==n+rise/2)check(near(g,.5,2.0/rise),"attack slope");
            if(i==n/2+fall+2)check(g==0,"closed endpoint");
            if(i==n+rise+2)check(g==1,"open endpoint");
        }
    }
    // Longest duration, 64 bars at 120 BPM in 4/4 = exactly 128 seconds.
    Settings s;s.bars=64;s.start=s.mid=s.end=16;s.loop=false;
    Core c;check(c.prepare(48000)&&c.configure(s),"64 bar setup");c.setActive(true);
    for(int i=0;i<6144000;++i){if(i%96000==0)check(near(c.progress(),double(i)/6144000),"long bar progress");c.tick();}
    check(!c.running()&&c.progress()==1,"64 bar end");
    // Unusual valid SDK sample rates; finite bounded output.
    for(double sr:{1234.568,384000.,1234567.8}){
        Core u;check(u.prepare(sr)&&u.configure(s)&&u.setClock(120,7,8),"unusual sample rate");u.setActive(true);
        for(int i=0;i<2048;++i){const double g=u.tick();check(g>=0&&g<=1,"unusual finite gain");}
    }
    // P009: reproduce the original malformed sign and stalled extreme setup.
    double parsed=.42;
    check(!parseEntry(Param::curve,"+-25%",parsed)&&parsed==.42,"double sign rejected");
    check(!parseEntry(Param::curve,"++25%",parsed)&&parsed==.42,"double plus rejected");
    check(parseEntry(Param::curve,"-25%",parsed)&&parsed==.375,"negative curve valid");
    check(parseEntry(Param::curve,"+25%",parsed)&&parsed==.625,"positive curve valid");
    Core running,reference;
    check(running.prepare(48000)&&reference.prepare(48000),"P009 prepare baseline");
    running.setActive(true);reference.setActive(true);
    for(int i=0;i<128;++i)check(running.tick()==reference.tick(),"P009 initial PCM");
    const double progress=running.progress(),phase=running.phase();
    check(!running.prepare(std::numeric_limits<double>::max())&&running.progress()==progress&&
          running.phase()==phase&&running.running(),"extreme setup atomic rejection");
    for(int i=0;i<4096;++i)check(running.tick()==reference.tick()&&running.phase()==reference.phase(),"P009 rollback exact PCM");
    std::cout<<"P009 parser and clock rollback PASS\n";
    std::cout<<"P002 parameter and envelope contract PASS\n";
}
