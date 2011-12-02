#ifndef __LDC_GEN_ABI_H__
#define __LDC_GEN_ABI_H__

#include <vector>

struct Type;
struct IrFuncTyArg;
struct DValue;

namespace llvm
{
    class Type;
    class Value;
}

// return rewrite rule
struct ABIRewrite
{
    /// get a rewritten value back to its original form
    virtual LLValue* get(Type* dty, DValue* v) = 0;

    /// get a rewritten value back to its original form and store result in provided lvalue
    /// this one is optional and defaults to calling the one above
    virtual void getL(Type* dty, DValue* v, llvm::Value* lval);

    /// put out rewritten value
    virtual LLValue* put(Type* dty, DValue* v) = 0;

    /// should return the transformed type for this rewrite
    virtual LLType* type(Type* dty, LLType* t) = 0;
};

// interface called by codegen
struct TargetABI
{
    /// Returns the ABI for the target we're compiling for
    static TargetABI* getTarget();
    /// Returns the ABI for intrinsics
    static TargetABI* getIntrinsic();

    virtual void newFunctionType(TypeFunction* tf) {}
    virtual bool returnInArg(TypeFunction* tf) = 0;
    virtual bool passByVal(Type* t) = 0;
    virtual void doneWithFunctionType() {}

    virtual void rewriteFunctionType(TypeFunction* t) = 0;
};

#endif
