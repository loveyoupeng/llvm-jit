#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <gtest/gtest.h>
#include <vector>

using namespace llvm;
using namespace llvm::orc;
int value = 20;
int plusOne(int value) { return value + 1; }
class Data {
public:
  static void setValueWrapper(Data *obj, int const value) {
    obj->setValue(value);
  }
  Data(int const value) : _value{value} {}
  void setValue(int const value) { _value = value; }
  int getValue() const { return _value; }

private:
  int _value;
};

TEST(JIT_METHOD, add) {
  ExitOnError ExitOnErr;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();

  auto jit = ExitOnErr(LLJITBuilder().create());
  auto context = std::make_unique<LLVMContext>();
  auto module = std::make_unique<Module>("lljit-test", *context);

  auto &lib = jit->getMainJITDylib();
  auto &layout = jit->getDataLayout();
  auto &session = jit->getExecutionSession();
  MangleAndInterner mangle(session, layout);
  ExitOnErr(lib.define(absoluteSymbols(
      {{mangle("plusOne"),
        JITEvaluatedSymbol(pointerToJITTargetAddress(&plusOne),
                           JITSymbolFlags::Exported)},
       {mangle("setValue"),
        JITEvaluatedSymbol(pointerToJITTargetAddress(&Data::setValueWrapper),
                           JITSymbolFlags::Exported)}})));

  StructType *type = StructType::create(*context, "Data");
  type->setBody(Type::getInt32Ty(*context));
  module->getOrInsertGlobal("Data", type);
  type->print(errs());

  Function *addFunc = Function::Create(
      FunctionType::get(
          Type::getInt32Ty(*context),
          {Type::getInt32Ty(*context), Type::getInt32Ty(*context)}, false),
      Function::ExternalLinkage, "add", module.get());

  BasicBlock *entry = BasicBlock::Create(*context, "entry", addFunc);
  IRBuilder<> builder(entry);

  Argument *x = nullptr;
  Argument *y = nullptr;
  for (auto &arg : addFunc->args()) {
    if (x) {
      y = &arg;
      arg.setName("y");
    } else {
      x = &arg;
      arg.setName("x");
    }
  }

  Value *sum = builder.CreateAdd(x, y, "sum");
  Function *plusOne =
      Function::Create(FunctionType::get(Type::getInt32Ty(*context),
                                         {Type::getInt32Ty(*context)}, false),
                       Function::ExternalLinkage, "plusOne", module.get());
  std::vector<Value *> args;
  args.push_back(sum);
  Value *result = builder.CreateCall(plusOne, args, "result");
  builder.CreateRet(result);

  Function *setValue = Function::Create(
      FunctionType::get(Type::getVoidTy(*context),
                        {type->getPointerTo(), Type::getInt32Ty(*context)},
                        false),
      Function::ExternalLinkage, "setValue", module.get());
  Function *setDataValue = Function::Create(
      FunctionType::get(Type::getVoidTy(*context),
                        {type->getPointerTo(), Type::getInt32Ty(*context)},
                        false),
      Function::ExternalLinkage, "setDataValue", module.get());
  entry = BasicBlock::Create(*context, "entry", setDataValue);
  IRBuilder<> builder1(entry);

  Argument *d = nullptr;
  Argument *v = nullptr;
  for (auto &arg : setDataValue->args()) {
    if (d) {
      v = &arg;
      arg.setName("value");
    } else {
      d = &arg;
      arg.setName("data");
    }
  }

  auto pointer =
      builder1.CreateAlloca(type->getPointerTo(), nullptr, "pointer");
  auto valuePtr =
      builder1.CreateAlloca(Type::getInt32Ty(*context), nullptr, "vPtr");
  builder1.CreateStore(d, pointer);
  builder1.CreateStore(v, valuePtr);
  auto dPtr = builder1.CreateLoad(type->getPointerTo(), pointer, "dataPtr");
  auto vPtr =
      builder1.CreateLoad(Type::getInt32Ty(*context), valuePtr, "valuePtr");
  auto r = builder1.CreateCall(setValue, {dPtr, vPtr});
  builder1.CreateRetVoid();

  for (auto &func : module->functions()) {
    func.print(errs());
  }

  auto threadSafeModule =
      ThreadSafeModule(std::move(module), std::move(context));

  ExitOnErr(jit->addIRModule(std::move(threadSafeModule)));
  auto symbol = ExitOnErr(jit->lookup("add"));
  int (*add)(int, int) = (int (*)(int, int))symbol.getAddress();

  auto sym = ExitOnErr(jit->lookup("setDataValue"));
  void (*sdv)(Data *, int) = ((void (*)(Data *, int))sym.getAddress());
  Data data{10};
  ASSERT_EQ(10, data.getValue());
  sdv(&data, 100);
  ASSERT_EQ(100, data.getValue());

  ASSERT_EQ(4, add(1, 2));
}
