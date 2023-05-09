#include "gc.h"

#include <cstdlib>
#include <cstring>

namespace X::Runtime::GC {
    Metadata *GC::addMeta(NodeType type, PointerList &&pointerList) {
        auto meta = new Metadata{type, std::move(pointerList)};
        metaBag.push_back(meta);
        return meta;
    }

    void *GC::alloc(std::size_t size) {
        auto ptr = std::malloc(size);
        if (!ptr) {
            std::abort();
        }
        std::memset(ptr, 0, size);

        allocs[ptr] = false;

        return ptr;
    }

    void *GC::realloc(void *ptr, std::size_t newSize) {
        auto newPtr = std::realloc(ptr, newSize);
        if (!newPtr) {
            std::abort();
        }

        if (newPtr != ptr) {
            allocs.erase(ptr);
            allocs[newPtr] = false;
        }

        return newPtr;
    }

    void GC::pushStackFrame() {
        stackFrames.emplace_back();
    }

    void GC::popStackFrame() {
        stackFrames.pop_back();
    }

    void GC::addRoot(void **root, Metadata *meta) {
        stackFrames.back().push_back({root, meta});
    }

    void GC::run() {
        mark();
        sweep();
    }

    void GC::mark() {
        for (auto &alloc: allocs) {
            alloc.second = false;
        }

        std::deque<std::pair<void *, Metadata *>> objects;
        for (auto &roots: stackFrames) {
            for (auto &root: roots) {
                if (*root.ptr) {
                    objects.emplace_back(*root.ptr, root.meta);
                }
            }
        }

        while (!objects.empty()) {
            auto [ptr, meta] = objects.back();
            objects.pop_back();

            auto it = allocs.find(ptr);
            if (it == allocs.cend()) {
                return; // some broken link (shouldn't happen)
            }

            if (it->second) {
                continue; // already visited this alloc
            }

            it->second = true;

            switch (meta->type) {
                case NodeType::CLASS:
                    for (auto [offset, fieldMeta]: meta->pointerList) {
                        auto fieldPtr = (void **)((uint64_t)ptr + offset);
                        if (*fieldPtr) {
                            objects.emplace_back(*fieldPtr, fieldMeta);
                        }
                    }

                    break;
                case NodeType::INTERFACE: {
                    auto objPtr = (void **)((uint64_t)ptr + sizeof(void *));
                    auto metaPtr = (Metadata **)((uint64_t)ptr + 2 * sizeof(void *));
                    objects.emplace_back(*objPtr, *metaPtr);
                    break;
                }
                case NodeType::ARRAY: {
                    auto arr = *(void **)ptr;
                    allocs[arr] = true;

                    if (meta->pointerList.empty()) { // if it's scalar array, then there's no need to process
                        break;
                    }

                    auto lenPtr = (int64_t *)((uint64_t)ptr + sizeof(void *));
                    auto len = *lenPtr;

                    for (auto i = 0; i < len; i++) {
                        auto elemPtr = (void **)((uint64_t)arr + i * sizeof(void *));
                        if (*elemPtr) {
                            objects.emplace_back(*elemPtr, meta->pointerList.front().second);
                        }
                    }

                    break;
                }
            }
        }
    }

    void GC::sweep() {
        for (auto it = allocs.begin(); it != allocs.end();) {
            if (it->second) {
                it->second = false;
                it++;
            } else {
                std::free(it->first);
                it = allocs.erase(it);
            }
        }
    }
}
