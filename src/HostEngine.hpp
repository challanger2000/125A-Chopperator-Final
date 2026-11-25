#pragma once
#include "Parameters.hpp"
#include <atomic>
namespace chopper {
// Single producer and single consumer; the consumer owns its front slot until
// the next read, the producer owns its back slot until publication.
template<class T> class Latest {
public:
    void publish(const T& value) noexcept {
        slots_[back_]=value;
        back_=middle_.exchange(back_|4u,std::memory_order_acq_rel)&3u;
    }
    T read() noexcept {
        if(middle_.load(std::memory_order_acquire)&4u)
            front_=middle_.exchange(front_,std::memory_order_acq_rel)&3u;
        return slots_[front_];
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::array<T,3> slots_{};
    std::atomic<unsigned> middle_{1};
    unsigned front_{0},back_{2};
};
struct Snapshot { Parameters params{}; std::uint64_t generation{0}; };
class HostEngine {
public:
    bool prepare(double sr) noexcept {
        if(!core_.prepare(sr))return false;
        sr_=sr;adopt();
        core_.setActive(false);apply();
        wet_=targetWet();
        return true;
    }
    // Host/UI thread only.
    void restore(const Parameters& params) noexcept {
        ++restore_.generation;restore_.params=params;incoming_.publish(restore_);
    }
    Parameters save() noexcept {
        const auto current=outgoing_.read();
        return current.generation<restore_.generation?restore_.params:current.params;
    }
    // Processing owner only (or setup when processing is stopped).
    void adopt() noexcept {
        const auto current=incoming_.read();
        if(current.generation!=applied_.generation){
            applied_=current;core_.setActive(false);apply();dirty_=true;
        }
    }
    void publish() noexcept {
        if(dirty_){outgoing_.publish(applied_);dirty_=false;}
    }
    const Parameters& parameters()const noexcept{return applied_.params;}
    bool set(Param id,double value) noexcept {
        const double previous=applied_.params.get(id);
        if(!applied_.params.set(id,value))return false;
        if(applied_.params.get(id)!=previous){apply();dirty_=true;}
        return true;
    }
    bool clock(double bpm,int n,int d) noexcept{return core_.setClock(bpm,n,d);}
    double tick() noexcept {
        const double g=core_.tick();
        const double target=targetWet();
        const double maxStep=1/std::max(1.0,.0005*sr_);
        const double delta=target-wet_;
        wet_=std::abs(delta)<=maxStep?target:wet_+std::copysign(maxStep,delta);
        return wet_==0?1:wet_==1?g:1-wet_+wet_*g;
    }
private:
    void apply() noexcept {
        auto s=applied_.params.settings();s.mix=1;
        core_.configure(s);core_.setActive(applied_.params.active());
    }
    double targetWet() const noexcept{return applied_.params.bypass()?0:applied_.params.get(Param::mix);}
    Latest<Snapshot> incoming_,outgoing_;
    Snapshot restore_{},applied_{};
    Core core_{};
    double sr_{48000},wet_{1};
    bool dirty_{false};
};
}
