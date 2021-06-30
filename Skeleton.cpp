#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

namespace {
struct SkeletonPass : public FunctionPass {
    static char ID;
    SkeletonPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
        // iterte over basic blocks in function, doing the vectorized adds
        // within each basic block
        for (auto &B : F) {
            // init types for vectors
            Type *leftVectorType = NULL;
            Type *rightVectorType = NULL;
            // iterate over instructions and get the appropriate types for each
            // vector
            // also add instructions to a vector of instructions
            for (auto &I : B) {
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    if (I.getOpcode() == Instruction::Add) {
                        Value *lhs = op->getOperand(0);
                        Value *rhs = op->getOperand(1);
                        leftVectorType = VectorType::get(lhs->getType(), 4);
                        rightVectorType = VectorType::get(rhs->getType(), 4);
                        break;  // assume all binary ops of same type
                    }
                }
            }
            // there were no binary operators
            if ((leftVectorType == NULL) || (rightVectorType == NULL)) {
                return false;
            }
            std::vector<BinaryOperator *> vec;
            // create vectors
            Value *leftVector = UndefValue::get(leftVectorType);
            Value *rightVector = UndefValue::get(rightVectorType);
            // place new instructions at end of basic block
            errs() << "Before cast:\n";
            IRBuilder<> builder(&B);
            int64_t count = 0;
            // iterate over instructions to see if they are binary operator adds
            // and then add to the appropriate vectors
            errs() << "Before getting operands:\n";
            for (auto &I : B) {
                if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                    if (I.getOpcode() == Instruction::Add) {
                        Value *lhs = op->getOperand(0);
                        Value *rhs = op->getOperand(1);
                        builder.CreateInsertElement(leftVector, lhs, count);
                        builder.CreateInsertElement(rightVector, rhs, count);
                        vec.push_back(op);
                        ++count;
                    }
                }
            }

            errs() << "Before assigning uses:\n";
            Value *sumVector = builder.CreateAdd(leftVector, rightVector);
            errs() << *sumVector << ":\n";
            for (int64_t i = 0; i < count; i++) {
                Value *sumElt = builder.CreateExtractElement(sumVector, i);
                BinaryOperator *instr = vec[i];
                errs() << *instr << ":\n";
                errs() << *sumElt << ":\n";
                for (auto &U : instr->uses()) {
                    User *user = U.getUser();
                    errs() << "Before setting operand:\n";
                    user->setOperand(U.getOperandNo(), sumElt);
                }
            }
        }
        return true;
    }
};
}  // namespace

char SkeletonPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSkeletonPass(const PassManagerBuilder &,
                                 legacy::PassManagerBase &PM) {
    PM.add(new SkeletonPass());
}
static RegisterStandardPasses RegisterMyPass(
    PassManagerBuilder::EP_EarlyAsPossible, registerSkeletonPass);
