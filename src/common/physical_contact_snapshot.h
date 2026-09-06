#pragma once
#include <array>
#include <atomic>
#include <cstdint>

// Bounded, immutable snapshots. No reader copies the geometry or waits for a
// writer. A producer writes only an unpublished slot with no pinned readers;
// monotonic versions prevent a late producer from replacing newer geometry.
template<class T>
class PhysicalContactSnapshot
{
    static constexpr uint32_t kWriting=0x80000000u;
    struct Slot { std::atomic<uint32_t> users{0}; T value{}; };
    std::array<Slot,3> slots_{};
    // Low two bits are slot (3 means none); upper bits are version.
    std::atomic<uint64_t> published_{3};
public:
    struct Read
    {
        PhysicalContactSnapshot* owner=nullptr;
        uint32_t slot=3;
        uint64_t version=0;
        Read()=default;
        Read(PhysicalContactSnapshot* o,uint32_t s,uint64_t v):owner(o),slot(s),version(v){}
        Read(const Read&)=delete;
        Read& operator=(const Read&)=delete;
        Read(Read&& other) noexcept:owner(other.owner),slot(other.slot),version(other.version) { other.owner=nullptr; }
        ~Read() { if (owner) owner->slots_[slot].users.fetch_sub(1,std::memory_order_release); }
        explicit operator bool() const { return owner!=nullptr; }
        const T& get() const { return owner->slots_[slot].value; }
    };
    struct Write
    {
        PhysicalContactSnapshot* owner=nullptr;
        uint32_t slot=3;
        Write()=default;
        Write(PhysicalContactSnapshot* o,uint32_t s):owner(o),slot(s){}
        Write(const Write&)=delete;
        Write& operator=(const Write&)=delete;
        Write(Write&& other) noexcept:owner(other.owner),slot(other.slot) { other.owner=nullptr; }
        ~Write() { if (owner) owner->slots_[slot].users.store(0,std::memory_order_release); }
        explicit operator bool() const { return owner!=nullptr; }
        T& get() { return owner->slots_[slot].value; }
        bool publish(uint64_t version)
        {
            if (!owner || !version || version>(UINT64_MAX>>2)) return false;
            uint64_t previous=owner->published_.load(std::memory_order_acquire);
            for (unsigned attempt=0;attempt<3;++attempt)
            {
                if ((previous>>2)>version) return false;
                if (owner->published_.compare_exchange_weak(previous,(version<<2)|slot,
                        std::memory_order_acq_rel,std::memory_order_acquire))
                {
                    // Publish before releasing the writer reservation. Reversing
                    // this order lets a second producer overwrite the slot in
                    // the gap between data-ready and publication.
                    owner->slots_[slot].users.store(0,std::memory_order_release);
                    owner=nullptr;
                    return true;
                }
            }
            return false;
        }
    };
    Read read()
    {
        for (unsigned attempt=0;attempt<3;++attempt)
        {
            const uint64_t descriptor=published_.load(std::memory_order_acquire);
            const uint32_t slot=static_cast<uint32_t>(descriptor&3u);
            if (slot>=slots_.size()) return {};
            uint32_t users=slots_[slot].users.load(std::memory_order_acquire);
            if ((users&kWriting) || users>=kWriting-1) continue;
            if (!slots_[slot].users.compare_exchange_weak(users,users+1,
                    std::memory_order_acquire,std::memory_order_relaxed)) continue;
            // A writer may have retired and republished this slot before the
            // pin succeeded. Match its descriptor as well as its reader count.
            if (published_.load(std::memory_order_acquire)==descriptor)
                return Read(this,slot,descriptor>>2);
            slots_[slot].users.fetch_sub(1,std::memory_order_release);
        }
        return {};
    }
    Write write()
    {
        for (uint32_t slot=0;slot<slots_.size();++slot)
        {
            if ((published_.load(std::memory_order_acquire)&3u)==slot) continue;
            uint32_t expected=0;
            if (!slots_[slot].users.compare_exchange_strong(expected,kWriting,
                    std::memory_order_acq_rel,std::memory_order_relaxed)) continue;
            if ((published_.load(std::memory_order_acquire)&3u)==slot)
            { slots_[slot].users.store(0,std::memory_order_release); continue; }
            return Write(this,slot);
        }
        return {};
    }
};
