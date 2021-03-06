//===----- CalledFunctionsAnalysis.h - Abstracting non-function calls -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the CalledFunctionsAnalysis pass
/// that collects all functions potentially called by the main function.
///
//===----------------------------------------------------------------------===//

#include "CalledFunctionsAnalysis.h"
#include "Utils.h"
#include <llvm/IR/Instructions.h>

AnalysisKey CalledFunctionsAnalysis::Key;

CalledFunctionsAnalysis::Result CalledFunctionsAnalysis::run(
        Module &Mod,
        AnalysisManager<Module, Function *> &mam,
        Function *Main) {
    Result result;
    collectCalled(Main, result);
    return result;
}

/// Recursively collect all functions potentially called by Fun and add them to
/// the Called set. All functions called by 'call' instructions and used as
/// operands to some instructions in Fun are collected.
void CalledFunctionsAnalysis::collectCalled(const Function *Fun,
                                            Result &Called) {
    if (Called.find(Fun) != Called.end())
        return;

    Called.insert(Fun);

    for (auto &BB : *Fun) {
        for (auto &Inst : BB) {
            if (auto Call = dyn_cast<CallInst>(&Inst)) {
                if (auto *CalledFun =
                        getCalledFunction(Call->getCalledValue())) {
                    collectCalled(CalledFun, Called);
                }
            }
            for (auto &Op : Inst.operands()) {
                if (auto *CalledFun = getCalledFunction(Op))
                    collectCalled(CalledFun, Called);
            }
        }
    }
}