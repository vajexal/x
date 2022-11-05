#include "array.h"

#include "runtime/string.h"
#include "utils.h"

namespace X::Runtime {
    void ArrayRuntime::add() {
        std::vector<llvm::Type *> types{
                llvm::Type::getInt64Ty(context),
                llvm::Type::getFloatTy(context),
                llvm::Type::getInt1Ty(context),
                llvm::StructType::getTypeByName(context, String::CLASS_NAME)->getPointerTo()
        };

        for (auto type: types) {
            auto arrayType = llvm::StructType::create(
                    context,
                    {type->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)},
                    Array::getClassName(type)
            );

            addConstructor(arrayType, type);
            addGetter(arrayType, type);
            addSetter(arrayType, type);
            addLength(arrayType, type);
            addIsEmpty(arrayType, type);
        }
    }

    void ArrayRuntime::addConstructor(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleMethod(arrayType->getName().str(), "construct"), module);
        auto that = fn->getArg(0);
        auto len = fn->getArg(1);

        that->setName("this");
        len->setName("len");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        // validate len
        auto thenBB = llvm::BasicBlock::Create(context, "then", fn);
        auto invalidLenBB = llvm::BasicBlock::Create(context, "invalid_len");
        auto cond = builder.CreateICmpSLT(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateCondBr(cond, invalidLenBB, thenBB);

        builder.SetInsertPoint(thenBB);

        // assign len
        auto lenPtr = builder.CreateStructGEP(arrayType, that, 1);
        builder.CreateStore(len, lenPtr);

        // assign cap
        auto capPtr = builder.CreateStructGEP(arrayType, that, 2);
        builder.CreateStore(len, capPtr);

        // malloc
        auto mallocFn = module.getFunction(mangler.mangleInternalFunction("malloc"));
        auto mallocSizeType = llvm::Type::getInt64Ty(context);
        llvm::Value *allocSize = llvm::ConstantExpr::getSizeOf(elemType);
        allocSize = builder.CreateTruncOrBitCast(allocSize, mallocSizeType);
        auto arr = llvm::CallInst::CreateMalloc(
                thenBB, mallocSizeType, elemType, allocSize, len, mallocFn, "arr");
        builder.Insert(arr);
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        builder.CreateStore(arr, arrPtr);

        // todo zero area

        builder.CreateRetVoid();

        // invalid len error
        fn->getBasicBlockList().push_back(invalidLenBB);
        builder.SetInsertPoint(invalidLenBB);

        auto abortFn = module.getFunction("abort");
        builder.CreateCall(abortFn);
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addGetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                elemType,
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleMethod(arrayType->getName().str(), "get[]"), module);
        auto that = fn->getArg(0);
        auto index = fn->getArg(1);

        that->setName("this");
        index->setName("index");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        // validate index
        auto continueCheckBB = llvm::BasicBlock::Create(context, "continue_check_bb", fn);
        auto invalidIndexBB = llvm::BasicBlock::Create(context, "invalid_index");
        auto cond = builder.CreateICmpSLT(index, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateCondBr(cond, invalidIndexBB, continueCheckBB);

        // continue index validation
        builder.SetInsertPoint(continueCheckBB);
        auto thenBB = llvm::BasicBlock::Create(context, "then");
        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(llvm::Type::getInt64Ty(context), arrLenPtr, "len");
        cond = builder.CreateICmpSGE(index, arrLen);
        builder.CreateCondBr(cond, invalidIndexBB, thenBB);

        fn->getBasicBlockList().push_back(thenBB);
        builder.SetInsertPoint(thenBB);

        // get elem
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(elemType->getPointerTo(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, index);
        auto val = builder.CreateLoad(elemType, elemPtr, "elem");
        builder.CreateRet(val);

        // invalid index error
        fn->getBasicBlockList().push_back(invalidIndexBB);
        builder.SetInsertPoint(invalidIndexBB);

        auto abortFn = module.getFunction("abort");
        builder.CreateCall(abortFn);
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addSetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context), elemType},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleMethod(arrayType->getName().str(), "set[]"), module);

        auto that = fn->getArg(0);
        auto index = fn->getArg(1);
        auto val = fn->getArg(2);

        that->setName("this");
        index->setName("index");
        val->setName("val");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        // validate index
        auto continueCheckBB = llvm::BasicBlock::Create(context, "continue_check_bb", fn);
        auto invalidIndexBB = llvm::BasicBlock::Create(context, "invalid_index");
        auto cond = builder.CreateICmpSLT(index, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateCondBr(cond, invalidIndexBB, continueCheckBB);

        // continue index validation
        builder.SetInsertPoint(continueCheckBB);
        auto thenBB = llvm::BasicBlock::Create(context, "then");
        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(llvm::Type::getInt64Ty(context), arrLenPtr, "len");
        cond = builder.CreateICmpSGE(index, arrLen);
        builder.CreateCondBr(cond, invalidIndexBB, thenBB);

        fn->getBasicBlockList().push_back(thenBB);
        builder.SetInsertPoint(thenBB);

        // set elem
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(elemType->getPointerTo(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, index);
        builder.CreateStore(val, elemPtr);

        builder.CreateRetVoid();

        // invalid index error
        fn->getBasicBlockList().push_back(invalidIndexBB);
        builder.SetInsertPoint(invalidIndexBB);

        auto abortFn = module.getFunction("abort");
        builder.CreateCall(abortFn);
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addLength(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), {arrayType->getPointerTo()}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleMethod(arrayType->getName().str(), "length"), module);

        auto that = fn->getArg(0);
        that->setName("this");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(llvm::Type::getInt64Ty(context), arrLenPtr, "len");
        builder.CreateRet(arrLen);
    }

    void ArrayRuntime::addIsEmpty(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt1Ty(context), {arrayType->getPointerTo()}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleMethod(arrayType->getName().str(), "isEmpty"), module);

        auto that = fn->getArg(0);
        that->setName("this");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        builder.SetInsertPoint(bb);

        auto arrLengthFn = module.getFunction(mangler.mangleMethod(arrayType->getName().str(), "length"));
        auto arrLen = builder.CreateCall(arrLengthFn, {that});
        auto isEmpty = builder.CreateICmpEQ(arrLen, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateRet(isEmpty);
    }

    std::string Array::getClassName(Type::TypeID typeID) {
        switch (typeID) {
            case Type::TypeID::INT:
                return CLASS_NAME + ".int";
            case Type::TypeID::FLOAT:
                return CLASS_NAME + ".float";
            case Type::TypeID::BOOL:
                return CLASS_NAME + ".bool";
            case Type::TypeID::STRING:
                return CLASS_NAME + ".string";
            default:
                throw InvalidArrayTypeException();
        }
    }

    std::string Array::getClassName(llvm::Type *type) {
        if (type->isIntegerTy(1)) {
            return CLASS_NAME + ".bool";
        }

        if (type->isIntegerTy()) {
            return CLASS_NAME + ".int";
        }

        if (type->isFloatTy()) {
            return CLASS_NAME + ".float";
        }

        if (String::isStringType(type)) {
            return CLASS_NAME + ".string";
        }

        throw InvalidArrayTypeException();
    }

    bool Array::isArrayType(llvm::Type *type) {
        type = deref(type);
        return type->isStructTy() && type->getStructName().startswith(Runtime::Array::CLASS_NAME);
    }
}
