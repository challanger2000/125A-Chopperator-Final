#pragma once
#include "Parameters.hpp"
#include <charconv>
#include <string>
#include <string_view>
namespace chopper {
inline std::string_view trimEntry(std::string_view s){
    const auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos)return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);
}
inline bool entryNumber(std::string_view text,double& value){
    text=trimEntry(text);
    if(text.empty())return false;
    if(text.front()=='+'){
        text.remove_prefix(1);
        if(text.empty()||text.front()=='+'||text.front()=='-')return false;
    }
    if(text.empty())return false;
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::fixed);
    return result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&std::isfinite(value);
}
inline bool parseEntry(Param id,std::string text,double& normalized){
    for(char& c:text){if(c==',')c='.';else if(c>='A'&&c<='Z')c=char(c-'A'+'a');}
    auto s=trimEntry(text);double value=0;
    if(id==Param::start||id==Param::mid||id==Param::end){
        if(s.starts_with("1/"))s.remove_prefix(2);
        if(!entryNumber(s,value))return false;
        for(std::size_t i=0;i<divisions.size();++i)if(value==divisions[i]){normalized=double(i)/6;return true;}
        return false;
    }
    if(id==Param::length){
        if(s.ends_with("bars"))s.remove_suffix(4);
        else if(s.ends_with("bar"))s.remove_suffix(3);
        s=trimEntry(s);
        if(s=="1/4")value=.25;else if(s=="1/2")value=.5;
        else if(!entryNumber(s,value))return false;
        for(std::size_t i=0;i<lengths.size();++i)if(value==lengths[i]){normalized=double(i)/65;return true;}
        return false;
    }
    if(id!=Param::position&&id!=Param::curve&&id!=Param::width&&id!=Param::attack&&id!=Param::release&&id!=Param::mix)return false;
    if(s.ends_with("%"))s.remove_suffix(1);
    if(!entryNumber(s,value))return false;
    const double lo=id==Param::position?1.:id==Param::curve?-100.:0.;
    const double hi=id==Param::position?99.:100.;
    if(value<lo||value>hi)return false;
    normalized=(value-lo)/(hi-lo);return true;
}
}
