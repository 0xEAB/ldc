#include "gen/llvm.h"
#include "gen/tollvm.h"
#include "gen/irstate.h"
#include "gen/runtime.h"
#include "gen/logger.h"
#include "gen/classes.h"
#include "gen/llvmhelpers.h"
#include "ir/irlandingpad.h"

IRLandingPadInfo::IRLandingPadInfo(Catch* catchstmt, llvm::BasicBlock* end)
: finallyBody(NULL)
{
    target = llvm::BasicBlock::Create(gIR->context(), "catch", gIR->topfunc(), end);
    gIR->scope() = IRScope(target,end);

    // assign storage to catch var
    if(catchstmt->var) {
        // use the same storage for all exceptions that are not accessed in
        // nested functions
    #if DMDV2
        if(!catchstmt->var->nestedrefs.dim) {
    #else
        if(!catchstmt->var->nestedref) {
    #endif
            assert(!catchstmt->var->ir.irLocal);
            catchstmt->var->ir.irLocal = new IrLocal(catchstmt->var);
            LLValue* catch_var = gIR->func()->gen->landingPadInfo.getExceptionStorage();
            catchstmt->var->ir.irLocal->value = gIR->ir->CreateBitCast(catch_var, getPtrToType(DtoType(catchstmt->var->type)));
        }

        // this will alloca if we haven't already and take care of nested refs
        DtoDeclarationExp(catchstmt->var);

        // the exception will only be stored in catch_var. copy it over if necessary
        if(catchstmt->var->ir.irLocal->value != gIR->func()->gen->landingPadInfo.getExceptionStorage()) {
            LLValue* exc = gIR->ir->CreateBitCast(DtoLoad(gIR->func()->gen->landingPadInfo.getExceptionStorage()), DtoType(catchstmt->var->type));
            DtoStore(exc, catchstmt->var->ir.irLocal->value);
        }
    }

    // emit handler, if there is one
    // handler is zero for instance for 'catch { debug foo(); }'
    if(catchstmt->handler)
        catchstmt->handler->toIR(gIR);

    if (!gIR->scopereturned())
        gIR->ir->CreateBr(end);

    assert(catchstmt->type);
    catchType = catchstmt->type->toBasetype()->isClassHandle();
    assert(catchType);
    catchType->codegen(Type::sir);
}

IRLandingPadInfo::IRLandingPadInfo(Statement* finallystmt)
: target(NULL), finallyBody(finallystmt), catchType(NULL)
{

}


void IRLandingPad::addCatch(Catch* catchstmt, llvm::BasicBlock* end)
{
    unpushed_infos.push_front(IRLandingPadInfo(catchstmt, end));
}

void IRLandingPad::addFinally(Statement* finallystmt)
{
    unpushed_infos.push_front(IRLandingPadInfo(finallystmt));
}

void IRLandingPad::push(llvm::BasicBlock* inBB)
{
    // store infos such that matches are right to left
    nInfos.push(infos.size());
    infos.insert(infos.end(), unpushed_infos.begin(), unpushed_infos.end());
    unpushed_infos.clear();

    constructLandingPad(inBB);

    // store as invoke target
    padBBs.push(inBB);
}

void IRLandingPad::pop()
{
    padBBs.pop();

    size_t n = nInfos.top();
    infos.resize(n);
    nInfos.pop();
}

llvm::BasicBlock* IRLandingPad::get()
{
    if(padBBs.size() == 0)
        return NULL;
    else
        return padBBs.top();
}

void IRLandingPad::constructLandingPad(llvm::BasicBlock* inBB)
{
    // save and rewrite scope
    IRScope savedscope = gIR->scope();
    gIR->scope() = IRScope(inBB,savedscope.end);

    // eh_ptr = llvm.eh.exception();
    llvm::Function* eh_exception_fn = GET_INTRINSIC_DECL(eh_exception);
    LLValue* eh_ptr = gIR->ir->CreateCall(eh_exception_fn);

    // build selector arguments
    LLSmallVector<LLValue*, 6> selectorargs;

    // put in classinfos in the right order
    bool hasFinally = false;
    bool hasCatch = false;
    std::deque<IRLandingPadInfo>::iterator it = infos.begin(), end = infos.end();
    for(; it != end; ++it)
    {
        if(it->finallyBody)
            hasFinally = true;
        else
        {
            hasCatch = true;
            assert(it->catchType);
            assert(it->catchType->ir.irStruct);
            selectorargs.insert(selectorargs.begin(), it->catchType->ir.irStruct->getClassInfoSymbol());
        }
    }
    // if there's a finally, the eh table has to have a 0 action
    if(hasFinally)
        selectorargs.push_back(DtoConstUint(0));

    // personality fn
    llvm::Function* personality_fn = LLVM_D_GetRuntimeFunction(gIR->module, "_d_eh_personality");
    LLValue* personality_fn_arg = gIR->ir->CreateBitCast(personality_fn, getPtrToType(LLType::getInt8Ty(gIR->context())));
    selectorargs.insert(selectorargs.begin(), personality_fn_arg);

    // eh storage target
    selectorargs.insert(selectorargs.begin(), eh_ptr);

    // if there is a catch and some catch allocated storage, store exception object
    if(hasCatch && catch_var)
    {
        const LLType* objectTy = DtoType(ClassDeclaration::object->type);
        gIR->ir->CreateStore(gIR->ir->CreateBitCast(eh_ptr, objectTy), catch_var);
    }

    // eh_sel = llvm.eh.selector(eh_ptr, cast(byte*)&_d_eh_personality, <selectorargs>);
    llvm::Function* eh_selector_fn = GET_INTRINSIC_DECL(eh_selector);
    LLValue* eh_sel = gIR->ir->CreateCall(eh_selector_fn, selectorargs.begin(), selectorargs.end());

    // emit finallys and 'if' chain to catch the exception
    llvm::Function* eh_typeid_for_fn = GET_INTRINSIC_DECL(eh_typeid_for);
    std::deque<IRLandingPadInfo> infos = this->infos;
    std::stack<size_t> nInfos = this->nInfos;
    std::deque<IRLandingPadInfo>::reverse_iterator rit, rend = infos.rend();
    for(rit = infos.rbegin(); rit != rend; ++rit)
    {
        // if it's a finally, emit its code
        if(rit->finallyBody)
        {
            size_t n = this->nInfos.top();
            this->infos.resize(n);
            this->nInfos.pop();
            rit->finallyBody->toIR(gIR);
        }
        // otherwise it's a catch and we'll add a if-statement
        else
        {
            llvm::BasicBlock *next = llvm::BasicBlock::Create(gIR->context(), "eh.next", gIR->topfunc(), gIR->scopeend());
            LLValue *classInfo = DtoBitCast(rit->catchType->ir.irStruct->getClassInfoSymbol(),
                                            getPtrToType(DtoType(Type::tint8)));
            LLValue *eh_id = gIR->ir->CreateCall(eh_typeid_for_fn, classInfo);
            gIR->ir->CreateCondBr(gIR->ir->CreateICmpEQ(eh_sel, eh_id), rit->target, next);
            gIR->scope() = IRScope(next, gIR->scopeend());
        }
    }

    // restore landing pad infos
    this->infos = infos;
    this->nInfos = nInfos;

    // no catch matched and all finallys executed - resume unwind
    llvm::Function* unwind_resume_fn = LLVM_D_GetRuntimeFunction(gIR->module, "_d_eh_resume_unwind");
    gIR->ir->CreateCall(unwind_resume_fn, eh_ptr);
    gIR->ir->CreateUnreachable();

    gIR->scope() = savedscope;
}

LLValue* IRLandingPad::getExceptionStorage()
{
    if(!catch_var)
    {
        Logger::println("Making new catch var");
        catch_var = DtoAlloca(ClassDeclaration::object->type, "catchvar");
    }
    return catch_var;
}
