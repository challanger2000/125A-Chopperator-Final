#pragma once
#include "ChopperCore.hpp"
#include <array>
#include <cstdint>
namespace chopper {
// Stable IDs for initial VST3 mapping, independent of display order.
enum class Param : std::uint32_t { start=100, mid, end, length, position, curve,
    shape, width, attack, release, mix, active, mode, bypass };
inline constexpr std::size_t parameterCount=14;
inline constexpr std::array<int,7> divisions{1,2,4,8,16,32,64};
inline constexpr std::array<double,66> lengths=[] {
    std::array<double,66> values{};values[0]=.25;values[1]=.5;
    for(std::size_t i=2;i<values.size();++i)values[i]=static_cast<double>(i-1);
    return values;
}();
inline constexpr std::array<int,parameterCount> steps{6,6,6,65,0,0,4,0,0,0,0,1,1,1};
inline constexpr std::array<double,parameterCount> defaults{1.0/6,4.0/6,1.0/6,3.0/65,.5,.5,0,.5,.02,.05,1,0,0,0};
struct Parameters {
    bool set(Param id,double value) noexcept {
        const auto index=static_cast<std::uint32_t>(id)-100u;
        if(index>=parameterCount||!std::isfinite(value))return false;
        value=std::clamp(value,0.0,1.0);
        if(steps[index])value=std::round(value*steps[index])/steps[index];
        values[index]=value;return true;
    }
    double get(Param id) const noexcept {
        const auto index=static_cast<std::uint32_t>(id)-100u;
        return index<parameterCount?values[index]:0;
    }
    Settings settings() const noexcept {
        const auto d=[](double x){return divisions[static_cast<std::size_t>(std::lround(x*6))];};
        Settings s;
        s.start=d(values[0]);s.mid=d(values[1]);s.end=d(values[2]);
        s.bars=lengths[static_cast<std::size_t>(std::lround(values[3]*65))];
        s.position=.01+.98*values[4];s.curve=2*values[5]-1;
        s.shape=static_cast<Shape>(std::lround(values[6]*4));
        s.width=values[7];s.attack=values[8];s.release=values[9];s.mix=values[10];
        s.loop=values[12]>=.5;return s;
    }
    bool active() const noexcept{return values[11]>=.5;}
    bool bypass() const noexcept{return values[13]>=.5;}
private:
    std::array<double,parameterCount> values=defaults;
};
}
