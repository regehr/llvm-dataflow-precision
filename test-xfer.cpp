#include "llvm/ADT/APInt.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <memory>
#include <iostream>
#include <iomanip>

using namespace llvm;

namespace {

const int W = 4;

LLVMContext C;
IRBuilder<NoFolder> B(C);

Value *maskKnown(const KnownBits &K, Value *V) {
  auto O = B.CreateOr(V, K.One);
  auto A = B.CreateAnd(O, ~K.Zero);
  return A;
}

// slow, not clever, but obviously correct
bool nextKB1(KnownBits &K) {
  do {
    K.Zero += 1;
    if (K.Zero == 0) {
      K.One += 1;
      if (K.One == 0)
        return false;
    }
  } while (K.hasConflict());
  return true;
}

// fast, clever, but less clearly correct
bool nextKB(KnownBits &K) {
  auto NotOne = ~K.One;
  K.Zero = (K.Zero - NotOne) & NotOne;
  if (K.Zero == 0) {
    K.One += 1;
    if (K.One == 0)
      return false;
  }
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
      s += "x";
    Known.Zero <<= 1;
    Known.One <<= 1;
  }
  return s;
}

struct BinOp {
  Instruction::BinaryOps Opcode;
  bool nsw, nuw, exact;
};

int countInsns(BasicBlock *BB) {
  int count = 0;
  for (auto &I2 : *BB)
    ++count;
  return count;
}

void optimizeModule(llvm::Module *M) {
  auto FPM = new legacy::FunctionPassManager(M);
  PassManagerBuilder PB;
  PB.OptLevel = 2;
  PB.SizeLevel = 0;
  PB.populateFunctionPassManager(*FPM);
  PB.populateModulePassManager(*FPM);
  FPM->doInitialization();
  for (Function &F : *M) {
    outs() << "optimizing " << F.getName() << "\n";
    FPM->run(F);
  }
  FPM->doFinalization();
  delete FPM;
}

void test(const BinOp &Op) {
  auto M = std::make_unique<Module>("", C);
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

  KnownBits K0(W), K1(W);
  while (true) {
    auto I = B.CreateBinOp(Op.Opcode, maskKnown(K0, Args[0]), maskKnown(K1, Args[1]));
#if 0
    auto B = dyn_cast<BinOp>(I);
    B->setHasNoSignedWrap(Op.nsw);
    B->setHasNoUnsignedWrap(Op.nuw);
    B->setIsExact(Op.exact);
#endif
    auto R = B.CreateRet(I);

    int before = countInsns(BB);
    optimizeModule(M.get());
    int after = countInsns(BB);
    if (before != after)
      outs() << "  insns went from " << before << " to " << after << "\n";
    
    KnownBits KB = computeKnownBits(I, DL);
    Bits += KB.Zero.countPopulation() + KB.One.countPopulation();
    Cases++;

    if (false) {
      M->print(errs(), nullptr);
      outs() << KBString(KB) << "\n";
    }

    if (!nextKB(K0))
      if (!nextKB(K1))
        break;

#if 1
    // this is not good code but should be fine for a very small
    // number of instructions, as we have here
    while (!BB->empty()) {
      for (auto &I2 : *BB) {
        if (I2.hasNUses(0)) {
          I2.eraseFromParent();
          break;
        }
      }
    }
#endif
  }  

  outs() << Instruction::getOpcodeName(Op.Opcode) << " ";
  if (Op.nsw)
    outs() << "nsw ";
  if (Op.nuw)
    outs() << "nuw ";
  if (Op.exact)
    outs() << "exact ";
  outs() << "\n";
  outs() << "  total known bits = " << Bits << " (" << Cases << " cases)\n";
}

std::vector<BinOp> Ops {
  { Instruction::Add, false, false, false },
#if 1
  { Instruction::Add, true, false, false },
  { Instruction::Add, false, true, false },
  { Instruction::Add, true, true, false },
  { Instruction::Sub, false, false, false },
  { Instruction::Sub, true, false, false },
  { Instruction::Sub, false, true, false },
  { Instruction::Sub, true, true, false },
  { Instruction::Mul, false, false, false },
  { Instruction::Mul, true, false, false },
  { Instruction::Mul, false, true, false },
  { Instruction::Mul, true, true, false },
  { Instruction::UDiv, false, false, false },
  { Instruction::UDiv, false, false, true },
  { Instruction::SDiv, false, false, false },
  { Instruction::SDiv, false, false, true },
  { Instruction::URem, false, false, false },
  { Instruction::SRem, false, false, false },
  { Instruction::Shl, false, false, false },
  { Instruction::LShr, false, false, false },
  { Instruction::LShr, false, false, true },
  { Instruction::AShr, false, false, false },
  { Instruction::AShr, false, false, true },
  { Instruction::And, false, false, false },
  { Instruction::Or, false, false, false },
  { Instruction::Xor, false, false, false },
#endif
};
  
} // namespace

int main(void) {
  for (auto &Op : Ops)
    test(Op);
  return 0;
}
