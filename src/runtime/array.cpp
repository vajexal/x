#include "array.h"

#include "runtime/string.h"
#include "utils.h"

namespace X::Runtime {
    llvm::StructType *ArrayRuntime::add(llvm::Type *type) {
        const auto &arrayTypeName = Array::getClassName(type);
        auto arrayType = llvm::StructType::create(
                context,
                {type->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)},
                arrayTypeName
        );

        addConstructor(arrayType, type);
        addGetter(arrayType, type);
        addSetter(arrayType, type);
        addLength(arrayType, type);
        addIsEmpty(arrayType, type);
        addAppend(arrayType, type);

        containedTypes[arrayTypeName] = type;

        return arrayType;
    }

    llvm::Type *ArrayRuntime::getContainedType(llvm::StructType *arrayType) const {
        auto type = containedTypes.find(arrayType->getName().str());
        return type != containedTypes.end() ? type->second : nullptr;
    }

    void ArrayRuntime::addConstructor(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), CONSTRUCTOR_FN_NAME), module);

        auto that = fn->getArg(0);
        auto len = fn->getArg(1);

        that->setName(THIS_KEYWORD);
        len->setName("len");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto capVar = builder.CreateAlloca(llvm::Type::getInt64Ty(context), nullptr, "cap");
        builder.CreateStore(len, capVar);

        // validate len
        auto setLenBB = llvm::BasicBlock::Create(context, "set_len", fn);
        auto invalidLenBB = llvm::BasicBlock::Create(context, "invalid_len");
        auto cond = builder.CreateICmpSLT(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateCondBr(cond, invalidLenBB, setLenBB);

        builder.SetInsertPoint(setLenBB);

        // set len
        auto lenPtr = builder.CreateStructGEP(arrayType, that, 1);
        builder.CreateStore(len, lenPtr);

        // check cap
        auto setCapBB = llvm::BasicBlock::Create(context, "set_cap");
        auto growCapBB = llvm::BasicBlock::Create(context, "grow_cap");
        auto minCap = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), Array::MIN_CAP);
        cond = builder.CreateICmpSLT(len, minCap);
        builder.CreateCondBr(cond, growCapBB, setCapBB);

        // grow cap
        fn->getBasicBlockList().push_back(growCapBB);
        builder.SetInsertPoint(growCapBB);
        builder.CreateStore(minCap, capVar);
        builder.CreateBr(setCapBB);

        // set cap
        fn->getBasicBlockList().push_back(setCapBB);
        builder.SetInsertPoint(setCapBB);
        auto cap = builder.CreateLoad(llvm::Type::getInt64Ty(context), capVar, "cap");
        auto capPtr = builder.CreateStructGEP(arrayType, that, 2);
        builder.CreateStore(cap, capPtr);

        // alloc

        auto allocFn = module.getFunction(mangler.mangleInternalFunction("gcAlloc"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));
        auto elemTypeSize = getTypeSize(module, elemType);
        auto allocSize = builder.CreateMul(cap, elemTypeSize);
        auto arrVoidPtr = builder.CreateCall(allocFn, {gcVar, allocSize});
        auto arr = builder.CreateBitCast(arrVoidPtr, elemType->getPointerTo());
        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        builder.CreateStore(arr, arrPtr);

        builder.CreateRetVoid();

        // invalid len error
        fn->getBasicBlockList().push_back(invalidLenBB);
        builder.SetInsertPoint(invalidLenBB);

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addGetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                elemType,
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context)},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), "get[]"), module);

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

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addSetter(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {arrayType->getPointerTo(), llvm::Type::getInt64Ty(context), elemType},
                false
        );
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), "set[]"), module);

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

        auto exitFn = module.getFunction("exit");
        builder.CreateCall(exitFn, {llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1)});
        builder.CreateUnreachable();
    }

    void ArrayRuntime::addLength(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), {arrayType->getPointerTo()}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), "length"), module);

        auto that = fn->getArg(0);
        that->setName(THIS_KEYWORD);

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrLenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto arrLen = builder.CreateLoad(llvm::Type::getInt64Ty(context), arrLenPtr, "len");
        builder.CreateRet(arrLen);
    }

    void ArrayRuntime::addIsEmpty(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getInt1Ty(context), {arrayType->getPointerTo()}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), "isEmpty"), module);

        auto that = fn->getArg(0);
        that->setName(THIS_KEYWORD);

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrLengthFn = module.getFunction(mangler.mangleInternalMethod(arrayType->getName().str(), "length"));
        auto arrLen = builder.CreateCall(arrLengthFn, {that});
        auto isEmpty = builder.CreateICmpEQ(arrLen, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
        builder.CreateRet(isEmpty);
    }

    void ArrayRuntime::addAppend(llvm::StructType *arrayType, llvm::Type *elemType) {
        auto fnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {arrayType->getPointerTo(), elemType}, false);
        auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                         mangler.mangleInternalMethod(arrayType->getName().str(), "append[]"), module);

        auto that = fn->getArg(0);
        auto val = fn->getArg(1);
        that->setName(THIS_KEYWORD);
        val->setName("val");

        auto bb = llvm::BasicBlock::Create(context, "entry", fn);
        llvm::IRBuilder<> builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        builder.SetInsertPoint(bb);

        auto arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        auto arr = builder.CreateLoad(elemType->getPointerTo(), arrPtr, "arr");
        auto lenPtr = builder.CreateStructGEP(arrayType, that, 1);
        auto len = builder.CreateLoad(llvm::Type::getInt64Ty(context), lenPtr, "len");
        auto capPtr = builder.CreateStructGEP(arrayType, that, 2);
        auto cap = builder.CreateLoad(llvm::Type::getInt64Ty(context), capPtr, "cap");
        auto newLen = builder.CreateAdd(len, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1));

        // grow
        auto growBB = llvm::BasicBlock::Create(context, "grow", fn);
        auto appendBB = llvm::BasicBlock::Create(context, "append");
        auto cond = builder.CreateICmpSGE(newLen, cap);
        builder.CreateCondBr(cond, growBB, appendBB);

        builder.SetInsertPoint(growBB);
        auto newCap = builder.CreateShl(cap, 1);
        builder.CreateStore(newCap, capPtr);

        auto reallocFn = module.getFunction(mangler.mangleInternalFunction("gcRealloc"));
        auto gcVar = module.getGlobalVariable(mangler.mangleInternalSymbol("gc"));
        auto arrVoidPtr = builder.CreateBitCast(arr, llvm::Type::getInt8PtrTy(context));
        auto elemTypeSize = getTypeSize(module, elemType);
        auto allocSize = builder.CreateMul(newCap, elemTypeSize);
        auto newArrVoidPtr = builder.CreateCall(reallocFn, {gcVar, arrVoidPtr, allocSize});
        auto newArr = builder.CreateBitCast(newArrVoidPtr, arr->getType());
        builder.CreateStore(newArr, arrPtr);
        builder.CreateBr(appendBB);

        // append val
        fn->getBasicBlockList().push_back(appendBB);
        builder.SetInsertPoint(appendBB);

        builder.CreateStore(newLen, lenPtr);
        arrPtr = builder.CreateStructGEP(arrayType, that, 0);
        arr = builder.CreateLoad(elemType->getPointerTo(), arrPtr, "arr");
        auto elemPtr = builder.CreateGEP(elemType, arr, len);
        builder.CreateStore(val, elemPtr);

        builder.CreateRetVoid();
    }

    std::string Array::getClassName(const Type *type) {
        switch (type->getTypeID()) {
            case Type::TypeID::INT:
                return CLASS_NAME + ".int";
            case Type::TypeID::FLOAT:
                return CLASS_NAME + ".float";
            case Type::TypeID::BOOL:
                return CLASS_NAME + ".bool";
            case Type::TypeID::STRING:
                return CLASS_NAME + ".string";
            case Type::TypeID::CLASS: {
                // todo fix me later
                Mangler mangler;
                return CLASS_NAME + '.' + mangler.mangleClass(type->getClassName());
            }
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

        if (type->isDoubleTy()) {
            return CLASS_NAME + ".float";
        }

        if (String::isStringType(type)) {
            return CLASS_NAME + ".string";
        }

        type = deref(type);
        if (type->isStructTy()) {
            return CLASS_NAME + '.' + type->getStructName().str();
        }

        throw InvalidArrayTypeException();
    }

    bool Array::isArrayType(llvm::Type *type) {
        type = deref(type);
        return type->isStructTy() && type->getStructName().startswith(Runtime::Array::CLASS_NAME);
    }
}
