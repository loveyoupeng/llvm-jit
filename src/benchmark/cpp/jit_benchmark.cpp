#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
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
#include <benchmark/benchmark.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;
/*
class Fixture : public benchmark::Fixture {
public:
  long before = 0;
  long after = 0;
  void SetUp(const ::benchmark::State &state) { before++; }

  void TearDown(const ::benchmark::State &state) { after++; }
};
BENCHMARK_F(Fixture, fixture_benchmark)(benchmark::State &state) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(before + after);
  }
  std::cout << "Setup=" << before << std::endl;
  std::cout << "TearDown=" << after << std::endl;
}
*/

double method(double x, double y) { return (x + y) * (y + x); }

double (*method_ptr)(double, double) = method;

static void JitBenchmarkBaseline(benchmark::State &state) {
  double const x = double(state.range(0));
  double const y = double(state.range(1));
  for (auto _ : state) {
    benchmark::DoNotOptimize(method(x, y));
  }
}

BENCHMARK(JitBenchmarkBaseline)->Arg(1 << 10)->Arg(1 << 16);

static void JitBenchmarkMethodPtr(benchmark::State &state) {
  double const x = double(state.range(0));
  double const y = double(state.range(1));
  for (auto _ : state) {
    benchmark::DoNotOptimize(method_ptr(x, y));
  }
}

BENCHMARK(JitBenchmarkMethodPtr)->Arg(1 << 10)->Arg(1 << 16);

static void JitBenchmark(benchmark::State &state) {
  double const a = double(state.range(0));
  double const b = double(state.range(1));
  std::unique_ptr<KaleidoscopeJIT> jit;
  if (jit)
    return;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  jit = std::make_unique<KaleidoscopeJIT>();
  LLVMContext context{};
  IRBuilder<> builder(context);
  std::unique_ptr<Module> module =
      std::make_unique<Module>("jit_module", context);
  module->setDataLayout(jit->getTargetMachine().createDataLayout());

  std::vector<Type *> Doubles(2, Type::getDoubleTy(context));
  FunctionType *ft =
      FunctionType::get(Type::getDoubleTy(context), Doubles, false);
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
  BasicBlock *bb = BasicBlock::Create(context, "entry", f);
  builder.SetInsertPoint(bb);
  Value *xpy = builder.CreateFAdd(x, y, "xpy");
  Value *ypx = builder.CreateFAdd(y, x, "ypx");
  Value *result = builder.CreateFMul(xpy, ypx, "result");
  builder.CreateRet(result);

  verifyFunction(*f);

  auto key = jit->addModule(std::move(module));
  auto func = jit->findSymbol("jitted_method");
  double (*func_ptr)(double, double) = nullptr;
  func_ptr = (double (*)(double, double))(intptr_t)cantFail(func.getAddress());
  for (auto _ : state) {
    benchmark::DoNotOptimize(func_ptr(a, b));
  }
  jit->removeModule(key);
}

BENCHMARK(JitBenchmark)->Arg(1 << 10)->Arg(1 << 16);

class AST {
public:
  virtual ~AST(){};
  virtual double execute() const = 0;
};

class Const : public AST {
public:
  Const(double const value) : _value(value) {}
  double execute() const override final { return _value; }

private:
  double const _value;
};

class Add : public AST {
public:
  Add(AST const *const left, AST const *const right)
      : _left(left), _right(right){};
  double execute() const override final {
    return _left->execute() + _right->execute();
  }

private:
  AST const *const _left;
  AST const *const _right;
};

class Mul : public AST {
public:
  Mul(AST const *const left, AST const *const right)
      : _left(left), _right(right){};
  double execute() const override final {
    return _left->execute() * _right->execute();
  }

private:
  AST const *const _left;
  AST const *const _right;
};

static void ASTBenchmarkMethodPtr(benchmark::State &state) {
  double const a = double(state.range(0));
  double const b = double(state.range(1));
  AST const *const x = new Const{a};
  AST const *const y = new Const{b};
  AST const *const xpy = new Add{x, y};
  AST const *const ypx = new Add{y, x};
  AST const *const result = new Mul{xpy, ypx};

  for (auto _ : state) {
    benchmark::DoNotOptimize(result->execute());
  }

  delete result;
  delete ypx;
  delete xpy;
  delete y;
  delete x;
}
BENCHMARK(ASTBenchmarkMethodPtr)->Arg(1 << 10)->Arg(1 << 16);
BENCHMARK_MAIN();
