#pragma once

#include <unordered_map>
#include <deque>
#include <vector>

#include "compiler_runtime.h"

namespace X::Runtime::GC {
    struct Metadata;

    struct Root {
        void **ptr;
        Metadata *meta;
    };

    enum class NodeType {
        CLASS,
        INTERFACE,
        ARRAY,
    };

    // pair<offset, meta>
    using PointerList = std::vector<std::pair<unsigned long, Metadata *>>;

    struct Metadata {
        NodeType type;
        PointerList pointerList;
    };

    class GC {
        std::vector<Metadata *> metaBag;

        // {ptr -> is alive}
        std::unordered_map<void *, bool> allocs;
        std::deque<std::vector<Root>> stackFrames;

    public:
        virtual ~GC() {
            for (auto meta: metaBag) {
                delete meta;
            }
        }

        Metadata *addMeta(NodeType type, PointerList &&pointerList);

        void run();

        void *alloc(std::size_t size);
        void pushStackFrame();
        void popStackFrame();
        void addRoot(void **root, Metadata *meta);

    private:
        void mark();
        void sweep();
    };
}
