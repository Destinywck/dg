#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include "PointerSubgraphValidator.h"

namespace dg {
namespace analysis {
namespace pta {
namespace debug {

static const llvm::Value *getValue(const PSNode *nd) {
    return nd->getUserData<llvm::Value>();
}

bool LLVMPointerSubgraphValidator::reportInvalNumberOfOperands(const PSNode *nd) {
    // just check whether the PHI is a pointer type. If it is a number,
    // we do not know whether it is an error.
    if (nd->getType() == PSNodeType::PHI) {
        const llvm::Value *val = getValue(nd);
        assert(val);

        if (val->getType()->isPointerTy()) {
            // this is the PHI node that corresponds to argv,
            // we're fine here
            if (isa<llvm::Argument>(val) && nd->getParent() &&
                nd->getParent()->getParent() == nullptr)
                return false;

            return PointerSubgraphValidator::reportInvalNumberOfOperands(nd);
        } else // else issue a warning?
            return false;
    }

    return PointerSubgraphValidator::reportInvalNumberOfOperands(nd);
}


} // namespace debug
} // namespace pta
} // namespace analysis
} // namespace dg

