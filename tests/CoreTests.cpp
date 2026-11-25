#include "ChopperCore.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
using namespace chopper;
void require(bool pass,const char* name){
    if(!pass){std::cerr<<"FAIL: "<<name<<'\n';std::exit(1);}
}
bool close(double a,double b,double eps=1e-7){return std::abs(a-b)<eps;}
double distance(double a,double b){return std::abs(std::remainder(a-b,1.0));}
int main(int argc,char**){
    if(argc>1)require(false,"intentional Release negative");
    Settings s;s.attack=s.release=0;
    require(close(rateAt(s,0),2)&&close(rateAt(s,.5),16)&&close(rateAt(s,1),2),"anchors");
    require(close(rateAt(s,.25),9)&&close(rateAt(s,.75),9),"linear rate");
    for(double curve:{-1.0,0.0,1.0}){
        s.curve=curve;s.position=.3;
        require(close(rateAt(s,.15),2+14*std::pow(.5,std::exp2(2*curve))),"curve");
        require(close(rateAt(s,.3),16),"midpoint");
    }
    s.curve=0;s.position=.5;
    for(double sr:{44100.0,48000.0,96000.0,192000.0}){
        Core c;require(c.prepare(sr)&&c.configure(s)&&c.setClock(120,4,4),"setup");
        c.setActive(true);
        const int n=static_cast<int>(sr*4);
        for(int i=0;i<n;++i){
            const double t=double(i)/n;
            const double expected=t<=.5?4*t+28*t*t:9+32*(t-.5)-28*(t-.5)*(t-.5);
            require(distance(c.phase(),expected)<2e-6,"phase integral per sample");
            const double g=c.tick();require(g>=0&&g<=1,"bounded gain");
        }
        require(!c.running()&&close(c.progress(),1),"one shot duration");
        for(int i=0;i<static_cast<int>(sr*.001)+1;++i)c.tick();
        require(c.tick()==1,"one shot dry");
        c.setActive(true);require(!c.running(),"no held retrigger");
        c.setActive(false);c.setActive(true);
        require(c.running()&&c.progress()==0&&c.phase()==0,"rising edge");
    }
    s.start=s.mid=s.end=8;s.bars=1;
    Core m;require(m.configure(s)&&m.setClock(120,3,4),"meter setup");m.setActive(true);
    for(int i=0;i<36000;++i)m.tick();
    require(close(m.progress(),.5)&&distance(m.phase(),0)<1e-6,"3/4 timing");
    require(m.setClock(60,3,4),"tempo change");
    for(int i=0;i<72000;++i)m.tick();
    require(!m.running(),"tempo duration");
    s.start=2;s.mid=4;s.end=8;s.bars=.25;s.loop=true;
    Core loop;require(loop.configure(s),"loop setup");loop.setActive(true);
    for(int i=0;i<24000;++i)loop.tick();
    require(loop.running()&&distance(loop.phase(),.125)<2e-6,"phase carry at loop");
    s.start=2;s.mid=16;s.end=2;s.bars=2;
    Core a,b;require(a.configure(s)&&b.configure(s),"partition setup");a.setActive(true);b.setActive(true);
    std::vector<float> l(20000,.25f),r(20000,-.5f),ll=l,rr=r;
    a.process(l.data(),r.data(),20000);
    for(int i=0;i<20000;){const int n=std::min(73+i%271,20000-i);b.process(ll.data()+i,rr.data()+i,n);i+=n;}
    require(l==ll&&r==rr,"partition PCM");
    bool closed=false,open=false;
    for(std::size_t i=0;i<l.size();++i){require(r[i]==-2*l[i],"stereo");closed|=l[i]==0;open|=l[i]==.25f;}
    require(closed&&open,"audible chop");
    s.mix=0;require(a.configure(s),"mix setup");
    std::array<double,3> dry{.123,-2.5,0},original=dry;
    a.process(dry.data(),static_cast<double*>(nullptr),3);require(dry==original,"exact dry");
    require(!a.setClock(std::numeric_limits<double>::quiet_NaN(),4,4),"invalid clock");
    s.width=std::numeric_limits<double>::quiet_NaN();require(!a.configure(s),"invalid settings");
    require(shapeAt(Shape::rectangle,.2,.5)==1&&shapeAt(Shape::rectangle,.6,.5)==0,"rectangle");
    require(close(shapeAt(Shape::sawUp,.25,.5),.5)&&close(shapeAt(Shape::sawDown,.125,.5),.75),"saws");
    require(close(shapeAt(Shape::triangle,.25,.5),1)&&close(shapeAt(Shape::sine,.25,.5),1),"triangle sine");
    require(shapeAt(Shape::rectangle,0,0)==0&&shapeAt(Shape::rectangle,.999,1)==1,"width endpoints");
    std::cout<<"Core contract PASS\n";
}
