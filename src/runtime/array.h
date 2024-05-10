#pragma once

#include <string>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "ast.h"
#include "mangler.h"

namespace X::Runtime {
    class ArrayRuntime {
        llvm::LLVMContext &context;
        llvm::Module &module;

    public:
        static inline const std::string CLASS_NAME = "Array";
        static inline const int MIN_CAP = 8;

        ArrayRuntime(llvm::LLVMContext &context, llvm::Module &module) : context(context), module(module) {}

        llvm::StructType *add(const Type &type, llvm::Type *llvmType);

        static std::string getClassName(const Type &type);

    private:
        void addConstructor(llvm::StructType *arrayType, llvm::Type *elemType);
        void addGetter(llvm::StructType *arrayType, llvm::Type *elemType);
        void addSetter(llvm::StructType *arrayType, llvm::Type *elemType);
        void addLength(llvm::StructType *arrayType);
        void addIsEmpty(llvm::StructType *arrayType);
        void addAppend(llvm::StructType *arrayType, llvm::Type *elemType);
    };

    template<typename T>
    struct Array {
        T *data;
        int64_t len;
        int64_t cap;
    };

    class InvalidArrayTypeException : public std::exception {
    public:
        const char *what() const noexcept override {
            return "invalid array type";
        }
    };
}
