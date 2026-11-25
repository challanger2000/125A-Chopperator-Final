#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace chopper {
enum class Shape { rectangle, sawUp, sawDown, triangle, sine };
struct Settings {
    int start{2}, mid{16}, end{2};
    double bars{2}, position{.5}, curve{0}, width{.5};
    double attack{.02}, release{.05}, mix{1};
    Shape shape{Shape::rectangle};
    bool loop{false};
};
inline bool valid(const Settings& s) noexcept {
    const auto unit=[](double x){return x>=0&&x<=1;};
    const auto rate=[](int n){return n>=1&&n<=64&&(n&(n-1))==0;};
    return rate(s.start)&&rate(s.mid)&&rate(s.end)
        &&(s.bars==.25||s.bars==.5||(s.bars>=1&&s.bars<=64&&std::floor(s.bars)==s.bars))
        &&s.position>=.01&&s.position<=.99&&s.curve>=-1&&s.curve<=1
        &&unit(s.width)&&unit(s.attack)&&unit(s.release)&&unit(s.mix)
        &&static_cast<int>(s.shape)>=0&&static_cast<int>(s.shape)<=4;
}
inline double segmentIntegral(double a,double b,double u,double k) noexcept {
    return a*u+(b-a)*(k==1?u*u:std::pow(u,k+1))/(k+1);
}
inline double rateAt(const Settings& s,double x) noexcept {
    x=std::clamp(x,0.0,1.0);
    const double k=std::exp2(2*s.curve);
    const auto p=[k](double u){return k==1?u:std::pow(u,k);};
    if(x<s.position)return s.start+(s.mid-s.start)*p(x/s.position);
    return s.mid+(s.end-s.mid)*p((x-s.position)/(1-s.position));
}
inline double integral(const Settings& s,double x) noexcept {
    x=std::clamp(x,0.0,1.0);
    const double k=std::exp2(2*s.curve);
    if(x<=s.position)return s.position*segmentIntegral(s.start,s.mid,x/s.position,k);
    return s.position*segmentIntegral(s.start,s.mid,1,k)
        +(1-s.position)*segmentIntegral(s.mid,s.end,(x-s.position)/(1-s.position),k);
}
inline double shapeAt(Shape shape,double phase,double width) noexcept {
    if(width<=0||phase>=width)return 0;
    const double u=phase/width;
    switch(shape){
        case Shape::rectangle:return 1;
        case Shape::sawUp:return u;
        case Shape::sawDown:return 1-u;
        case Shape::triangle:return 1-std::abs(2*u-1);
        case Shape::sine:return .5-.5*std::cos(2*std::numbers::pi*u);
    }
    return 0;
}
class Core {
public:
    bool prepare(double sampleRate) noexcept {
        if(!clockUsable(sampleRate,bpm_,quartersPerBar_))return false;
        sr_=sampleRate;active_=running_=false;
        progress_=phase_=correction_=0;gain_=1;return true;
    }
    bool configure(const Settings& s) noexcept {
        if(!valid(s))return false;
        settings_=s;return true;
    }
    bool setClock(double bpm,int numerator,int denominator) noexcept {
        if(!std::isfinite(bpm)||bpm<=0||numerator<=0||denominator<=0)return false;
        const double q=4.0*numerator/denominator;
        if(!clockUsable(sr_,bpm,q))return false;
        bpm_=bpm;quartersPerBar_=q;return true;
    }
    void setActive(bool active) noexcept {
        if(active&&!active_){progress_=phase_=correction_=0;running_=true;}
        if(!active)running_=false;
        active_=active;
    }
    double progress()const noexcept{return progress_;}
    double phase()const noexcept{return phase_;}
    bool running()const noexcept{return running_;}
    double tick()noexcept {
        const auto& s=settings_;
        double target=1,seconds=.0005;
        if(running_){
            target=shapeAt(s.shape,phase_,s.width);
            const double period=240/bpm_/rateAt(s,progress_);
            seconds=std::max(seconds,period*(target>gain_?s.attack:s.release));
        }
        const double step=1/std::max(1.0,seconds*sr_);
        const double delta=target-gain_;
        gain_=std::abs(delta)<=step?target:gain_+std::copysign(step,delta);
        if(running_)advance();
        return s.mix==0?1:s.mix==1?gain_:1-s.mix+s.mix*gain_;
    }
    template<class Sample>
    void process(Sample* left,Sample* right,int frames)noexcept {
        for(int i=0;i<frames;++i){
            const auto g=static_cast<Sample>(tick());
            if(g!=Sample{1}){left[i]*=g;if(right&&right!=left)right[i]*=g;}
        }
    }
private:
    // Validate the complete supported LENGTH range before mutating clock or
    // setup. Preserve the existing arithmetic for ordinary rates and tempo.
    static bool clockUsable(double sr,double bpm,double q) noexcept {
        if(!std::isfinite(sr)||sr<=0||!std::isfinite(bpm)||bpm<=0||
           !std::isfinite(q)||q<=0)return false;
        const double denMin=60*sr*q*.25,denMax=60*sr*q*64;
        if(!std::isfinite(denMin)||denMin<=0||!std::isfinite(denMax)||denMax<=0)return false;
        const double dxMin=bpm/denMax,dxMax=bpm/denMin,period=240/bpm;
        return std::isfinite(dxMin)&&dxMin>0&&std::isfinite(dxMax)&&dxMax>0&&
               std::isfinite(period)&&period>0&&std::isfinite(period*sr)&&
               std::isfinite((q*64/4)*(dxMax+1)*64);
    }
    void advance()noexcept {
        const double scale=quartersPerBar_*settings_.bars/4;
        const double dx=bpm_/(60*sr_*quartersPerBar_*settings_.bars);
        if(!std::isfinite(dx)||dx<=0)return;
        const double y=dx-correction_,next=progress_+y;
        if(!std::isfinite(next))return;
        correction_=(next-progress_)-y;
        double cycles;
        if(settings_.loop){
            const double whole=std::floor(next),remainder=next-whole;
            cycles=scale*(whole*integral(settings_,1)+integral(settings_,remainder)-integral(settings_,progress_));
            progress_=remainder;
        }else{
            const double stop=std::min(1.0,next);
            cycles=scale*(integral(settings_,stop)-integral(settings_,progress_));
            progress_=stop;
            if(next>=1-1e-15){progress_=1;running_=false;}
        }
        if(std::isfinite(cycles)){
            phase_=std::fmod(phase_+std::fmod(cycles,1.0),1.0);
            if(phase_<0)phase_+=1;
        }
    }
    Settings settings_{};
    double sr_{48000},bpm_{120},quartersPerBar_{4};
    double progress_{0},phase_{0},correction_{0},gain_{1};
    bool active_{false},running_{false};
};
}
