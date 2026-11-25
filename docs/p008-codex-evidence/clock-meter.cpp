#define main prior_probe_main
#include "probe.cpp"
#undef main
int main(){
  for(int variant=0;variant<3;++variant) {
    Host h;set(h,0,1,0,false);
    auto context=clock(120,3,4);
    if(variant==0)context.state=ProcessContext::kTimeSigValid;
    if(variant==1)context.state=0;
    if(variant==2)context.tempo=std::numeric_limits<double>::quiet_NaN();
    Block b;
    double value=-1;
    for(int offset=0;offset<=18024;){
      int count=std::min(512,18025-offset);
      b.d.numSamples=count;b.d.processContext=offset==0?&context:nullptr;
      b.run(h);value=b.ol[count-1];offset+=count;
    }
    std::cout<<"meter variant "<<variant<<" sample 18024="<<value<<'\n';
    check(variant==0?"meter-only reaches dry at 18024":variant==1?"4/4 counterfactual remains closed":"valid meter survives NaN tempo",value==(variant==1?0.:1.));
  }
  return failures?1:0;
}
