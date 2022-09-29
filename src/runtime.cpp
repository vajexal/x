#include <iostream>
#include <cstring>

#include "runtime.h"

namespace X {
    String *String_new(uint64_t len = 0) {
        auto res = new String;
        res->len = len;
        res->str = new char[res->len + 1];
        res->str[res->len] = 0;
        return res;
    }

    void String_construct(String *that, const char *s) {
        that->len = std::strlen(s);
        that->str = new char[that->len + 1];
        that->str[that->len] = 0;
        std::memcpy(that->str, s, that->len);
    }

    String *String_create(const char *s) {
        auto res = new String;
        String_construct(res, s);
        return res;
    }

    String *String_copy(String *str) {
        auto res = String_new(str->len);
        std::memcpy(res->str, str->str, str->len);
        return res;
    }

    String *String_concat(String *that, String *other) {
        if (!other->len) {
            return String_copy(that);
        }

        auto res = String_new(that->len + other->len);
        std::memcpy(res->str, that->str, that->len);
        std::memcpy(res->str + that->len, other->str, other->len);
        return res;
    }

    uint64_t String_length(String *that) {
        return that->len;
    }

    bool String_isEmpty(String *that) {
        return !String_length(that);
    }

    String *String_trim(String *that) {
        if (!that->len) {
            return String_new();
        }

        auto startIdx = 0;
        for (; startIdx < that->len && isspace(that->str[startIdx]); startIdx++) {}

        if (startIdx == that->len - 1) {
            return String_new();
        }

        auto endIdx = that->len - 1;
        for (; endIdx > startIdx && isspace(that->str[endIdx]); endIdx--) {}

        auto res = String_new(endIdx - startIdx + 1);
        std::memcpy(res->str, that->str + startIdx, res->len);
        return res;
    }

    String *String_toLower(String *that) {
        auto res = String_new(that->len);

        for (auto i = 0; i < that->len; i++) {
            res->str[i] = (char) std::tolower(that->str[i]);
        }

        return res;
    }

    String *String_toUpper(String *that) {
        auto res = String_new(that->len);

        for (auto i = 0; i < that->len; i++) {
            res->str[i] = (char) std::toupper(that->str[i]);
        }

        return res;
    }

    int64_t String_index(String *that, String *other) {
        auto res = std::strstr(that->str, other->str);
        return res ? res - that->str : -1;
    }

    bool String_contains(String *that, String *other) {
        return String_index(that, other) != -1;
    }

    bool String_startsWith(String *that, String *other) {
        if (other->len > that->len) {
            return false;
        }

        return std::strncmp(that->str, other->str, other->len) == 0;
    }

    bool String_endsWith(String *that, String *other) {
        if (other->len > that->len) {
            return false;
        }

        return std::strncmp(that->str + that->len - other->len, other->str, other->len) == 0;
    }

    String *String_substring(String *that, int64_t offset, int64_t length) {
        if (offset < 0 || length <= 0 || offset > that->len) {
            return String_new();
        }

        if (length + offset > that->len) {
            length = (int64_t) that->len - offset;
        }

        auto res = String_new(length);
        std::memcpy(res->str, that->str + offset, length);
        return res;
    }

    void println(String *str) {
        std::cout << str->str << std::endl;
    }

    String *castBoolToString(bool value) {
        return value ? String_create("true") : String_create("false");
    }

    String *castIntToString(int64_t value) {
        auto s = std::to_string(value);
        return String_create(s.c_str());
    }

    String *castFloatToString(float value) {
        auto s = std::to_string(value);
        return String_create(s.c_str());
    }

    /// returns true is stings are equal
    bool compareStrings(String *first, String *second) {
        return first->len == second->len && std::strncmp(first->str, second->str, first->len) == 0;
    }

    void Runtime::addDeclarations(llvm::LLVMContext &context, llvm::Module &module) {
        auto abortFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {}, false);
        functions["abort"] = llvm::cast<llvm::Function>(module.getOrInsertFunction("abort", abortFnType).getCallee());

        auto stringType = llvm::StructType::create(
                context, {llvm::Type::getInt8PtrTy(context), llvm::Type::getInt64Ty(context)}, String::CLASS_NAME);

        auto stringConstructFnType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context), {stringType->getPointerTo(), llvm::Type::getInt8PtrTy(context)}, false);
        auto stringConstructFnName = mangler.mangleMethod(String::CLASS_NAME, "construct");
        functions[stringConstructFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringConstructFnName, stringConstructFnType).getCallee());

        auto stringConcatFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringConcatFnName = mangler.mangleMethod(String::CLASS_NAME, "concat");
        functions[stringConcatFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringConcatFnName, stringConcatFnType).getCallee());

        auto stringLengthFnType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context), {stringType->getPointerTo()}, false);
        auto stringLengthFnName = mangler.mangleMethod(String::CLASS_NAME, "length");
        functions[stringLengthFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringLengthFnName, stringLengthFnType).getCallee());

        auto stringIsEmptyFnType = llvm::FunctionType::get(
                llvm::Type::getInt1Ty(context), {stringType->getPointerTo()}, false);
        auto stringIsEmptyFnName = mangler.mangleMethod(String::CLASS_NAME, "isEmpty");
        functions[stringIsEmptyFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringIsEmptyFnName, stringIsEmptyFnType).getCallee());

        auto stringTrimFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo()}, false);
        auto stringTrimFnName = mangler.mangleMethod(String::CLASS_NAME, "trim");
        functions[stringTrimFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringTrimFnName, stringTrimFnType).getCallee());

        auto stringToLowerFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo()}, false);
        auto stringToLowerFnName = mangler.mangleMethod(String::CLASS_NAME, "toLower");
        functions[stringToLowerFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringToLowerFnName, stringToLowerFnType).getCallee());

        auto stringToUpperFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo()}, false);
        auto stringToUpperFnName = mangler.mangleMethod(String::CLASS_NAME, "toUpper");
        functions[stringToUpperFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringToUpperFnName, stringToUpperFnType).getCallee());

        auto stringIndexFnType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringIndexFnName = mangler.mangleMethod(String::CLASS_NAME, "index");
        functions[stringIndexFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringIndexFnName, stringIndexFnType).getCallee());

        auto stringContainsFnType = llvm::FunctionType::get(
                llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringContainsFnName = mangler.mangleMethod(String::CLASS_NAME, "contains");
        functions[stringContainsFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringContainsFnName, stringContainsFnType).getCallee());

        auto stringStartsWithFnType = llvm::FunctionType::get(
                llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringStartsWithFnName = mangler.mangleMethod(String::CLASS_NAME, "startsWith");
        functions[stringStartsWithFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringStartsWithFnName, stringStartsWithFnType).getCallee());

        auto stringEndsWithFnType = llvm::FunctionType::get(
                llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        auto stringEndsWithFnName = mangler.mangleMethod(String::CLASS_NAME, "endsWith");
        functions[stringEndsWithFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringEndsWithFnName, stringEndsWithFnType).getCallee());

        auto stringSubstringFnType = llvm::FunctionType::get(
                stringType->getPointerTo(), {stringType->getPointerTo(), llvm::Type::getInt64Ty(context), llvm::Type::getInt64Ty(context)}, false);
        auto stringSubstringFnName = mangler.mangleMethod(String::CLASS_NAME, "substring");
        functions[stringSubstringFnName] = llvm::cast<llvm::Function>(
                module.getOrInsertFunction(stringSubstringFnName, stringSubstringFnType).getCallee());

        auto printlnFnType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), {stringType->getPointerTo()}, false);
        functions["println"] = llvm::cast<llvm::Function>(module.getOrInsertFunction("println", printlnFnType).getCallee());

        auto castBoolToStringFnType = llvm::FunctionType::get(stringType->getPointerTo(), {llvm::Type::getInt1Ty(context)}, false);
        functions["castBoolToString"] = llvm::cast<llvm::Function>(module.getOrInsertFunction(".castBoolToString", castBoolToStringFnType).getCallee());

        auto castIntToStringFnType = llvm::FunctionType::get(stringType->getPointerTo(), {llvm::Type::getInt64Ty(context)}, false);
        functions["castIntToString"] = llvm::cast<llvm::Function>(module.getOrInsertFunction(".castIntToString", castIntToStringFnType).getCallee());

        auto castFloatToStringFnType = llvm::FunctionType::get(stringType->getPointerTo(), {llvm::Type::getFloatTy(context)}, false);
        functions["castFloatToString"] = llvm::cast<llvm::Function>(module.getOrInsertFunction(".castFloatToString", castFloatToStringFnType).getCallee());

        auto compareStringsFnType = llvm::FunctionType::get(llvm::Type::getInt1Ty(context), {stringType->getPointerTo(), stringType->getPointerTo()}, false);
        functions["compareStrings"] = llvm::cast<llvm::Function>(module.getOrInsertFunction(".compareStrings", compareStringsFnType).getCallee());
    }

    void Runtime::addDefinitions(llvm::ExecutionEngine &engine) {
        engine.addGlobalMapping(functions["abort"], reinterpret_cast<void *>(std::abort));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "construct")], reinterpret_cast<void *>(String_construct));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "concat")], reinterpret_cast<void *>(String_concat));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "length")], reinterpret_cast<void *>(String_length));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "isEmpty")], reinterpret_cast<void *>(String_isEmpty));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "trim")], reinterpret_cast<void *>(String_trim));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "toLower")], reinterpret_cast<void *>(String_toLower));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "toUpper")], reinterpret_cast<void *>(String_toUpper));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "index")], reinterpret_cast<void *>(String_index));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "contains")], reinterpret_cast<void *>(String_contains));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "startsWith")], reinterpret_cast<void *>(String_startsWith));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "endsWith")], reinterpret_cast<void *>(String_endsWith));
        engine.addGlobalMapping(
                functions[mangler.mangleMethod(String::CLASS_NAME, "substring")], reinterpret_cast<void *>(String_substring));
        engine.addGlobalMapping(functions["println"], reinterpret_cast<void *>(println));

        engine.addGlobalMapping(functions["castBoolToString"], reinterpret_cast<void *>(castBoolToString));
        engine.addGlobalMapping(functions["castIntToString"], reinterpret_cast<void *>(castIntToString));
        engine.addGlobalMapping(functions["castFloatToString"], reinterpret_cast<void *>(castFloatToString));
        engine.addGlobalMapping(functions["compareStrings"], reinterpret_cast<void *>(compareStrings));
    }
}
