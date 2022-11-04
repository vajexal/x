#ifndef X_ARRAY_H
#define X_ARRAY_H

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
        llvm::IRBuilder<> &builder;
        llvm::Module &module;
        Mangler mangler;

    public:
        ArrayRuntime(llvm::LLVMContext &context, llvm::IRBuilder<> &builder, llvm::Module &module) : context(context), builder(builder), module(module) {}

        void add();

    private:
        void addConstructor(llvm::StructType *arrayType, llvm::Type *elemType);
        void addGetter(llvm::StructType *arrayType, llvm::Type *elemType);
        void addSetter(llvm::StructType *arrayType, llvm::Type *elemType);
        void addLength(llvm::StructType *arrayType, llvm::Type *elemType);
        void addIsEmpty(llvm::StructType *arrayType, llvm::Type *elemType);
    };

    struct Array {
        static inline const std::string CLASS_NAME = "Array";

        static std::string getClassName(Type::TypeID typeID);
        static std::string getClassName(llvm::Type *type);
    };

    class InvalidArrayTypeException : public std::exception {
    public:
        const char *what() const noexcept override {
            return "invalid array type";
        }
    };
}

#endif //X_ARRAY_H
