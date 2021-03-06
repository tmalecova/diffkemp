//===-- FunctionAbstractionsGenerator.h - Abstracting non-function calls --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the class and functions for generating
/// abstractions for indirect calls and inline assemblies.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_FUNCTIONABSTRACTIONSGENERATOR_H
#define DIFFKEMP_SIMPLL_FUNCTIONABSTRACTIONSGENERATOR_H

#include <llvm/IR/PassManager.h>
#include <set>
#include <unordered_map>

const std::string SimpllInlineAsmPrefix = "simpll__inlineasm_";
const std::string SimpllIndirectFunctionPrefix = "simpll__indirect_";

using namespace llvm;

/// Generates abstractions for indirect function calls and for inline assemblies
/// Implemented as a Function pass.
class FunctionAbstractionsGenerator
        : public AnalysisInfoMixin<FunctionAbstractionsGenerator> {
  public:
    typedef std::unordered_map<std::string, Function *> FunMap;
    struct Result {
        FunMap funAbstractions;
        StringMap<StringRef> asmValueMap;
    };

    Result run(Module &Mod,
               AnalysisManager<Module, Function *> &mam,
               Function *Main);

  protected:
    /// Hash of the abstraction function to be used in the function map.
    /// \param Fun Called value (can be a value or an inline assembly).
    std::string funHash(Value *Fun);

    /// Prefix of the abstraction function.
    /// \param Fun Called value (can be a value or an inline assembly).
    std::string abstractionPrefix(Value *Fun);

  private:
    friend AnalysisInfoMixin<FunctionAbstractionsGenerator>;
    static AnalysisKey Key;
};

/// Unify function abstractions between modules. Makes sure that corresponding
/// abstractions get the same name.
void unifyFunctionAbstractions(
        FunctionAbstractionsGenerator::Result &FirstResult,
        FunctionAbstractionsGenerator::Result &SecondResult);

#endif //DIFFKEMP_SIMPLL_FUNCTIONABSTRACTIONSGENERATOR_H
