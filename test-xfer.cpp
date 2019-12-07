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

const int Width = 4;

LLVMContext C;
IRBuilder<> B(C);

} // namespace
  
int main(void) {
  auto M = make_unique<Module>("awesome module", C);
  

  return 0;
}
