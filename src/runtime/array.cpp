#include "array.h"

#include "runtime/string.h"
#include "utils.h"

namespace X::Runtime {
    std::string ArrayRuntime::getClassName(const Type &type) {
        if (!type.is(Type::TypeID::ARRAY)) {
            throw InvalidArrayTypeException();
        }

        switch (type.getSubtype()->getTypeID()) {
            case Type::TypeID::INT:
                return CLASS_NAME + ".int";
            case Type::TypeID::FLOAT:
                return CLASS_NAME + ".float";
            case Type::TypeID::BOOL:
                return CLASS_NAME + ".bool";
            case Type::TypeID::STRING:
                return CLASS_NAME + ".string";
            case Type::TypeID::CLASS:
                return CLASS_NAME + ".pointer";
            default:
                throw InvalidArrayTypeException();
        }
    }

    llvm::StructType *ArrayRuntime::add(const Type &arrType, llvm::Type *elemLlvmType) {
        const auto &arrayTypeName = getClassName(arrType);
        auto arrLlvmType = llvm::StructType::create(
                context,
                // data pointer, length, capacity
                {llvm::PointerType::getUnqual(context), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)},
                arrayTypeName
        );

        addConstructor(arrLlvmType, elemLlvmType);
        addGetter(arrLlvmType, elemLlvmType);
        addSetter(arrLlvmType, elemLlvmType);
        addLength(arrLlvmType);
        addIsEmpty(arrLlvmType);
        addAppend(arrLlvmType, elemLlvmType);

        return arrLlvmType;
    }

    void ArrayRuntime::addConstructor(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), CONSTRUCTOR_FN_NAME), module);

        auto that = fn->getArg(0);
        auto len = fn->getArg(1);

        that->setName(THIS_KEYWORD);
        len->setName("len");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto capVar = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "cap");
        builder.CreateStore(len, capVar);

        // validate len
        auto setLenBB = llvm::BasicBlock::Create(context, "set_len", fn);
        auto invalidLenBB = llvm::BasicBlock::Create(context, "invalid_len");
        auto cond = builder.CreateICmpSLT(len, builder.getInt64(0));
        builder.CreateCondBr(cond, invalidLenBB, setLenBB);

        builder.SetInsertPoint(setLenBB);

        // set len
        auto lenPtr = builder.CreateStructGEP(arrayType, that, 1);
        builder.CreateStore(len, lenPtr);

        // check cap
        auto setCapBB = llvm::BasicBlock::Create(context, "set_cap");
        auto growCapBB = llvm::BasicBlock::Create(context, "grow_cap");
        auto minCap = builder.getInt64(ArrayRuntime::MIN_CAP);
        cond = builder.CreateICmpSLT(len, minCap);
        builder.CreateCondBr(cond, growCapBB, setCapBB);

        // grow cap
        fn->insert(fn->end(), growCapBB);
        builder.SetInsertPoint(growCapBB);
        builder.CreateStore(minCap, capVar);
        builder.CreateBr(setCapBB);

        // set cap
        fn->insert(fn->end(), setCapBB);
        builder.SetInsertPoint(setCapBB);
        auto cap = builder.CreateLoad(builder.getInt64Ty(), capVar, "cap");
        auto capPtr = builder.CreateStructGEP(arrayType, that, 2);
        builder.CreateStore(cap, capPtr);

        // alloc

        auto allocFn = module.getFunction(Mangler::mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(Mangler::mangleInternalSymbol("gc"));
        auto elemTypeSize = getTypeSize(module, elemType);
        auto allocSize = builder.CreateMul(cap, elemTypeSize);
        auto arr = builder.CreateCall(allocFn, {gcVar, allocSize});
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        builder.CreateStore(arr, arrPtr);

        builder.CreateRetVoid();

        // invalid len error
        fn->insert(fn->end(), invalidLenBB);
        builder.SetInsertPoint(invalidLenBB);

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {builder.getInt64(1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addGetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                elemType,
                {llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), "get[]"), module);

        auto that = fn->getArg(0);
        auto index = fn->getArg(1);

        that->setName(THIS_KEYWORD);
        index->setName("index");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        // validate index
        auto continueCheckBB = llvm::BasicBlock::Create(context, "continue_check", fn);
        auto invalidIndexBB = llvm::BasicBlock::Create(context, "invalid_index");
        auto cond = builder.CreateICmpSLT(index, builder.getInt64(0));
        builder.CreateCondBr(cond, invalidIndexBB, continueCheckBB);

        // continue index validation
        builder.SetInsertPoint(continueCheckBB);
        auto thenBB = llvm::BasicBlock::Create(context, "then");
        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(builder.getInt64Ty(), arrLenPtr, "len");
        cond = builder.CreateICmpSGE(index, arrLen);
        builder.CreateCondBr(cond, invalidIndexBB, thenBB);

        fn->insert(fn->end(), thenBB);
        builder.SetInsertPoint(thenBB);

        // get elem
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(builder.getPtrTy(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, index);
        auto val = builder.CreateLoad(elemType, elemPtr, "elem");
        builder.CreateRet(val);

        // invalid index error
        fn->insert(fn->end(), invalidIndexBB);
        builder.SetInsertPoint(invalidIndexBB);

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {builder.getInt64(1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addSetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context), elemType},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), "set[]"), module);

        auto that = fn->getArg(0);
        auto index = fn->getArg(1);
        auto val = fn->getArg(2);

        that->setName(THIS_KEYWORD);
        index->setName("index");
        val->setName("val");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        // validate index
        auto continueCheckBB = llvm::BasicBlock::Create(context, "continue_check", fn);
        auto invalidIndexBB = llvm::BasicBlock::Create(context, "invalid_index");
        auto cond = builder.CreateICmpSLT(index, builder.getInt64(0));
        builder.CreateCondBr(cond, invalidIndexBB, continueCheckBB);

        // continue index validation
        builder.SetInsertPoint(continueCheckBB);
        auto thenBB = llvm::BasicBlock::Create(context, "then");
        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(builder.getInt64Ty(), arrLenPtr, "len");
        cond = builder.CreateICmpSGE(index, arrLen);
        builder.CreateCondBr(cond, invalidIndexBB, thenBB);

        fn->insert(fn->end(), thenBB);
        builder.SetInsertPoint(thenBB);

        // set elem
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(builder.getPtrTy(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, index);
        builder.CreateStore(val, elemPtr);

        builder.CreateRetVoid();

        // invalid index error
        fn->insert(fn->end(), invalidIndexBB);
        builder.SetInsertPoint(invalidIndexBB);

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {builder.getInt64(1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addLength(llvm::StructType *arrayType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), {llvm::PointerType::get(context, 0)}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), "length"), module);

        auto that = fn->getArg(0);
        that->setName(THIS_KEYWORD);

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(builder.getInt64Ty(), arrLenPtr, "len");
        builder.CreateRet(arrLen);
    }

    void ArrayRuntime::addIsEmpty(llvm::StructType *arrayType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt1Ty(context), {llvm::PointerType::get(context, 0)}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), "isEmpty"), module);

        auto that = fn->getArg(0);
        that->setName(THIS_KEYWORD);

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrLengthFn = module.getFunction(Mangler::mangleInternalMethod(arrayType->getName().str(), "length"));
        auto arrLen = builder.CreateCall(arrLengthFn, {that});
        auto isEmpty = builder.CreateICmpEQ(arrLen, builder.getInt64(0));
        builder.CreateRet(isEmpty);
    }

    void ArrayRuntime::addAppend(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {llvm::PointerType::get(context, 0), elemType}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         Mangler::mangleInternalMethod(arrayType->getName().str(), "append[]"), module);

        auto that = fn->getArg(0);
        auto val = fn->getArg(1);
        that->setName(THIS_KEYWORD);
        val->setName("val");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(builder.getPtrTy(), arrPtr, "arr");
        auto lenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto len = builder.CreateLoad(builder.getInt64Ty(), lenPtr, "len");
        auto capPtr = builder.CreateStructGEP(arrayType, that, 2);
        auto cap = builder.CreateLoad(builder.getInt64Ty(), capPtr, "cap");
        auto newLen = builder.CreateAdd(len, builder.getInt64(1));

        // grow
        auto growBB = llvm::BasicBlock::Create(context, "grow", fn);
        auto appendBB = llvm::BasicBlock::Create(context, "append");
        auto cond = builder.CreateICmpSGE(newLen, cap);
        builder.CreateCondBr(cond, growBB, appendBB);

        builder.SetInsertPoint(growBB);
        auto newCap = builder.CreateShl(cap, 1);
        builder.CreateStore(newCap, capPtr);

        auto reallocFn = module.getFunction(Mangler::mangleInternalFunction("gcRealloc"));
        auto gcVar = module.getGlobalVariable(Mangler::mangleInternalSymbol("gc"));
        auto elemTypeSize = getTypeSize(module, elemType);
        auto allocSize = builder.CreateMul(newCap, elemTypeSize);
        auto newArr = builder.CreateCall(reallocFn, {gcVar, arr, allocSize});
        builder.CreateStore(newArr, arrPtr);
        builder.CreateBr(appendBB);

        // append val
        fn->insert(fn->end(), appendBB);
        builder.SetInsertPoint(appendBB);

        builder.CreateStore(newLen, lenPtr);
        arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        arr = builder.CreateLoad(builder.getPtrTy(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, len);
        builder.CreateStore(val, elemPtr);

        builder.CreateRetVoid();
    }
}
