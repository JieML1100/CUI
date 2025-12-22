#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <Windows.h>
#include <functional>
#ifndef PROPERTY
#define PROPERTY(t,n) __declspec( property (put = Set##n, get = Get##n)) t n
#define READONLY_PROPERTY(t,n) __declspec( property (get = Get##n) ) t n
#define WRITEONLY_PROPERTY(t,n) __declspec( property (put = Set##n) ) t n
#define GET(t,n) t Get##n() 
#define SET(t,n) void Set##n(t value)
#define PROPERTY_CPP(t,n) __declspec( property (put = Set##n, get = Get##n)); t nt Get##n();t Get##n();
#define GET_CPP(c,t,n) t c::Get##n() 
#define SET_CPP(c,t,n) void c::Set##n(t value)
#define EPROPERTY_R(t,n)READONLY_PROPERTY(t, n);GET(t, n)
#endif 
#ifndef typeof
#define typeof(x) decltype(x)
#endif

#ifndef M_E        
#define M_E        2.71828182845904523536   
#endif
#ifndef M_LOG2E    
#define M_LOG2E    1.44269504088896340736   
#endif
#ifndef M_LOG10E   
#define M_LOG10E   0.434294481903251827651  
#endif
#ifndef M_LN2      
#define M_LN2      0.693147180559945309417  
#endif
#ifndef M_LN10     
#define M_LN10     2.30258509299404568402   
#endif
#ifndef M_PI       
#define M_PI       3.14159265358979323846   
#endif
#ifndef M_PI_2     
#define M_PI_2     1.57079632679489661923   
#endif
#ifndef M_PI_4     
#define M_PI_4     0.785398163397448309616  
#endif
#ifndef M_1_PI     
#define M_1_PI     0.318309886183790671538  
#endif
#ifndef M_2_PI     
#define M_2_PI     0.636619772367581343076  
#endif
#ifndef M_2_SQRTPI 
#define M_2_SQRTPI 1.12837916709551257390   
#endif
#ifndef M_SQRT2    
#define M_SQRT2    1.41421356237309504880   
#endif
#ifndef M_SQRT1_2  
#define M_SQRT1_2  0.707106781186547524401  
#endif
template <typename T>
class Property {
public:
    template <typename Getter, typename Setter>
    Property(Getter getter, Setter setter)
        : getter_(getter), setter_(setter) {}
    auto get() {
        return getter_();
    }
    operator T() const {
        return getter_();
    }
    void operator=(const T& value) {
        setter_(value);
    }
private:
    std::function<T()> getter_;
    std::function<void(T)> setter_;
};
template <typename T>
class ReadOnlyProperty {
public:
    template <typename Getter>
    ReadOnlyProperty(Getter getter)
        : getter_(getter) {}
    operator T() const {
        return getter_();
    }
    auto get() {
        return getter_();
    }
private:
    std::function<T()> getter_;
};