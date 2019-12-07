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
#include <iomanip>

using namespace llvm;

namespace {

const int W = 6;

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
  
std::string KBString(KnownBits Known) {
  std::string s = "";
  for (int x = 0; x < Known.getBitWidth(); ++x) {
    if (Known.Zero.isSignBitSet())
      s += "0";
    else if (Known.One.isSignBitSet())
      s += "1";
    else
      s += "?";
    Known.Zero = Known.Zero << 1;
    Known.One = Known.One << 1;
  }
  return s;
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
  long Bits = 0, Cases = 0, TotalBits = 0;

  // fixme: parameterize on instruction
  // fixme: support unary and ternary instructions
  // fixme: support pseudo-unary and constant arguments

  KnownBits K0(W), K1(W);
  while (true) {
    auto I = B.CreateURem(maskKnown(K0, Args[0]), maskKnown(K1, Args[1]));
    auto R = B.CreateRet(I);
    KnownBits KB = computeKnownBits(I, DL);
    Bits += KB.Zero.countPopulation() + KB.One.countPopulation();
    Cases++;
    TotalBits += 2 * W;

    if (0) {
      M->print(errs(), nullptr);
      outs() << KBString(KB) << "\n";
    }

    if (!nextKB(K0))
      if (!nextKB(K1))
        break;
    
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
  
  outs() << std::fixed << std::showpoint << std::setprecision(4);
  outs() << "total cases = " << Cases << "\n";
  outs() << "total known bits = " << Bits << "\n";
  outs() << "% known bits = " << (100.0 * Bits / TotalBits) << "\n";
  
  return 0;
}
