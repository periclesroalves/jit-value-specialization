/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IonBuilder.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonAnalysis.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

#ifdef DEBUG

#define CP_PASS_ARGS 0
#define CP_CALL 1
#define CP_CREATE_THIS 2
#define CP_STORE_ELEMENTS 3
#define CP_SLOTS 4
#define CP_TOTAL 5
void
ion::CheckInstructionsWithConstantOperands(MIRGraph &graph){
	int analyzeInstructions[CP_TOTAL];
	if (IonSpewEnabled(IonSpew_CP)) {
		IonSpewHeader(IonSpew_CP);
		fprintf(IonSpewFile, "Instructions with only constant operands:\n");
	}
	MBasicBlockIterator itEndBlock = graph.end();
	for (MBasicBlockIterator itBlock = graph.begin(); itBlock != itEndBlock; itBlock++) {
		MBasicBlock *block = *itBlock;
		MInstructionIterator itEndIns = block->end();
		for (MInstructionIterator itIns = block->begin(); itIns != itEndIns; itIns++) {
			MInstruction *ins = *itIns;
			if(!ins->isConstant() && ins->numOperands() > 0){
				bool allOperandsAreConstant = true;
				for(size_t i = 0; i < ins->numOperands(); i++){
					if(!ins->getOperand(i)->isConstant())
						allOperandsAreConstant = false;
				}

				if(allOperandsAreConstant){
					if (IonSpewEnabled(IonSpew_CP)) {
						IonSpewHeader(IonSpew_CP);
						ins->printOpcode(IonSpewFile);
						fprintf(IonSpewFile, "\n");
					}
				}
			}
			if(ins->isPassArg())
				analyzeInstructions[CP_PASS_ARGS]++;
			else if(ins->isCall())
				analyzeInstructions[CP_CALL]++;
			else if(ins->isCreateThis())
				analyzeInstructions[CP_CREATE_THIS]++;
			else if(ins->isStoreElement())
				analyzeInstructions[CP_STORE_ELEMENTS]++;
			else if(ins->isSlots())
				analyzeInstructions[CP_SLOTS]++;
		}
	}
//	IonSpew(IonSpew_CP, "Instructions statistics");
//	for(){
//		IonSpew(IonSpew_CP, );
//	}
}
#endif

// Some blocks become joinable with their predecessors after inlining.
// This function joins these blocks.
bool
ion::CoalesceBlocks(MIRGraph &graph)
{
    MBasicBlockIterator itEndBlock = graph.end();
    for (MBasicBlockIterator itBlock = graph.begin(), itNextBlock = itBlock;
         itNextBlock++, itBlock != itEndBlock; itBlock = itNextBlock) {
        MBasicBlock *block = *itBlock;
        if (block->numPredecessors() != 1)
            continue;

        MBasicBlock *pred = block->getPredecessor(0);
        MInstruction *predLastIns = pred->lastIns();
        if (pred->numSuccessors() != 1 || !predLastIns->isGoto())
            continue;

        bool canCoalesce = true;
        MInstructionIterator itEndIns = block->end();
        // Check if the block has any unmovable instruction.
        for (MInstructionIterator itIns = block->begin(); itIns != itEndIns; itIns++) {
            if (itIns->isGetPropertyCache() || itIns->isGetElementCache() ||
                itIns->isCallGetElement()) {
                canCoalesce = false;
                break;
            }
        }

        if (!canCoalesce)
            continue;

        if (!block->phisEmpty()) {
            // Replace all one-parameter phis with their only operand.
            MPhiIterator itEndPhis = block->phisEnd();
            for (MPhiIterator itPhi = block->phisBegin(); itPhi != itEndPhis; ) {
                if (itPhi->numOperands() == 1) {
                    itPhi->replaceAllUsesWith(itPhi->getOperand(0));
                    itPhi = block->discardPhiAt(itPhi);
                }
            }
        }

        // Move all instructions to the predecessor.
        for (MInstructionIterator itIns = block->begin(), itNextIns = itIns; itNextIns++,
             itNextIns != itEndIns; itIns = itNextIns)
            block->moveBefore(predLastIns, *itIns);

        // Update the predecessor list for each successor of the block to remove.
        for (uint32 i = 0; i < block->numSuccessors(); i++)
            block->getSuccessor(i)->replacePredecessor(block, pred);

        IonSpew(IonSpew_BCoal, "Coalescing block %d to %d", block->id(), pred->id());

        pred->discardLastIns();
        pred->end(block->lastIns());
        graph.removeBlock(block);
    }
    return true;
}

// Eliminates conditionals with completely folded conditions.
bool
ion::DCEConditionals(MIRGraph &graph)
{
    bool* deleteBlock = new bool[graph.numBlockIds()];

    for (uint32 i = 0; i < graph.numBlockIds(); i++)
        deleteBlock[i] = false;

    MBasicBlockIterator itEndBlock = graph.end();

    for (ReversePostorderIterator itBlock = graph.rpoBegin(), itNextBlock = itBlock;
         itNextBlock++, itBlock != graph.rpoEnd(); itBlock = itNextBlock) {
        MBasicBlock *block = *itBlock;

        // Skip over blocks marked for deletion.
        if (deleteBlock[block->id()])
            continue;

        MControlInstruction *lastIns = block->lastIns();
        if (lastIns->isTest()) {
            JS_ASSERT(lastIns->numOperands() == 1);
            JS_ASSERT(lastIns->getOperand(0)->isDefinition());
            MDefinition *lastInsOper = lastIns->getOperand(0);
            if (lastInsOper->isConstant()) {
                Value boolOperVal = static_cast<MConstant *>(lastInsOper)->value();
                // May be undefined.
                if (!boolOperVal.isBoolean())
                    continue;

                // Block from where the branch for deletion starts.
                MBasicBlock *deleteBranchStart = block->getSuccessor(boolOperVal.toBoolean());
                // Block that starts the branch to keep.
                MBasicBlock *keepBranchStart = block->getSuccessor(!boolOperVal.toBoolean());

                // Rare condition, not worth fixing.
                if (deleteBranchStart->numSuccessors() <= 2 &&
                    (deleteBranchStart->isLoopHeader() || deleteBranchStart->isLoopBackedge()))
                   continue;

                deleteBlock[deleteBranchStart->id()] = true;

                IonSpew(IonSpew_DCEC, "Marking conditional starting at block %d for removal", 
                        deleteBranchStart->id());

                // Just replace the last instruction, don't update any predecessor lists.
                block->discardLastIns();
                block->end(MGoto::New(keepBranchStart));

                ReversePostorderIterator itDel = graph.begin(deleteBranchStart);
                bool first = true;
                MBasicBlock *lastBranchBlock = block;

                deleteBlock[block->id()] = true;
                for (; itDel != graph.rpoEnd(); itDel++) {
                    MBasicBlock *delBlock = *itDel;
                    bool branchOver = false;
                    bool atLeastOneDel = false;

                    for (uint32 i = 0; i < delBlock->numPredecessors(); i++) {
                        if (!deleteBlock[delBlock->getPredecessor(i)->id()]) {
                            deleteBlock[delBlock->id()] = false;
                            branchOver = true;
                        } else {
                            atLeastOneDel = true;
                            if (branchOver)
                                break;
                        }
                    }

                    if (branchOver && atLeastOneDel) {
                        if (!lastBranchBlock || lastBranchBlock->numSuccessors() == 0)
                            break;

                        uint32 index = 0;

                        if (!first)
                            delBlock = lastBranchBlock->getSuccessor(0);

                        for (uint32 i = 0; i < delBlock->numPredecessors(); i++) {
                            if (delBlock->getPredecessor(i) == lastBranchBlock) {
                                index = i;
                                break;
                            }
                        }

                        for (MPhiIterator itPhi = delBlock->phisBegin(); 
                             itPhi != delBlock->phisEnd(); ) {
                            if (itPhi->numOperands() == 2) {
                                itPhi->replaceAllUsesWith(itPhi->getOperand(!index));
                                itPhi = delBlock->discardPhiAt(itPhi);
                            } else {
                                itPhi->removeOperand(index);
                                itPhi++;
                            }
                        }

                        if (first) {
                            deleteBlock[block->id()] = false;
                        } else {
                            for (uint32 j = 0; j < delBlock->numPredecessors(); j++) {
                                 if (delBlock->getPredecessor(j) == lastBranchBlock) {
                                     delBlock->removePredecessor(j);
                                     if (delBlock->numPredecessors() == 0)
                                         deleteBlock[delBlock->id()] = true; 
                                     break;
                                 }
                            }
                        }
                        break;
                    }

                    if (first) {
                        first = false;
                        deleteBlock[block->id()] = false;
                    }

                    if (atLeastOneDel) {
                        deleteBlock[delBlock->id()] = true;
                        IonSpew(IonSpew_DCEC, "Marking block %d for removal", delBlock->id());
                        lastBranchBlock = delBlock;
                    }
                }

                for (uint32 j = 0; j < deleteBranchStart->numPredecessors(); j++) {
                    if (deleteBranchStart->getPredecessor(j) == block) {
                        deleteBranchStart->removePredecessor(j);
                        if (deleteBranchStart->numPredecessors() == 0)
                            deleteBlock[deleteBranchStart->id()] = true; 
                        break;
                    }
                }
            }
        }
    }

    for (MBasicBlockIterator itBlock = graph.begin(), itNextBlock = itBlock; 
         itNextBlock++, itBlock != itEndBlock; itBlock = itNextBlock) {
        MBasicBlock *block = *itBlock;
        if (deleteBlock[block->id()]) {
            for (MInstructionIterator itIns = block->begin(), itNextIns = itIns; itNextIns++, 
                 itNextIns != block->end(); itIns = itNextIns)
                block->discardAt(itIns);

            for (MPhiIterator itPhi = block->phisBegin(); itPhi != block->phisEnd(); )
                itPhi = block->discardPhiAt(itPhi);

            for (uint32 i = 0; i < block->numSuccessors(); i++) {
                MBasicBlock *succ = block->getSuccessor(i);
                for (uint32 j = 0; j < succ->numPredecessors(); j++) {
                    if (succ->getPredecessor(j) == block) {
                        succ->removePredecessor(j);
                        break;
                    }
                }
            }

            block->discardLastIns();
            graph.removeBlock(block);
        }
    }

    delete[] deleteBlock;
    return true;
}

// A critical edge is an edge which is neither its successor's only predecessor
// nor its predecessor's only successor. Critical edges must be split to
// prevent copy-insertion and code motion from affecting other edges.
bool
ion::SplitCriticalEdges(MIRGenerator *gen, MIRGraph &graph)
{
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->numSuccessors() < 2)
            continue;
        for (size_t i = 0; i < block->numSuccessors(); i++) {
            MBasicBlock *target = block->getSuccessor(i);
            if (target->numPredecessors() < 2)
                continue;

            // Create a new block inheriting from the predecessor.
            MBasicBlock *split = MBasicBlock::NewSplitEdge(graph, block->info(), *block);
            split->setLoopDepth(block->loopDepth());
            graph.insertBlockAfter(*block, split);
            split->end(MGoto::New(target));

            block->replaceSuccessor(i, split);
            target->replacePredecessor(*block, split);
        }
    }
    return true;
}

// Instructions are useless if they are unused and have no side effects.
// This pass eliminates useless instructions.
// The graph itself is unchanged.
bool
ion::EliminateDeadCode(MIRGraph &graph)
{
    // Traverse in postorder so that we hit uses before definitions.
    // Traverse instruction list backwards for the same reason.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        // Remove unused instructions.
        for (MInstructionReverseIterator inst = block->rbegin(); inst != block->rend(); ) {
            if (!inst->isEffectful() && !inst->hasUses() && !inst->isGuard() &&
                !inst->isControlInstruction()) {
                inst = block->discardAt(inst);
            } else {
                inst++;
            }
        }
    }

    return true;
}

static inline bool
IsPhiObservable(MPhi *phi)
{
    // If the phi has bytecode uses, there may be no SSA uses but the value
    // is still observable in the interpreter after a bailout.
    if (phi->hasBytecodeUses())
        return true;

    // Check for any SSA uses. Note that this skips reading resume points,
    // which we don't count as actual uses. If the only uses are resume points,
    // then the SSA name is never consumed by the program.
    for (MUseDefIterator iter(phi); iter; iter++) {
        if (!iter.def()->isPhi())
            return true;
    }

    // If the Phi is of the |this| value, it must always be observable.
    if (phi->slot() == 1)
        return true;
    return false;
}

static inline MDefinition *
IsPhiRedundant(MPhi *phi)
{
    MDefinition *first = phi->getOperand(0);

    for (size_t i = 1; i < phi->numOperands(); i++) {
        if (phi->getOperand(i) != first && phi->getOperand(i) != phi)
            return NULL;
    }

    // Propagate the HasBytecodeUses flag if |phi| is replaced with
    // another phi.
    if (phi->hasBytecodeUses() && first->isPhi())
        first->toPhi()->setHasBytecodeUses();

    return first;
}

bool
ion::EliminatePhis(MIRGraph &graph)
{
    Vector<MPhi *, 16, SystemAllocPolicy> worklist;

    // Add all observable phis to a worklist. We use the "in worklist" bit to
    // mean "this phi is live".
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        MPhiIterator iter = block->phisBegin();
        while (iter != block->phisEnd()) {
            // Flag all as unused, only observable phis would be marked as used
            // when processed by the work list.
            iter->setUnused();

            // If the phi is redundant, remove it here.
            if (MDefinition *redundant = IsPhiRedundant(*iter)) {
                iter->replaceAllUsesWith(redundant);
                iter = block->discardPhiAt(iter);
                continue;
            }

            // Enqueue observable Phis.
            if (IsPhiObservable(*iter)) {
                iter->setInWorklist();
                if (!worklist.append(*iter))
                    return false;
            }
            iter++;
        }
    }

    // Iteratively mark all phis reachable from live phis.
    while (!worklist.empty()) {
        MPhi *phi = worklist.popCopy();
        JS_ASSERT(phi->isUnused());
        phi->setNotInWorklist();

        // The removal of Phis can produce newly redundant phis.
        if (MDefinition *redundant = IsPhiRedundant(phi)) {
            phi->replaceAllUsesWith(redundant);
            if (redundant->isPhi())
                redundant->setUnusedUnchecked();
        } else {
            // Otherwise flag them as used.
            phi->setNotUnused();
        }

        for (size_t i = 0; i < phi->numOperands(); i++) {
            MDefinition *in = phi->getOperand(i);
            if (!in->isPhi() || !in->isUnused() || in->isInWorklist())
                continue;
            in->setInWorklist();
            if (!worklist.append(in->toPhi()))
                return false;
        }
    }

    // Sweep dead phis.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        MPhiIterator iter = block->phisBegin();
        while (iter != block->phisEnd()) {
            if (iter->isUnused())
                iter = block->discardPhiAt(iter);
            else
                iter++;
        }
    }

    return true;
}

// The type analysis algorithm inserts conversions and box/unbox instructions
// to make the IR graph well-typed for future passes.
//
// Phi adjustment: If a phi's inputs are all the same type, the phi is
// specialized to return that type.
//
// Input adjustment: Each input is asked to apply conversion operations to its
// inputs. This may include Box, Unbox, or other instruction-specific type
// conversion operations.
//
class TypeAnalyzer
{
    MIRGraph &graph;
    Vector<MPhi *, 0, SystemAllocPolicy> phiWorklist_;

    bool addPhiToWorklist(MPhi *phi) {
        if (phi->isInWorklist())
            return true;
        if (!phiWorklist_.append(phi))
            return false;
        phi->setInWorklist();
        return true;
    }
    MPhi *popPhi() {
        MPhi *phi = phiWorklist_.popCopy();
        phi->setNotInWorklist();
        return phi;
    }

    bool respecialize(MPhi *phi, MIRType type);
    bool propagateSpecialization(MPhi *phi);
    bool specializePhis();
    void replaceRedundantPhi(MPhi *phi);
    void adjustPhiInputs(MPhi *phi);
    bool adjustInputs(MDefinition *def);
    bool insertConversions();

  public:
    TypeAnalyzer(MIRGraph &graph)
      : graph(graph)
    { }

    bool analyze();
};

// Try to specialize this phi based on its non-cyclic inputs.
static MIRType
GuessPhiType(MPhi *phi)
{
    MIRType type = MIRType_None;
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->isPhi()) {
            if (!in->toPhi()->triedToSpecialize())
                continue;
            if (in->type() == MIRType_None) {
                // The operand is a phi we tried to specialize, but we were
                // unable to guess its type. propagateSpecialization will
                // propagate the type to this phi when it becomes known.
                continue;
            }
        }
        if (type == MIRType_None) {
            type = in->type();
            continue;
        }
        if (type != in->type()) {
            // Specialize phis with int32 and double operands as double.
            if (IsNumberType(type) && IsNumberType(in->type()))
                type = MIRType_Double;
            else
                return MIRType_Value;
        }
    }
    return type;
}

bool
TypeAnalyzer::respecialize(MPhi *phi, MIRType type)
{
    if (phi->type() == type)
        return true;
    phi->specialize(type);
    return addPhiToWorklist(phi);
}

bool
TypeAnalyzer::propagateSpecialization(MPhi *phi)
{
    JS_ASSERT(phi->type() != MIRType_None);

    // Verify that this specialization matches any phis depending on it.
    for (MUseDefIterator iter(phi); iter; iter++) {
        if (!iter.def()->isPhi())
            continue;
        MPhi *use = iter.def()->toPhi();
        if (!use->triedToSpecialize())
            continue;
        if (use->type() == MIRType_None) {
            // We tried to specialize this phi, but were unable to guess its
            // type. Now that we know the type of one of its operands, we can
            // specialize it.
            if (!respecialize(use, phi->type()))
                return false;
            continue;
        }
        if (use->type() != phi->type()) {
            // Specialize phis with int32 and double operands as double.
            if (IsNumberType(use->type()) && IsNumberType(phi->type())) {
                if (!respecialize(use, MIRType_Double))
                    return false;
                continue;
            }

            // This phi in our use chain can now no longer be specialized.
            if (!respecialize(use, MIRType_Value))
                return false;
        }
    }

    return true;
}

bool
TypeAnalyzer::specializePhis()
{
    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); block++) {
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            MIRType type = GuessPhiType(*phi);
            phi->specialize(type);
            if (type == MIRType_None) {
                // We tried to guess the type but failed because all operands are
                // phis we still have to visit. Set the triedToSpecialize flag but
                // don't propagate the type to other phis, propagateSpecialization
                // will do that once we know the type of one of the operands.
                continue;
            }
            if (!propagateSpecialization(*phi))
                return false;
        }
    }

    while (!phiWorklist_.empty()) {
        MPhi *phi = popPhi();
        if (!propagateSpecialization(phi))
            return false;
    }

    return true;
}

void
TypeAnalyzer::adjustPhiInputs(MPhi *phi)
{
    MIRType phiType = phi->type();

    if (phiType == MIRType_Double) {
        // Convert int32 operands to double.
        for (size_t i = 0; i < phi->numOperands(); i++) {
            MDefinition *in = phi->getOperand(i);

            if (in->type() == MIRType_Int32) {
                MToDouble *toDouble = MToDouble::New(in);
                in->block()->insertBefore(in->block()->lastIns(), toDouble);
                phi->replaceOperand(i, toDouble);
            } else {
                JS_ASSERT(in->type() == MIRType_Double);
            }
        }
        return;
    }

    if (phiType != MIRType_Value)
        return;

    // Box every typed input.
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;

        if (in->isUnbox()) {
            // The input is being explicitly unboxed, so sneak past and grab
            // the original box.
            phi->replaceOperand(i, in->toUnbox()->input());
        } else {
            MBox *box = MBox::New(in);
            in->block()->insertBefore(in->block()->lastIns(), box);
            phi->replaceOperand(i, box);
        }
    }
}

bool
TypeAnalyzer::adjustInputs(MDefinition *def)
{
    TypePolicy *policy = def->typePolicy();
    if (policy && !policy->adjustInputs(def->toInstruction()))
        return false;
    return true;
}

void
TypeAnalyzer::replaceRedundantPhi(MPhi *phi)
{
    MBasicBlock *block = phi->block();
    js::Value v = (phi->type() == MIRType_Undefined) ? UndefinedValue() : NullValue();
    MConstant *c = MConstant::New(v);
    // The instruction pass will insert the box
    block->insertBefore(*(block->begin()), c);
    phi->replaceAllUsesWith(c);
}

bool
TypeAnalyzer::insertConversions()
{
    // Instructions are processed in reverse postorder: all uses are defs are
    // seen before uses. This ensures that output adjustment (which may rewrite
    // inputs of uses) does not conflict with input adjustment.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd();) {
            if (phi->type() <= MIRType_Null) {
                replaceRedundantPhi(*phi);
                phi = block->discardPhiAt(phi);
            } else {
                adjustPhiInputs(*phi);
                phi++;
            }
        }
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            if (!adjustInputs(*iter))
                return false;
        }
    }
    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!specializePhis())
        return false;
    if (!insertConversions())
        return false;
    return true;
}

bool
ion::ApplyTypeInformation(MIRGraph &graph)
{
    TypeAnalyzer analyzer(graph);

    if (!analyzer.analyze())
        return false;

    return true;
}

bool
ion::RenumberBlocks(MIRGraph &graph)
{
    size_t id = 0;
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++)
        block->setId(id++);

    return true;
}

// A Simple, Fast Dominance Algorithm by Cooper et al.
// Modified to support empty intersections for OSR, and in RPO.
static MBasicBlock *
IntersectDominators(MBasicBlock *block1, MBasicBlock *block2)
{
    MBasicBlock *finger1 = block1;
    MBasicBlock *finger2 = block2;

    JS_ASSERT(finger1);
    JS_ASSERT(finger2);

    // In the original paper, the block ID comparisons are on the postorder index.
    // This implementation iterates in RPO, so the comparisons are reversed.

    // For this function to be called, the block must have multiple predecessors.
    // If a finger is then found to be self-dominating, it must therefore be
    // reachable from multiple roots through non-intersecting control flow.
    // NULL is returned in this case, to denote an empty intersection.

    while (finger1->id() != finger2->id()) {
        while (finger1->id() > finger2->id()) {
            MBasicBlock *idom = finger1->immediateDominator();
            if (idom == finger1)
                return NULL; // Empty intersection.
            finger1 = idom;
        }

        while (finger2->id() > finger1->id()) {
            MBasicBlock *idom = finger2->immediateDominator();
            if (idom == finger2)
                return NULL; // Empty intersection.
            finger2 = finger2->immediateDominator();
        }
    }
    return finger1;
}

static void
ComputeImmediateDominators(MIRGraph &graph)
{
    // The default start block is a root and therefore only self-dominates.
    MBasicBlock *startBlock = *graph.begin();
    startBlock->setImmediateDominator(startBlock);

    // Any OSR block is a root and therefore only self-dominates.
    MBasicBlock *osrBlock = graph.osrBlock();
    if (osrBlock)
        osrBlock->setImmediateDominator(osrBlock);

    bool changed = true;

    while (changed) {
        changed = false;

        ReversePostorderIterator block = graph.rpoBegin();

        // For each block in RPO, intersect all dominators.
        for (; block != graph.rpoEnd(); block++) {
            // If a node has once been found to have no exclusive dominator,
            // it will never have an exclusive dominator, so it may be skipped.
            if (block->immediateDominator() == *block)
                continue;

            MBasicBlock *newIdom = block->getPredecessor(0);

            // Find the first common dominator.
            for (size_t i = 1; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                if (pred->immediateDominator() != NULL)
                    newIdom = IntersectDominators(pred, newIdom);

                // If there is no common dominator, the block self-dominates.
                if (newIdom == NULL) {
                    block->setImmediateDominator(*block);
                    changed = true;
                    break;
                }
            }

            if (newIdom && block->immediateDominator() != newIdom) {
                block->setImmediateDominator(newIdom);
                changed = true;
            }
        }
    }

#ifdef DEBUG
    // Assert that all blocks have dominator information.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        JS_ASSERT(block->immediateDominator() != NULL);
    }
#endif
}

bool
ion::BuildDominatorTree(MIRGraph &graph)
{
    ComputeImmediateDominators(graph);

    // Traversing through the graph in post-order means that every use
    // of a definition is visited before the def itself. Since a def
    // dominates its uses, by the time we reach a particular
    // block, we have processed all of its dominated children, so
    // block->numDominated() is accurate.
    for (PostorderIterator i(graph.poBegin()); i != graph.poEnd(); i++) {
        MBasicBlock *child = *i;
        MBasicBlock *parent = child->immediateDominator();

        // If the block only self-dominates, it has no definite parent.
        if (child == parent)
            continue;

        if (!parent->addImmediatelyDominatedBlock(child))
            return false;

        // An additional +1 for the child block.
        parent->addNumDominated(child->numDominated() + 1);
    }

#ifdef DEBUG
    // If compiling with OSR, many blocks will self-dominate.
    // Without OSR, there is only one root block which dominates all.
    if (!graph.osrBlock())
        JS_ASSERT(graph.begin()->numDominated() == graph.numBlocks() - 1);
#endif
    // Now, iterate through the dominator tree and annotate every
    // block with its index in the pre-order traversal of the
    // dominator tree.
    Vector<MBasicBlock *, 1, IonAllocPolicy> worklist;

    // The index of the current block in the CFG traversal.
    size_t index = 0;

    // Add all self-dominating blocks to the worklist.
    // This includes all roots. Order does not matter.
    for (MBasicBlockIterator i(graph.begin()); i != graph.end(); i++) {
        MBasicBlock *block = *i;
        if (block->immediateDominator() == block) {
            if (!worklist.append(block))
                return false;
        }
    }
    // Starting from each self-dominating block, traverse the CFG in pre-order.
    while (!worklist.empty()) {
        MBasicBlock *block = worklist.popCopy();
        block->setDomIndex(index);

        for (size_t i = 0; i < block->numImmediatelyDominatedBlocks(); i++) {
            if (!worklist.append(block->getImmediatelyDominatedBlock(i)))
                return false;
        }
        index++;
    }

    return true;
}

bool
ion::BuildPhiReverseMapping(MIRGraph &graph)
{
    // Build a mapping such that given a basic block, whose successor has one or
    // more phis, we can find our specific input to that phi. To make this fast
    // mapping work we rely on a specific property of our structured control
    // flow graph: For a block with phis, its predecessors each have only one
    // successor with phis. Consider each case:
    //   * Blocks with less than two predecessors cannot have phis.
    //   * Breaks. A break always has exactly one successor, and the break
    //             catch block has exactly one predecessor for each break, as
    //             well as a final predecessor for the actual loop exit.
    //   * Continues. A continue always has exactly one successor, and the
    //             continue catch block has exactly one predecessor for each
    //             continue, as well as a final predecessor for the actual
    //             loop continuation. The continue itself has exactly one
    //             successor.
    //   * An if. Each branch as exactly one predecessor.
    //   * A switch. Each branch has exactly one predecessor.
    //   * Loop tail. A new block is always created for the exit, and if a
    //             break statement is present, the exit block will forward
    //             directly to the break block.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->numPredecessors() < 2) {
            JS_ASSERT(block->phisEmpty());
            continue;
        }

        // Assert on the above.
        for (size_t j = 0; j < block->numPredecessors(); j++) {
            MBasicBlock *pred = block->getPredecessor(j);

#ifdef DEBUG
            size_t numSuccessorsWithPhis = 0;
            for (size_t k = 0; k < pred->numSuccessors(); k++) {
                MBasicBlock *successor = pred->getSuccessor(k);
                if (!successor->phisEmpty())
                    numSuccessorsWithPhis++;
            }
            JS_ASSERT(numSuccessorsWithPhis <= 1);
#endif

            pred->setSuccessorWithPhis(*block, j);
        }
    }

    return true;
}

static inline MBasicBlock *
SkipContainedLoop(MBasicBlock *block, MBasicBlock *header)
{
    while (block->loopHeader() || block->isLoopHeader()) {
        if (block->loopHeader())
            block = block->loopHeader();
        if (block == header)
            break;
        block = block->loopPredecessor();
    }
    return block;
}

#ifdef DEBUG
static bool
CheckSuccessorImpliesPredecessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = succ(A), verify A = pred(B).
    for (size_t i = 0; i < B->numPredecessors(); i++) {
        if (A == B->getPredecessor(i))
            return true;
    }
    return false;
}

static bool
CheckPredecessorImpliesSuccessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = pred(A), verify A = succ(B).
    for (size_t i = 0; i < B->numSuccessors(); i++) {
        if (A == B->getSuccessor(i))
            return true;
    }
    return false;
}

static bool
CheckMarkedAsUse(MInstruction *ins, MDefinition *operand)
{
    for (MUseIterator i = operand->usesBegin(); i != operand->usesEnd(); i++) {
        if (i->node()->isDefinition()) {
            if (ins == i->node()->toDefinition())
                return true;
        }
    }
    return false;
}
#endif // DEBUG

#ifdef DEBUG
static void
AssertReversePostOrder(MIRGraph &graph)
{
    // Check that every block is visited after all its predecessors (except backedges).
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        JS_ASSERT(!block->isMarked());

        for (size_t i = 0; i < block->numPredecessors(); i++) {
            MBasicBlock *pred = block->getPredecessor(i);
            JS_ASSERT_IF(!pred->isLoopBackedge(), pred->isMarked());
        }

        block->mark();
    }

    graph.unmarkBlocks();
}
#endif

void
ion::AssertGraphCoherency(MIRGraph &graph)
{
#ifdef DEBUG
    // Assert successor and predecessor list coherency.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        for (size_t i = 0; i < block->numSuccessors(); i++)
            JS_ASSERT(CheckSuccessorImpliesPredecessor(*block, block->getSuccessor(i)));

        for (size_t i = 0; i < block->numPredecessors(); i++)
            JS_ASSERT(CheckPredecessorImpliesSuccessor(*block, block->getPredecessor(i)));

        for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            for (uint32 i = 0; i < ins->numOperands(); i++)
                JS_ASSERT(CheckMarkedAsUse(*ins, ins->getOperand(i)));
        }
    }

    AssertReversePostOrder(graph);
#endif
}


struct BoundsCheckInfo
{
    MBoundsCheck *check;
    uint32 validUntil;
};

typedef HashMap<uint32,
                BoundsCheckInfo,
                DefaultHasher<uint32>,
                IonAllocPolicy> BoundsCheckMap;

// Compute a hash for bounds checks which ignores constant offsets in the index.
static HashNumber
BoundsCheckHashIgnoreOffset(MBoundsCheck *check)
{
    LinearSum indexSum = ExtractLinearSum(check->index());
    uintptr_t index = indexSum.term ? uintptr_t(indexSum.term) : 0;
    uintptr_t length = uintptr_t(check->length());
    return index ^ length;
}

static MBoundsCheck *
FindDominatingBoundsCheck(BoundsCheckMap &checks, MBoundsCheck *check, size_t index)
{
    // See the comment in ValueNumberer::findDominatingDef.
    HashNumber hash = BoundsCheckHashIgnoreOffset(check);
    BoundsCheckMap::Ptr p = checks.lookup(hash);
    if (!p || index > p->value.validUntil) {
        // We didn't find a dominating bounds check.
        BoundsCheckInfo info;
        info.check = check;
        info.validUntil = index + check->block()->numDominated();

        if(!checks.put(hash, info))
            return NULL;

        return check;
    }

    return p->value.check;
}

// Extract a linear sum from ins, if possible (otherwise giving the sum 'ins + 0').
LinearSum
ion::ExtractLinearSum(MDefinition *ins)
{
    if (ins->type() != MIRType_Int32)
        return LinearSum(ins, 0);

    if (ins->isConstant()) {
        const Value &v = ins->toConstant()->value();
        JS_ASSERT(v.isInt32());
        return LinearSum(NULL, v.toInt32());
    } else if (ins->isAdd() || ins->isSub()) {
        MDefinition *lhs = ins->getOperand(0);
        MDefinition *rhs = ins->getOperand(1);
        if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
            LinearSum lsum = ExtractLinearSum(lhs);
            LinearSum rsum = ExtractLinearSum(rhs);

            JS_ASSERT(lsum.term || rsum.term);
            if (lsum.term && rsum.term)
                return LinearSum(ins, 0);

            // Check if this is of the form <SUM> + n, n + <SUM> or <SUM> - n.
            if (ins->isAdd()) {
                int32 constant;
                if (!SafeAdd(lsum.constant, rsum.constant, &constant))
                    return LinearSum(ins, 0);
                return LinearSum(lsum.term ? lsum.term : rsum.term, constant);
            } else if (lsum.term) {
                int32 constant;
                if (!SafeSub(lsum.constant, rsum.constant, &constant))
                    return LinearSum(ins, 0);
                return LinearSum(lsum.term, constant);
            }
        }
    }

    return LinearSum(ins, 0);
}

static bool
TryEliminateBoundsCheck(MBoundsCheck *dominating, MBoundsCheck *dominated, bool *eliminated)
{
    JS_ASSERT(!*eliminated);

    // We found two bounds checks with the same hash number, but we still have
    // to make sure the lengths and index terms are equal.
    if (dominating->length() != dominated->length())
        return true;

    LinearSum sumA = ExtractLinearSum(dominating->index());
    LinearSum sumB = ExtractLinearSum(dominated->index());

    // Both terms should be NULL or the same definition.
    if (sumA.term != sumB.term)
        return true;

    // This bounds check is redundant.
    *eliminated = true;

    // Normalize the ranges according to the constant offsets in the two indexes.
    int32 minimumA, maximumA, minimumB, maximumB;
    if (!SafeAdd(sumA.constant, dominating->minimum(), &minimumA) ||
        !SafeAdd(sumA.constant, dominating->maximum(), &maximumA) ||
        !SafeAdd(sumB.constant, dominated->minimum(), &minimumB) ||
        !SafeAdd(sumB.constant, dominated->maximum(), &maximumB))
    {
        return false;
    }

    // Update the dominating check to cover both ranges, denormalizing the
    // result per the constant offset in the index.
    int32 newMinimum, newMaximum;
    if (!SafeSub(Min(minimumA, minimumB), sumA.constant, &newMinimum) ||
        !SafeSub(Max(maximumA, maximumB), sumA.constant, &newMaximum))
    {
        return false;
    }

    dominating->setMinimum(newMinimum);
    dominating->setMaximum(newMaximum);
    return true;
}

// A bounds check is considered redundant if it's dominated by another bounds
// check with the same length and the indexes differ by only a constant amount.
// In this case we eliminate the redundant bounds check and update the other one
// to cover the ranges of both checks.
//
// Bounds checks are added to a hash map and since the hash function ignores
// differences in constant offset, this offers a fast way to find redundant
// checks.
bool
ion::EliminateRedundantBoundsChecks(MIRGraph &graph)
{
    BoundsCheckMap checks;

    if (!checks.init())
        return false;

    // Stack for pre-order CFG traversal.
    Vector<MBasicBlock *, 1, IonAllocPolicy> worklist;

    // The index of the current block in the CFG traversal.
    size_t index = 0;

    // Add all self-dominating blocks to the worklist.
    // This includes all roots. Order does not matter.
    for (MBasicBlockIterator i(graph.begin()); i != graph.end(); i++) {
        MBasicBlock *block = *i;
        if (block->immediateDominator() == block) {
            if (!worklist.append(block))
                return false;
        }
    }

    // Starting from each self-dominating block, traverse the CFG in pre-order.
    while (!worklist.empty()) {
        MBasicBlock *block = worklist.popCopy();

        // Add all immediate dominators to the front of the worklist.
        for (size_t i = 0; i < block->numImmediatelyDominatedBlocks(); i++) {
            if (!worklist.append(block->getImmediatelyDominatedBlock(i)))
                return false;
        }

        for (MDefinitionIterator iter(block); iter; ) {
            if (!iter->isBoundsCheck()) {
                iter++;
                continue;
            }

            MBoundsCheck *check = iter->toBoundsCheck();

            // Replace all uses of the bounds check with the actual index.
            // This is (a) necessary, because we can coalesce two different
            // bounds checks and would otherwise use the wrong index and
            // (b) helps register allocation. Note that this is safe since
            // no other pass after bounds check elimination moves instructions.
            check->replaceAllUsesWith(check->index());

            if (!check->isMovable()) {
                iter++;
                continue;
            }

            MBoundsCheck *dominating = FindDominatingBoundsCheck(checks, check, index);
            if (!dominating)
                return false;

            if (dominating == check) {
                // We didn't find a dominating bounds check.
                iter++;
                continue;
            }

            bool eliminated = false;
            if (!TryEliminateBoundsCheck(dominating, check, &eliminated))
                return false;

            if (eliminated)
                iter = check->block()->discardDefAt(iter);
            else
                iter++;
        }
        index++;
    }

    JS_ASSERT(index == graph.numBlocks());
    return true;
}
