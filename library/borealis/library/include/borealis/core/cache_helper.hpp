/*
    Copyright 2022 xfangfang

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <stdexcept>
#include <list>
#include <limits>
#include <unordered_map>

#include "borealis/core/singleton.hpp"

namespace brls
{

template <typename K, typename T>
struct Node
{
    K key;
    T value;

    /// Cache entries marked as dirty will be refreshed when fetching the cache next time
    bool dirty = false;

    /// Reference count, 1 for each cache hit
    size_t count = 1;
    size_t bytes = 0;

    Node(K k, T v)
        : key(k)
        , value(v)
    {
    }
};

/**
 * LRU cache
 * If the reference count is not 0, the cache will never expire.
 * Cache items with a reference count of 0 are cached according to LRU rules
 */
template <typename K, typename T>
class LRUCache
{
  public:
    typedef typename std::list<Node<K, T>>::iterator CacheIter;
#define DIRTY "_$dirty$"
    inline static size_t DEFAULT_CAPACITY      = 400;
    inline static bool ALWAYS_CACHE_LOCAL_FILE = true;

    LRUCache(size_t c, T defaultValue)
        : capacity(c)
        , defaultValue(defaultValue)
    {
        if (c < 1)
            throw std::logic_error("Cache capacity cannot less than 1.");
    }

    T get(K key)
    {
        if (!isCacheHit(key))
        {
            // Cache not hit
            return defaultValue;
        }

        // Cache hit
        cacheList.splice(cacheList.begin(), cacheList, cacheMap[key]);
        cacheMap[key]                  = cacheList.begin();
        valueMap[cacheMap[key]->value] = cacheList.begin();
        cacheMap[key]->count++;
        return cacheMap[key]->value;
    }

    void set(K key, T value, size_t bytes = 0)
    {
        if (isCacheHit(key))
        {
            throw std::logic_error("Can not cache the same key twice.");
        }

        // Add new cache
        cacheList.push_front(Node<K, T>(key, value));
        cacheList.front().bytes = bytes;
        usedBytes += bytes;

        // Update the values of two maps
        cacheMap[key]                  = cacheList.begin();
        valueMap[cacheMap[key]->value] = cacheList.begin();
        trim();
    }

    /**
     * It is not really to remove a cache, but to reduce the counter of the corresponding cache by 1
     */
    void remove(T value)
    {
        if (!isExisted(value))
        {
            return;
        }
        if (valueMap[value]->count > 0)
            valueMap[value]->count--;
        trim();
    }

    /**
     * Update a cache value
     */
    void update(T old_val, T new_val, size_t bytes = std::numeric_limits<size_t>::max())
    {
        if (!isExisted(old_val))
        {
            return;
        }
        valueMap[old_val]->value = new_val;
        if (bytes != std::numeric_limits<size_t>::max())
        {
            usedBytes -= valueMap[old_val]->bytes;
            valueMap[old_val]->bytes = bytes;
            usedBytes += bytes;
        }
        if (old_val != new_val)
        {
            valueMap[new_val] = valueMap[old_val];
            valueMap.erase(old_val);
        }
        trim();
    }

    void setCapacity(int c)
    {
        if (c < 1)
            throw std::logic_error("Cache capacity cannot less than 1.");
        // The setting standard of the DEFAULT_CAPACITY should be the upper limit of the number of all pictures.
        c += DEFAULT_CAPACITY;
        this->capacity = c;
        trim();
    }

    void setMemoryLimit(size_t bytes)
    {
        byteLimit = bytes;
        trim();
    }

    size_t memoryUsage() const { return usedBytes; }

    void clear()
    {
        auto vg = brls::Application::getNVGContext();
        for (const auto& entry : cacheList)
            nvgDeleteImage(vg, entry.value);
        cacheList.clear();
        cacheMap.clear();
        valueMap.clear();
        usedBytes = 0;
    }

    /**
     * A dirty cache is not able to be hit
     * @param value
     */
    void markDirty(T value)
    {
        if (!isExisted(value))
        {
            return;
        }

        auto item = valueMap[value];
        if (item->dirty)
            return;

        // modify existing Key.
        cacheMap.erase(item->key);
        item->key += DIRTY;
        item->dirty = true;
    }

    void markAllDirty()
    {
        for (auto& i : cacheList)
        {
            if (i.dirty)
                continue;
            cacheMap.erase(i.key);
            i.key += DIRTY;
            i.dirty = true;
        }
    }

    std::list<Node<K, T>>& getCacheList() { return cacheList; }

    void debug()
    {
        printf("===== cache size: %zu =====\n", cacheList.size());
        for (auto& i : cacheList)
        {
            printf("count: %zu, dirty: %d, value: %zu, key: %s\n", i.count,
                i.dirty, i.value, i.key.c_str());
        }
    }

  private:
    size_t capacity = 1;
    size_t byteLimit = 0;
    size_t usedBytes = 0;
    T defaultValue;
    std::list<Node<K, T>> cacheList;
    std::unordered_map<K, CacheIter> cacheMap;
    std::unordered_map<T, CacheIter> valueMap;

    // Never delete a texture still referenced by a view. If live images alone
    // exceed the budget, discard released textures as soon as they become idle.
    void trim()
    {
        auto vg = brls::Application::getNVGContext();
        auto i = cacheList.end();
        while (i != cacheList.begin() &&
               (cacheList.size() > capacity || (byteLimit && usedBytes > byteLimit)))
        {
            --i;
            if (i->count == 0)
            {
                nvgDeleteImage(vg, i->value);
                cacheMap.erase(i->key);
                valueMap.erase(i->value);
                usedBytes -= i->bytes;
                i = cacheList.erase(i);
            }
        }
    }

    bool isExisted(K key) { return cacheMap.find(key) != cacheMap.end(); }

    bool isExisted(T val) { return valueMap.find(val) != valueMap.end(); }

    bool isCacheHit(K key)
    {
        if (isExisted(key))
            return !cacheMap[key]->dirty;
        return false;
    }
};

class TextureCache : public Singleton<TextureCache>
{
  public:
    TextureCache()
    {
        brls::Application::getWindowSizeChangedEvent()->subscribe(
            [this]()
            { this->cache.markAllDirty(); });

        brls::Application::getExitEvent()->subscribe([this]()
            { this->clean(); });
    }

    int getCache(const std::string& key) { return cache.get(key); }

    /**
     * Add cache
     */
    void addCache(const std::string& key, size_t texture)
    {
        if (texture <= 0)
            return;
        cache.set(key, texture, textureBytes(texture));
    }

    /**
     * Subtract the matching texture cache counter by 1,
     * and delete the cache if the counter returns to zero.
     */
    void removeCache(size_t texture)
    {
        if (texture <= 0)
            return;
        cache.remove(texture);
    }

    void markDirty(size_t texture)
    {
        if (texture <= 0)
            return;
        cache.markDirty(texture);
    }

    /**
     * update texture id
     */
    void updateCache(size_t old_tex, size_t new_tex)
    {
        if (old_tex <= 0 || new_tex <= 0)
            return;
        cache.update(old_tex, new_tex, textureBytes(new_tex));
    }

    void clean()
    {
        cache.clear();
    }

    void debug() { cache.debug(); }

    LRUCache<std::string, size_t> cache = LRUCache<std::string, size_t>(200, 0);

  private:
    static size_t textureBytes(size_t texture)
    {
        int width = 0, height = 0;
        nvgImageSize(brls::Application::getNVGContext(), texture, &width, &height);
        return width > 0 && height > 0 ? size_t(width) * size_t(height) * 4 : 0;
    }
};

}
