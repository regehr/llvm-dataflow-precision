#include "llvm/ADT/APInt.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

using namespace llvm;

namespace {

const int W = 4;

LLVMContext C;
IRBuilder<> B(C);

} // namespace
  
int main(void) {
  auto M = make_unique<Module>("", C);
  std::vector<Type *> T(2, Type::getIntNTy(C, W));
  FunctionType *FT = FunctionType::get(Type::getIntNTy(C, W), T, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, "test", M.get());
  BasicBlock *BB = BasicBlock::Create(C, "", F);
  B.SetInsertPoint(BB);
  std::vector<Argument *> Args;
  for (auto &A : F->args())
    Args.push_back(&A);
  auto DL = M->getDataLayout();
  long Bits = 0;

  // fixme: loop over possibilties
  // fixme: masking arguments
  auto I = B.CreateAShr(Args[0], Args[1]);
  B.CreateRet(I);
  KnownBits KB;
  computeKnownBits(I, KB, DL);
  Bits += KB.Zero.countPopulation() + KB.One.countPopulation();
  
  // fixme: analyze known bits
  
  M->print(errs(), nullptr);
  outs() << "total known bits = " << Bits << "\n";
  
  return 0;
}
