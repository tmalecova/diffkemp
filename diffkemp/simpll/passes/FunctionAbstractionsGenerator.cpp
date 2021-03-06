//===- FunctionAbstractionsGenerator.cpp - Abstracting non-function calls -===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains functions generating and unifying abstractions for
/// indirect function calls and for inline assemblies.
///
//===----------------------------------------------------------------------===//

#include "CalledFunctionsAnalysis.h"
#include "FunctionAbstractionsGenerator.h"
#include "Utils.h"
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ADT/Hashing.h>
#include <Config.h>

AnalysisKey FunctionAbstractionsGenerator::Key;

/// Creates a new function for each type of function that is called indirectly
/// or as an inline assembly.
FunctionAbstractionsGenerator::Result FunctionAbstractionsGenerator::run(
        Module &Mod,
        AnalysisManager<Module, Function *> &mam,
        Function *Main) {
    FunMap funAbstractions;
    StringMap<StringRef> asmValueMap;
    int i = 0;
    std::vector<Instruction *> toErase;

    auto &CalledFuns = mam.getResult<CalledFunctionsAnalysis>(Mod, Main);

    for (auto &Fun : Mod) {
        if (CalledFuns.find(&Fun) == CalledFuns.end())
            continue;
        for (auto &BB : Fun) {
            for (auto &Instr : BB) {
                if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                    auto funCalled =
                            getCalledFunction(CallInstr->getCalledValue());
                    if (funCalled)
                        continue;
                    auto CalledType = CallInstr->getCalledValue()->getType();
                    if (!CalledType->isPointerTy())
                        continue;

                    // Retrieve function from the hash map if it has already
                    // been created or create a new one.
                    auto FunType = dyn_cast<FunctionType>(
                            dyn_cast<PointerType>(
                                    CalledType)->getElementType());

                    std::string hash = funHash(CallInstr->getCalledValue());
                    auto funAbstr = funAbstractions.find(hash);
                    Function *newFun;

                    if (funAbstr == funAbstractions.end()) {
                        std::vector<Type *> newParamTypes;
                        newParamTypes = FunType->params();
                        if (!CallInstr->isInlineAsm())
                            newParamTypes.push_back(CalledType);
                        auto newFunType = FunctionType::get(
                                FunType->getReturnType(), newParamTypes, false);

                        uint64_t hashNumber = hash_value(hash);
                        while (Mod.getFunction(abstractionPrefix(
                                CallInstr->getCalledValue()) +
                                std::to_string(hashNumber)) != nullptr)
                            hashNumber++;
                        const std::string funName =
                                abstractionPrefix(CallInstr->getCalledValue()) +
                                std::to_string(hashNumber);

                        newFun = Function::Create(
                                newFunType,
                                Function::ExternalLinkage,
                                funName, &Mod);
                        funAbstractions.emplace(hash, newFun);
                        if (auto assembly =
                            dyn_cast<InlineAsm>(CallInstr->getCalledValue())) {
                            asmValueMap[funName] = assembly->getAsmString();
                        }
                    } else {
                        newFun = funAbstr->second;
                    }

                    // Transform the call to a call to the abstraction
                    std::vector<Value *> args;
                    for (auto &a : CallInstr->arg_operands()) {
                        if (auto argVal = dyn_cast<Value>(&a))
                            args.push_back(argVal);
                    }
                    if (!CallInstr->isInlineAsm())
                        args.push_back(CallInstr->getCalledValue());
                    auto newCall = CallInst::Create(newFun, args, "",
                                                    CallInstr);
                    newCall->setDebugLoc(CallInstr->getDebugLoc());

                    CallInstr->replaceAllUsesWith(newCall);
                    toErase.push_back(&Instr);
                }
            }
            for (auto &I : toErase)
                I->eraseFromParent();
            toErase.clear();
        }
    }
    return FunctionAbstractionsGenerator::Result {funAbstractions, asmValueMap};
}

/// A hash that uniquely identifies an indirect function or an inline asm.
/// It contains the string representing the function type, and for inline asm
/// also the assembly parameters and code.
std::string FunctionAbstractionsGenerator::funHash(Value *Fun) {
    std::string result = typeName(Fun->getType());
    if (auto inlineAsm = dyn_cast<InlineAsm>(Fun)) {
        result += "$" + inlineAsm->getAsmString() + "$" +
                inlineAsm->getConstraintString();
    }
    return result;
}

std::string FunctionAbstractionsGenerator::abstractionPrefix(Value *Fun) {
    if (isa<InlineAsm>(Fun))
        return SimpllInlineAsmPrefix;
    else
        return SimpllIndirectFunctionPrefix;
}

/// Swaps names of two functions in a module.
/// \param Map Function hash map of the appropriate module.
/// \param SrcHash Hash of one of the functions.
/// \param DestName Name of the other of the functions.
/// \return True if the swap was successful.
bool trySwap(FunctionAbstractionsGenerator::FunMap &Map,
             const std::string SrcHash,
             const std::string DestName) {
    for (auto &Fun : Map) {
        if (Fun.second->getName() == DestName) {
            const std::string srcName = Map.find(SrcHash)->second->getName();
            Map.find(SrcHash)->second->setName("$tmpName");
            Fun.second->setName(srcName);
            Map.find(SrcHash)->second->setName(DestName);
            return true;
        }
    }
    return false;
}

/// Unify abstractions between modules.
/// Finds functions with same hashes and if their name do not match, unifies it.
/// The unification is done by swapping name of one of the functions with
/// another function having the desired name in the same module.
void unifyFunctionAbstractions(
        FunctionAbstractionsGenerator::Result &FirstResult,
        FunctionAbstractionsGenerator::Result &SecondResult) {
    for (auto &FirstFun : FirstResult.funAbstractions) {
        auto SecondFun = SecondResult.funAbstractions.find(FirstFun.first);

        if (SecondFun == SecondResult.funAbstractions.end())
            continue;

        if (FirstFun.second->getName() != SecondFun->second->getName()) {
            if (trySwap(FirstResult.funAbstractions, FirstFun.first,
                        SecondFun->second->getName())) {
                if (FirstFun.second->getName().startswith(
                            SimpllInlineAsmPrefix) &&
                        SecondFun->second->getName().startswith(
                            SimpllInlineAsmPrefix)) {
                    FirstResult.asmValueMap[SecondFun->second->getName()] =
                    SecondResult.asmValueMap[SecondFun->second->getName()];
                }
            } else if (trySwap(SecondResult.funAbstractions, SecondFun->first,
                               FirstFun.second->getName())) {
                if (FirstFun.second->getName().startswith(
                            SimpllInlineAsmPrefix) &&
                        SecondFun->second->getName().startswith(
                            SimpllInlineAsmPrefix)) {
                    SecondResult.asmValueMap[FirstFun.second->getName()] =
                    FirstResult.asmValueMap[FirstFun.second->getName()];
                }
            } else {
                FirstFun.second->setName(SecondFun->second->getName());

                if (FirstFun.second->getName().startswith(
                            SimpllInlineAsmPrefix) &&
                        SecondFun->second->getName().startswith(
                            SimpllInlineAsmPrefix)) {
                    SecondResult.asmValueMap[SecondFun->second->getName()] =
                    FirstResult.asmValueMap[SecondFun->second->getName()];
                }
            }
        }
    }
}
