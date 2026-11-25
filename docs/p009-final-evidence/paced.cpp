#define main original_audit_main
#include "../p008-codex-evidence/probe.cpp"
#undef main

constexpr double pi=std::numbers::pi;
#include "paced-oracle.inc"

int main() {
    Host h;
    set(h,4,1.,.5,true);
    ParameterChanges setup(14);
    for(auto [id,v]:{std::pair{100u,0.},std::pair{101u,1.},std::pair{102u,0.},
                     std::pair{103u,1.},std::pair{104u,.01},std::pair{105u,1.}})
        point(setup,id,0,v);
    h.flush(setup);
    Block b(128);
    for(int j=0;j<128;++j){
        b.l[j]=.25*std::sin(2*pi*440*j/48000);
        b.r[j]=-.5*b.l[j];
    }
    PacedOracle oracle;
    double input=0,error=0,frozen=0,constantPower=0,constantDelta=0;
    for(std::size_t i=0;i<384;++i){
        ParameterChanges params(14);
        point(params,104,127,i%2?.01:.99);
        point(params,105,127,i%2?0.:1.);
        point(params,107,127,i%2?.2:.9);
        b.d.inputParameterChanges=&params;
        b.run(h);
        for(int j=0;j<128;++j){
            const auto want=b.l[j]*oracle.tick(i,j);
            error+=std::pow(b.ol[j]-want,2);
            frozen+=std::pow(.5*b.l[j]-want,2);
            input+=std::pow(b.l[j],2);
            constantPower+=std::pow(.5*b.l[j],2);
            constantDelta+=std::pow(b.l[j]-.5*b.l[j],2);
            require(b.orr[j]==-.5*b.ol[j]);
        }
    }
    std::cout<<std::setprecision(14)<<"N06 relative squared PCM error="<<error/input
             <<" constant-gain negative="<<frozen/input<<" samples=49152\n";
    std::cout<<"N06 former energy ratios for constant gain: "<<constantPower/input
             <<" and "<<constantDelta/input<<'\n';
    return input>0&&error/input<1e-10&&frozen/input>.01&&
           constantPower/input==.25&&constantDelta/input==.25 ? 0 : 1;
}
