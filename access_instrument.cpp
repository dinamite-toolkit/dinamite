#include "Util.hpp"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Dwarf.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/ADT/SmallVector.h>
#include "json11.hpp"
#include "IdMap.hpp"
#include "AllocDefs.hpp"
#include "MetadataCrawler.hpp"
#include "LogFunctionManager.hpp"
#include "InstrumentationFilter.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <vector>

#define TRACELINE() cerr << __LINE__ << endl;
#define CHECK_TAG(x, tag) (((x) & (tag)) == (tag))
//#define INST_ALLOC_ONLY 1

#define DEBUG_PRINT 1

using namespace llvm;
using namespace std;

enum fn_events {
    FN_BEGIN, FN_END
};

namespace {

    struct AccessInstrumentationPass : public ModulePass {
        static char ID;
        IdMap srcmap;
        IdMap typemap;
        IdMap varmap;
        IdMap fnmap;


        AllocDefManager adm;
        MetadataCrawler mdc;
        LogFunctionManager lfm;
        InstrumentationFilter insfilt;

        set<Value *> argLogSet;

        Function *currentFunction;

        typedef struct _SourceLoc {
            int fileId;
            int line;
            int col;
        } SourceLoc;

        SourceLoc getSourceLoc(Instruction *i) {
            SourceLoc retval;
            int line = -1;
            int col = -1;
            int fileid = -1;
            StringRef file("");
            StringRef dir("");
            if (MDNode *N = i->getMetadata("dbg")) {  // Here I is an LLVM instruction 
                DILocation loc(N);                      // DILocation is in DebugInfo.h 
                line = loc.getLineNumber();
                col = loc.getColumnNumber();
                file = loc.getFilename();
                dir = loc.getDirectory();
                fileid = srcmap.getId(dir.str() + "/" + file.str());
#ifdef DEBUG_PRINT
                cerr << dir.str() << " " << file.str() << ":" << line << ":" << col << endl;
#endif
            }

            retval.fileId = fileid;
            retval.line = line;
            retval.col = col;

            return retval;
        }


        Function * loadExternalFunction(Module *m, Module *extM, const char *name) {
            Function *fn = extM->getFunction(name);
            FunctionType *ft = fn->getFunctionType();
            Function *newFn = Function::Create(ft, Function::ExternalWeakLinkage, name, m);
            return newFn;
        }

        AccessInstrumentationPass() : ModulePass(ID), srcmap("map_sources.json"), typemap("map_types.json"), varmap("map_variables.json"), fnmap("map_functions.json") {}

        Constant *getConstantFromInt(int val, Type *t) {
            APInt ai(32, val, true);
            return Constant::getIntegerValue(t, ai);
        }

        string getGEPType(GetElementPtrInst *gep) {
            Value *v_src = gep->getOperand(0);
            Type *t_src = v_src->getType();

            string prefix;
            string end;
            if (GetElementPtrInst *gepv = dyn_cast<GetElementPtrInst>(v_src)) {
                prefix.assign(getGEPType(gepv));
            }
            if (t_src->isPointerTy()) {
                t_src = t_src->getPointerElementType();
                if (t_src->isStructTy()) {
                    if (StructType *st = (StructType *)(t_src)) {
                        if (st->isLiteral()) {
                            // This seems to be a bug in llvm. hasName() will return false, but sometimes it's possible to get the name, and sometimes it breaks :(
                            //end.assign("");
                            end.assign("literal");
                        } else {
                            end.assign(t_src->getStructName().str());
                        }
                    }
                }
            }

            if (end.find("class.") == 0) {
                end.erase(0, 6);
            }
            if (end.find("struct.") == 0) {
                end.erase(0, 7);
            }


            if (!prefix.empty()) {
                return prefix + std::string(".") + end;
            } else {
                return end;
            }
        }

        string getValueType(Value *v) {
            std::string t_string;
            llvm::raw_string_ostream rso(t_string);
            v->getType()->print(rso);
            std::string result(rso.str());

            if (result.substr(0,7).compare("%struct") == 0) {
                result = result.substr(8);
            } 

            return result;
        }

        StringRef getGEPVarName(GetElementPtrInst *gep) {
            unsigned long fieldIndex;
            if (gep->getNumOperands() < 3) {
                fieldIndex = -1;
            } else {
                Value *fiV = gep->getOperand(2);
                if (ConstantInt *ciV = dyn_cast<ConstantInt> (fiV)) {
                    fieldIndex = ciV->getSExtValue();
                }
            }


            Value *v_src = gep->getOperand(0);
            Type *t_src = v_src->getType();

            string parent;

            if (t_src->isPointerTy()) {
                t_src = t_src->getPointerElementType();
                if (t_src->isStructTy()) {
                    if (StructType *st = (StructType*)(t_src)) {
                        if (st->isLiteral()) {
                            parent.assign("literal");
                        } else {
                            parent.assign(t_src->getStructName().str());
                        }
                    }
                }
            }

            if (parent.find("class.") == 0) {
                parent.erase(0, 6);

            }
            if (parent.find("struct.") == 0) {
                parent.erase(0, 7);
            }

            if (parent.find("::") != string::npos) {
                int dblcolon = parent.find("::");
                parent.erase(0, dblcolon+2);
            }

            string fieldName = mdc.getFieldName(parent, fieldIndex);
            parent.append(".");
            parent.append(fieldName);

            if ((parent.length() > 5) && (parent.substr(parent.length()-5,
							parent.length()).compare(".addr") == 0)) {
                parent.erase(parent.length()-5, parent.length());
            }

            return parent;
        }

        string getVarName(Value *v) {
            if (v->getName().str().compare("this.addr") == 0) {
                string thisclass(getValueType(v));
                if (thisclass.find("%class.") == 0) {
                    thisclass.erase(0, 7);
                }
                if (thisclass.find("%struct.") == 0) {
                    thisclass.erase(0, 8);
                }

                while (thisclass.find("*") != string::npos) {
                    thisclass.erase(thisclass.find("*") ,1);
                }
                thisclass.append(".this");
                return thisclass;

            }
            if (GetElementPtrInst *gepv = dyn_cast<GetElementPtrInst>(v)) {
                return getGEPVarName(gepv);
            }
            string base("");
            if (dyn_cast<GlobalVariable>(v)) {
                base.assign("<global>.");
            } else {
                //return "<function_local>";
                base = demangle(currentFunction->getName().str().c_str());
                if (base.find("(") == string::npos) {
                    base.append("()");
                } else {
                    base.erase(base.find("("), base.size()-1);
                    base.append("()");

                }
                base.append(".");
            }

            if (v->getName().str().size() == 0) {
                base.append("<NA>");

            }
            base.append(v->getName().str());
            if ((base.length() > 5) && (base.substr(base.length()-5, base.length()).compare(".addr") == 0)) {
                base.erase(base.length()-5, base.length());
            }

            return base;
        }

        void instrumentAccess(Instruction *si, char accessType) {
            int line = -1;
            int col = -1;
            int fileid = -1;
            int tid = -1;
            int varid = -1;
            StringRef file("");
            StringRef dir("");
            Function *afunc;
            if (MDNode *N = si->getMetadata("dbg")) {
                DILocation loc(N);
                line = loc.getLineNumber();
                col = loc.getColumnNumber();
                file = loc.getFilename();
                dir = loc.getDirectory();
                fileid = srcmap.getId(dir.str() + "/" + file.str());
#ifdef DEBUG_PRINT
                cerr << dir.str() << " " << file.str() << ":" << line << ":" << col << endl;
#endif
            }
            IRBuilder<> Builder(si);
            std::vector<Value *> args;
            //		for (auto op = si->op_begin(); op != si->op_end(); op++) {
            {
                Value *accessedValue;
                if ((accessType == 'w') || (accessType == 'a')) {
                    accessedValue = si->op_begin()->get();
                } else {
                    accessedValue = Builder.CreateLoad(si->op_begin()->get());
                }

#ifdef DEBUG_PRINT
                {
                    std::string s;
                    llvm::raw_string_ostream rso(s);
                    si->print(rso);
                    cerr << "Instruction: " << s << endl;
                }
#endif

                afunc = lfm.getLogFunction(accessedValue, si->getParent()->getParent());

                if (afunc != NULL) {
#ifdef DEBUG_PRINT
                    cerr << afunc->getName().str() << endl;
#endif

                    auto op = si->op_end() - 1; // destination
                    Value *v = op->get();

                    string fullType(getValueType(v));
                    string varName(getVarName(v));

#ifdef DEBUG_PRINT
                    cerr << "DEBUG Var name: " << varName << " Insn: ";
                    si->dump();
                    cerr << endl;
#endif

                    tid = typemap.getId(fullType);
                    varid = varmap.getId(varName);

                    Value *destPtrVal = op->get();
                    Type *destType = afunc->getFunctionType()->getParamType(0);

#ifdef DEBUG_PRINT
                    cerr << "\t" << op->get()->getName().data() << endl;
#endif
                    Value *address = Builder.CreateCast(Instruction::BitCast, destPtrVal, destType);
                    args.push_back(address);

                    Value *castValue;
                    Type *valueType = afunc->getFunctionType()->getParamType(1);
                    if (accessedValue->getType()->isFloatingPointTy()) {
                        castValue = Builder.CreateFPCast(accessedValue, valueType);
                    } else if (accessedValue->getType()->getScalarSizeInBits() == 1) {
                        castValue = Builder.CreateZExtOrBitCast(accessedValue, valueType);
                    } else {
                        if (Constant *c = dyn_cast<Constant>(accessedValue)) {
                            castValue = ConstantExpr::getBitCast(c, valueType);
                        } else {
                            castValue = Builder.CreateBitCast(accessedValue, valueType);
                        }
                    }

                    if (castValue->getType() != valueType) {
                        cerr << "BAD CAST" << endl;
                    }

                    args.push_back(castValue);

                    args.push_back(getConstantFromInt(accessType, afunc->getFunctionType()->getParamType(2)));
                    args.push_back(getConstantFromInt(fileid, afunc->getFunctionType()->getParamType(3)));
                    args.push_back(getConstantFromInt(line, afunc->getFunctionType()->getParamType(4)));
                    args.push_back(getConstantFromInt(col, afunc->getFunctionType()->getParamType(5)));
                    args.push_back(getConstantFromInt(tid, afunc->getFunctionType()->getParamType(6)));
                    args.push_back(getConstantFromInt(varid, afunc->getFunctionType()->getParamType(7)));

                    Builder.CreateCall(afunc, args);
                }
            }
        }

        void instrumentAlloc(CallInst *ci) {
            Function *fn = ci->getCalledFunction();
            if (fn == NULL) {
                return;
            }
#ifdef DEBUG_PRINT
            {
                std::string s;
                llvm::raw_string_ostream rso(s);
                ci->print(rso);
                cerr << "Instruction: " << s << endl;
            }
#endif
            if (AllocDefinition *adef = adm.getAllocDef(fn)) {
#ifdef DEBUG_PRINT
                cerr << "=========================================" << endl;
                cerr << "alloc call: " << adef->name << " " << adef->sizeIdx << " " << adef->numIdx << " " << adef->addrIdx << endl;
                cerr << "=========================================" << endl;
#endif

                BasicBlock::iterator insertionPoint = ci;
                insertionPoint++;
                Instruction *nextInst = insertionPoint;
                string allocType;

                if (BitCastInst *bci = dyn_cast<BitCastInst>(nextInst)) {
                    allocType = getValueType(bci);
                } else {
                    allocType = "void";
                }
                int typeId = typemap.getId(allocType);

                SourceLoc srcLoc = getSourceLoc(ci);

                IRBuilder<> Builder(insertionPoint);
                std::vector<Value *> args;

                Type *addrType = lfm.allocLogFunc->getFunctionType()->getParamType(0);
                Value *addrVal = ci; // get actual value

                Value *castAddr = Builder.CreateBitCast(addrVal, addrType);

                Type *sizeType = lfm.allocLogFunc->getFunctionType()->getParamType(1);
                Value *size = ci->getArgOperand(adef->sizeIdx);

                Type *numType = lfm.allocLogFunc->getFunctionType()->getParamType(2);
                Value *num;
                if (adef->numIdx != -1) {
                    num = ci->getArgOperand(adef->numIdx);
                } else {
                    num = getConstantFromInt(1, numType);
                }

#ifdef DEBUG_PRINT
                printType(sizeType);
                printType(size->getType());
#endif

                Value *castSize = Builder.CreateZExtOrBitCast(size, sizeType);
                Value *castNum = Builder.CreateZExtOrBitCast(num, numType);

                args.push_back(castAddr);
                args.push_back(castSize);
                args.push_back(castNum);
                args.push_back(getConstantFromInt(typeId, lfm.allocLogFunc->getFunctionType()->getParamType(2)));
                args.push_back(getConstantFromInt(srcLoc.fileId, lfm.allocLogFunc->getFunctionType()->getParamType(3)));
                args.push_back(getConstantFromInt(srcLoc.line, lfm.allocLogFunc->getFunctionType()->getParamType(4)));
                args.push_back(getConstantFromInt(srcLoc.col, lfm.allocLogFunc->getFunctionType()->getParamType(5)));
                Builder.CreateCall(lfm.allocLogFunc, args);

            }

        }


        void instrumentFnEvent(Instruction *i, int event) {
            IRBuilder<> Builder(i);
            std::vector<Value *> args;
            Function *f = i->getParent()->getParent();
            Function *lfunc;

            if (f->getName().str().substr(0, 8).compare("_GLOBAL_") == 0) {
                if (event == FN_BEGIN) {
                    lfunc = lfm.initLogFunc;
                } else {
                    lfunc = lfm.exitLogFunc;
                }
            } else 
                if (f->getName().str().compare("main") == 0) {
                    if (event == FN_BEGIN) {
                        lfunc = lfm.initLogFunc;
                    } else {
                        lfunc = lfm.exitLogFunc;
                    }
                } else {
#ifdef INST_ALLOC_ONLY
                    return;
#endif
                    if (event == FN_BEGIN) {
                        lfunc = lfm.fnBeginLogFunc;
                    } else {
                        lfunc = lfm.fnEndLogFunc;
                    }
                }
            int fnid = fnmap.getId(demangle(f->getName().str().c_str()));
            args.push_back(getConstantFromInt(fnid, lfunc->getFunctionType()->getParamType(0)));
            Builder.CreateCall(lfunc, args);
        }


        void instrumentExit(CallInst *ci) {
            IRBuilder<> Builder(ci);
            std::vector<Value *> args;
            Function *f = ci->getCalledFunction();
            if (f == NULL) {
                return;
            }

            if (f->getName().str().compare("exit") == 0) {
                args.push_back(getConstantFromInt(-1, lfm.exitLogFunc->getFunctionType()->getParamType(0)));
                Builder.CreateCall(lfm.exitLogFunc, args);
            }
        }

        bool checkAndConsumeArg(StoreInst *si) {
            Value *argCandidate = si->op_begin()->get();

#ifdef DEBUG_PRINT
            cerr << "========================================" << endl;
            cerr << "Check and consume" << endl;
            cerr << "========================================" << endl;
            si->dump(); cerr << endl;
            cerr << " Candidate " << hex << argCandidate << endl;
            for (auto it = argLogSet.begin(); it != argLogSet.end(); it++) {
                cerr << hex << *it << endl;
            }
            cerr << dec;
            cerr << "========================================" << endl;
#endif


            if (argLogSet.count(argCandidate) != 0) {
                argLogSet.erase(argCandidate);
#ifdef DEBUG_PRINT
                cerr << "ARGLOG HIT" << endl;
#endif
                return true;
            }
            return false;
        }

        void queueAndInjectArgsToLog(Function *f) {
            if (f->empty()) { return; }

            string fname = f->getName().str();

            Instruction *first = f->getEntryBlock().getFirstInsertionPt();
            IRBuilder<> ArgLoadBuilder(first);

            int idx = 0;
            for (auto it = f->arg_begin(); it != f->arg_end(); it++) {
                if (insfilt.checkFunctionArgFilter(fname, idx)) {
                    argLogSet.insert(it);
                    Value *destPtr = ArgLoadBuilder.CreateAlloca(it->getType());
                    Value *storeInst = ArgLoadBuilder.CreateStore(it, destPtr);
		    (void)storeInst;
                }
                idx++;
            }
        }

        virtual bool runOnModule(Module &m) {

            insfilt.loadFilterDataEnv();

            if (!insfilt.checkFileFilter(m.getModuleIdentifier())) {
                return false;
            }

            adm.loadAllocDefs();
            lfm.loadFunctions(&m);

            cerr << "Generating class field maps... ";
            mdc.crawlModule(m);

            cerr << "done!" << endl;

            for (Function &f : m) {
                if (!lfm.isLogFunction(&f)) { // TODO: remove and test, should work
                    //string fname = demangle(f.getName().str().c_str());

                    size_t fnSize;

                    fn_size_metrics metric = insfilt.getFunctionSizeMetric();

                    if (metric == FN_SIZE_IR) {
                        fnSize = f.size();
                    } else if (metric == FN_SIZE_LOC) {
                        fnSize = getFunctionLOCSize(f);
                    } else { /* FN_SIZE_PATH */
			    fnSize = longestPathSize(f);

			    /* If the function has loops and the loop
			     * check is on, we want to keep functions
			     * with loops regardless of their size.
			     * Adjust the size of the function to
			     * a very large number to reflect that
			     * setting.
			     */
			    if (checkFunctionLoops(f) &&
				insfilt.loopCheckEnabled()) {
				    fnSize = (size_t) ((uint32_t)~0);
#ifdef DEBUG_PRINT
				    cerr << "Loop check enabled: " <<
					    "size set to " << fnSize
					 << endl;
#endif
			    }
                    }

#ifdef DEBUG_PRINT
		    cerr << f.getName().str() << ": " << fnSize
			 << ", metric is ";

		    if (metric == FN_SIZE_LOC)
			    cerr << "LOC";
		    else if (metric == FN_SIZE_IR)
			    cerr << "IR";
		    else
			    cerr << "LOC_PATH";
		    cerr << endl;
#endif

                    if (!insfilt.checkFunctionSize(f.getName().str(), fnSize))
			    continue;

                    bool accessFilter = insfilt.checkFunctionFilter(
			    f.getName().str(), "access");
                    bool functionFilter = insfilt.checkFunctionFilter(
			    f.getName().str(), "function");
                    bool allocFilter = insfilt.checkFunctionFilter(
			    f.getName().str(), "alloc");

#ifdef DEBUG_PRINT
		    cerr << "functionFilter for " << f.getName().str() << " is "
			 << functionFilter << ".";
		    if (functionFilter)
			    cerr << "Instrumenting...";
		    else
			    cerr << "Not instrumenting...";
		    cerr << endl;
#endif
                    currentFunction = &f;

                    //TODO: fill this with functionality:
                    if (functionFilter) {
                        queueAndInjectArgsToLog(&f);
                    }

                    if (!f.empty()) {
                        BasicBlock &entryBlock = f.getEntryBlock();
                        Instruction *first = entryBlock.getFirstInsertionPt();
                        if ((functionFilter) || (f.getName().str().compare("main") == 0)) {
                            instrumentFnEvent(first, FN_BEGIN);
                        }

                    }
                    for (BasicBlock &b : f) {
                        {
                            TerminatorInst *ti = b.getTerminator();
                            if (ReturnInst *ri = dyn_cast<ReturnInst>(ti)) {
                            if ((functionFilter) || (f.getName().str().compare("main") == 0)) {
                                    instrumentFnEvent(ri, FN_END);
                                }
                            }
                        }
                        for (Instruction &i : b) {
#ifdef DEBUG_PRINT
                            cerr << "Ins: " << i.getOpcodeName() << endl;
#endif

#ifndef INST_ALLOC_ONLY
                            bool isArg;
                            if (StoreInst *si = dyn_cast<StoreInst>(&i)) {
				    /* First check for argument instrumentation,
				     * and remember the result.
				     */
				    if ( (functionFilter &&
					  (isArg = checkAndConsumeArg(si)))
					 || (accessFilter)) {
                                            /* These guys get read once at the
					     * start of the function.
					     */
					    if (isArg) {
						    instrumentAccess(si, 'a');
					    } else {
						    instrumentAccess(si, 'w');
					    }
				    }
                            }

                            if (LoadInst *li = dyn_cast<LoadInst>(&i)) {
                                if (accessFilter) {
                                    instrumentAccess(li, 'r');
                                }
                            }
#endif

                            if (CallInst *ci = dyn_cast<CallInst>(&i)) {
                                if (allocFilter) {
                                    instrumentAlloc(ci);
                                }

                                if ((functionFilter) || (f.getName().str().compare("main") == 0)) {
                                    instrumentExit(ci);
                                }
                            }
                        }

                    }

                }
            }
            srcmap.saveMap();
            typemap.saveMap();
            varmap.saveMap();
            fnmap.saveMap();

            return true;
        }


        };

        static void registerAccessInstrumentationPass(const llvm::PassManagerBuilder &,
                llvm::legacy::PassManagerBase &PM) {
            PM.add(new AccessInstrumentationPass());
        }
        static llvm::RegisterStandardPasses
            //RegisterMyPass(llvm::PassManagerBuilder::EP_EarlyAsPossible,
            RegisterMyPass(llvm::PassManagerBuilder::EP_EnabledOnOptLevel0,
                    registerAccessInstrumentationPass);
/*
        static llvm::RegisterStandardPasses
            RegisterMyPassOx(llvm::PassManagerBuilder::EP_OptimizerLast,
                    registerAccessInstrumentationPass);
*/
	static llvm::RegisterStandardPasses
	RegisterMyPassOx(llvm::PassManagerBuilder::EP_ModuleOptimizerEarly,
			 registerAccessInstrumentationPass);
    }

    char AccessInstrumentationPass::ID = 0;
    static RegisterPass<AccessInstrumentationPass> X("accessinst", "Memory access instrumentation", false, false);

