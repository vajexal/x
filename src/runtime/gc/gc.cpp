#include "gc.h"

#include <cstdlib>
#include <cstring>

namespace X::Runtime::GC {
    void *GC::alloc(std::size_t size) {
        auto ptr = std::malloc(size);
        if (!ptr) {
            std::abort();
        }
        std::memset(ptr, 0, size);

        allocs[ptr] = false;

        return ptr;
    }

    void GC::pushStackFrame() {
        stackFrames.emplace_back();
    }

    void GC::popStackFrame() {
        stackFrames.pop_back();
    }

    void GC::addRoot(void **root, unsigned long classId) {
        stackFrames.back().push_back({root, classId});
    }

    void GC::run() {
        mark();
        sweep();
    }

    void GC::mark() {
        for (auto &alloc: allocs) {
            alloc.second = false;
        }

        std::deque<std::pair<void *, unsigned long>> objects;
        for (auto &roots: stackFrames) {
            for (auto &root: roots) {
                if (*root.ptr) {
                    objects.emplace_back(*root.ptr, root.classId);
                }
            }
        }

        while (!objects.empty()) {
            auto [ptr, classId] = objects.back();
            objects.pop_back();

            auto it = allocs.find(ptr);
            if (it == allocs.cend()) {
                return; // some broken link (shouldn't happen)
            }

            if (it->second) {
                continue; // already visited this alloc
            }

            it->second = true;

            switch (classId) {
                case INTERFACE_CLASS_ID: {
                    auto objPtr = (void **)((uint64_t)ptr + 8);
                    auto classIdPtr = (unsigned long *)((uint64_t)ptr + 16);
                    objects.emplace_back(*objPtr, *classIdPtr);
                    break;
                }
                case STRING_CLASS_ID:
                    break;
                default:
                    for (auto [offset, fieldClassId]: compilerRuntime.classPointerLists.at(classId)) {
                        auto fieldPtr = (void **)((uint64_t)ptr + offset);
                        if (*fieldPtr) {
                            objects.emplace_back(*fieldPtr, fieldClassId);
                        }
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
