#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

TEST(JitTest, method_ptr) {
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  std::unique_ptr<KaleidoscopeJIT> jit = std::make_unique<KaleidoscopeJIT>();
  LLVMContext context{};
  IRBuilder<> builder(context);
  std::unique_ptr<Module> module =
      std::make_unique<Module>("jit_module", context);
  module->setDataLayout(jit->getTargetMachine().createDataLayout());

  std::vector<Type *> ints(2, Type::getInt32Ty(context));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(context), ints, false);
  Function *f = Function::Create(ft, Function::ExternalLinkage, "jitted_method",
                                 module.get());
  Value *x = nullptr;
  Value *y = nullptr;
  for (auto &arg : f->args()) {
    if (x) {
      y = &arg;
      arg.setName(std::string("y"));
    } else {
      x = &arg;
      arg.setName(std::string("x"));
    }
  }
  std::cout << 1 << ' ' << x->getName().str() << std::endl;
  std::cout << 2 << ' ' << y->getName().str() << std::endl;
  BasicBlock *bb = BasicBlock::Create(context, "entry", f);
  std::cout << 3 << std::endl;
  builder.SetInsertPoint(bb);
  Value *xpy = builder.CreateAdd(x, y, "xpy");
  Value *ypx = builder.CreateAdd(y, x, "ypx");
  Value *result = builder.CreateMul(xpy, ypx, "result");
  builder.CreateRet(result);

  verifyFunction(*f);
  f->print(errs());
  auto key = jit->addModule(std::move(module));
  auto func = jit->findSymbol("jitted_method");
  int (*func_ptr)(int, int);
  func_ptr = (int (*)(int, int))(intptr_t)cantFail(func.getAddress());

  ASSERT_EQ(9, func_ptr(1, 2));
  std::cout << "func_ptr(1, 2)=" << func_ptr(1, 2) << std::endl;
  jit->removeModule(key);
}
