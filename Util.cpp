#include "Util.hpp"

using namespace llvm;
using namespace std;

#include <set>
#include <map>

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/ADT/SmallVector.h"

std::string demangle(const char* name) 
{
    int status = -1; 

    std::unique_ptr<char, void(*)(void*)> res { abi::__cxa_demangle(name, NULL, NULL, &status), std::free };
    return (status == 0) ? res.get() : std::string(name);
}

void printType(llvm::Type *t) {
    std::string type_str;
    llvm::raw_string_ostream rso(type_str);
    t->print(rso);
    std::cerr << "Type: " << rso.str() << std::endl;
}


// Each basic block has a terminator that can get a list of successors
// We can form a graph with this and find out if there are any loops

bool checkFunctionLoops(Function &f) {
    SmallVector< std::pair< const BasicBlock *, const BasicBlock * >, 10 > backedges;
    if (f.empty()) {
        return false;
    }
    FindFunctionBackedges(f, backedges);
    if (backedges.size() != 0) {
        return true;
    } else {
        return false;
    }
}



size_t getFunctionLOCSize(Function &f) {
    if (f.size() == 0) {
        return 0;
    }
    set<int> lineSet;
    for (BasicBlock &b : f) {
        for (Instruction &i : b) {
            int curline = -1;  
            if (MDNode *N = i.getMetadata("dbg")) {  
                DILocation loc(N);
                curline = loc.getLineNumber();
            } 
            if (curline != -1) {
                if (lineSet.count(curline) == 0) {
                    lineSet.insert(curline);
                }
            }
        }
    }
    return lineSet.size();
}

set<int> calculateBlock(BasicBlock *block) {
    set<int> lineSet;
    for (Instruction &i : *block) {
        int curline = -1;  
        if (MDNode *N = i.getMetadata("dbg")) {  
            DILocation loc(N);
            curline = loc.getLineNumber();
        } 
        if (curline != -1) {
            if (lineSet.count(curline) == 0) {
                lineSet.insert(curline);
            }
        }
    }
    return lineSet;
}

void explorePath(map<BasicBlock *, set<int> > &visits, BasicBlock &currentBlock) {

	/* Return if we already visited this block */
	if (visits.find(&currentBlock) != visits.end())
		return;

	set<int> currentBlockLineSet = calculateBlock(&currentBlock);

	/* Update the visits map to reflect the visit to the current block.
	 * If we are in a loop, we will visit this block again, so we need
	 * to remember that we already visited it to avoid going into an
	 * infinite loop.
	 */
	visits[&currentBlock] = currentBlockLineSet;

	TerminatorInst *ti = currentBlock.getTerminator();
	/* Check if this is the leaf node */
	if (ti == NULL || dyn_cast<ReturnInst>(ti))
		return;

	set<int> maxLineSet;
	for (unsigned i = 0; i < ti->getNumSuccessors(); i++) {
		explorePath(visits, *(ti->getSuccessor(i)));
		set<int> pLineSet = visits[ti->getSuccessor(i)];
		pLineSet.insert(currentBlockLineSet.begin(),
				currentBlockLineSet.end());
		if (maxLineSet.size() < pLineSet.size()) {
			maxLineSet = pLineSet;
		}
	}
	visits[&currentBlock] = maxLineSet;
	return;
}

// Cheap way to do this, traverse the basic block graph following terminators
size_t longestPathSize(Function &f) {
    if (f.empty()) {
        return 0;
    }
    map<BasicBlock *, set<int> > visits;
    BasicBlock &entrybb = f.getEntryBlock();
    explorePath(visits, entrybb);

    return visits[&entrybb].size();
}


