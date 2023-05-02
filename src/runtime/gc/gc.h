#pragma once

#include <unordered_map>
#include <deque>
#include <vector>

#include "compiler_runtime.h"

namespace X::Runtime::GC {
    struct Root {
        void **ptr;
        unsigned long classId;
    };

    class GC {
        CompilerRuntime &compilerRuntime;

        // {ptr -> is alive}
        std::unordered_map<void *, bool> allocs;
        std::deque<std::vector<Root>> stackFrames;

    public:
        explicit GC(CompilerRuntime &compilerRuntime) : compilerRuntime(compilerRuntime) {}

        void run();

        void *alloc(std::size_t size);
        void pushStackFrame();
        void popStackFrame();
        void addRoot(void **root, unsigned long classId);

    private:
        void mark();
        void sweep();
    };
}