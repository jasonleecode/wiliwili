#include <cassert>
#include <functional>
#include <string>
#include <vector>
#include <set>

struct NVGcontext {};
static std::set<size_t> deleted;
void nvgDeleteImage(NVGcontext*, size_t id) { assert(deleted.insert(id).second); }
void nvgImageSize(NVGcontext*, size_t, int* w, int* h) { *w = 512; *h = 512; }
namespace brls {
struct FakeEvent { template<class F> void subscribe(F) {} };
struct Application {
    static NVGcontext* getNVGContext() { static NVGcontext context; return &context; }
    static FakeEvent* getWindowSizeChangedEvent() { static FakeEvent event; return &event; }
    static FakeEvent* getExitEvent() { return getWindowSizeChangedEvent(); }
};
}
#include "borealis/core/cache_helper.hpp"

int main()
{
    using Cache = brls::LRUCache<std::string, size_t>;
    Cache::DEFAULT_CAPACITY = 0;
    Cache cache(2, 0);
    cache.setMemoryLimit(100);
    cache.set("a", 1, 40); cache.remove(1);
    cache.set("b", 2, 40); cache.remove(2);
    assert(cache.get("a") == 1);
    cache.set("c", 3, 50);
    assert(deleted.count(2) && !deleted.count(1));
    cache.remove(3);
    cache.set("d", 4, 90);
    assert(cache.memoryUsage() == 130); // Live references are protected.
    cache.remove(1);
    assert(cache.memoryUsage() == 90 && deleted.count(1));
    cache.remove(1); // A repeated release cannot underflow a reference count.
    cache.remove(4);
    cache.setMemoryLimit(32);
    assert(cache.memoryUsage() == 0);
    cache.clear(); cache.clear();

    Cache dirty(2, 0);
    dirty.set("same", 10, 10); dirty.markDirty(10);
    dirty.set("same", 11, 20);
    assert(dirty.get("same") == 11);
    dirty.update(11, 11, 30);
    assert(dirty.get("same") == 11 && dirty.memoryUsage() == 40);
    dirty.remove(10); dirty.remove(11); dirty.remove(11); dirty.remove(11);
    dirty.clear();

    Cache stress(112, 0);
    stress.setMemoryLimit(64 * 1024 * 1024);
    for (size_t n = 100; n < 5100; n++) {
        stress.set(std::to_string(n), n, 1024 * 1024);
        stress.remove(n);
        assert(stress.memoryUsage() <= 64 * 1024 * 1024);
    }
    stress.clear();
    assert(stress.memoryUsage() == 0 && stress.getCacheList().empty());
    return 0;
}
