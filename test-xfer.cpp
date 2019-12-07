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

Value *maskKnown(const KnownBits &K, Value *V) {
  auto O = B.CreateOr(V, K.One);
  auto A = B.CreateAnd(O, ~K.Zero);
  return A;
}

bool nextKB(KnownBits &K) {
  do {
    K.Zero = K.Zero + 1;
    if (K.Zero == 0) {
      K.One = K.One + 1;
      if (K.One == 0)
        return false;
    }
  } while (K.hasConflict());
  return true;
}
  
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
  long Bits = 0, Cases = 0;

  // fixme: iterate over possibilties
  // fixme: parameterize on instructio
  // fixme: support pseudo-unary and constant arguments

  KnownBits A0(W), A1(W);
  while (true) {
    auto I = B.CreateAShr(maskKnown(A0, Args[0]), maskKnown(A1, Args[1]));
    auto R = B.CreateRet(I);
    KnownBits KB;
    computeKnownBits(I, KB, DL);
    Bits += KB.Zero.countPopulation() + KB.One.countPopulation();
    Cases++;

    if (!nextKB(A0))
      if (!nextKB(A1))
        break;
    
    //R->eraseFromParent();
    // this is not good code but should be fine for very small number
    // of instructions
    while (!BB->empty()) {
      for (auto &I2 : *BB) {
        if (I2.hasNUses(0)) {
          I2.eraseFromParent();
          break;
        }
      }
    }
  }
  
  M->print(errs(), nullptr);
  outs() << "total known bits = " << Bits << "\n";
  outs() << "total cases = " << Cases << "\n";
  
  return 0;
}
