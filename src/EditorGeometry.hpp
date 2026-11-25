#pragma once
#include <algorithm>
#include <cmath>
namespace chopper {
struct EditorPixels { int width,height; };
inline EditorPixels editorPixels(double zoom,double scale) noexcept {
    return {static_cast<int>(std::floor(960.*zoom*scale)),
            static_cast<int>(std::floor(576.*zoom*scale))};
}
inline double editorZoomForWidth(int width,double scale) noexcept {
    if(width<=editorPixels(.75,scale).width)return .75;
    if(width>=editorPixels(1.5,scale).width)return 1.5;
    return std::clamp(width/(960.*scale),.75,1.5);
}
inline bool editorSizeValid(int width,int height,double scale) noexcept {
    return width>=editorPixels(.75,scale).width&&width<=editorPixels(1.5,scale).width&&
           std::abs(height-width*.6)<=1.01;
}
}
