#include "LogFunctionManager.hpp"

#include "llvm/IR/LLVMContext.h"
#include <llvm/Support/SourceMgr.h>
#include <llvm/IRReader/IRReader.h>

#define TRACELINE() cerr << __LINE__ << endl;
int LogFunctionManager::getSizeIndex(int size) {
    switch (size) {
        case 8: return S8;
                break;
        case 16: return S16;
                 break;
        case 32: return S32;
                 break;
        case 64: return S64;
                 break;
        default: return S8;
                 break;
    };
}

string LogFunctionManager::getInstrumentationLibPath() {
    const char * val = ::getenv("INST_LIB");
    if ((val == 0) || (strcmp(val,"") == 0)) {
        cerr << "INST_LIB path not set, defaulting to ./library/instrumentation.bc" << endl;
        return "./library/instrumentation.bc";
    }
    else {
        string s = val;
        s += "/instrumentation.bc";
        cerr << "Instrumentation bitcode at " << s << endl;
        return s;
    }
}

Function * LogFunctionManager::loadExternalFunction(Module *m, Module *extM, const char *name) {
    Function *fn = extM->getFunction(name);
    FunctionType *ft = fn->getFunctionType();
    Function *newFn = Function::Create(ft, Function::ExternalWeakLinkage, name, m);
    return newFn;
}


void LogFunctionManager::loadFunctions(Module *m) {
    LLVMContext context;
    SMDiagnostic error;
    Module *lib = ParseIRFile(getInstrumentationLibPath().c_str(), error, m->getContext());

    cerr << "Loading external functions...";
    logFunctions[FLOAT][S8] = loadExternalFunction(m, lib, "logAccessF8");
    logFunctions[FLOAT][S16] = loadExternalFunction(m, lib, "logAccessF16");
    logFunctions[FLOAT][S32] = loadExternalFunction(m, lib, "logAccessF32");
    logFunctions[FLOAT][S64] = loadExternalFunction(m, lib, "logAccessF64");
    logFunctions[INTEGER][S8] = loadExternalFunction(m, lib, "logAccessI8");
    logFunctions[INTEGER][S16] = loadExternalFunction(m, lib, "logAccessI16");
    logFunctions[INTEGER][S32] = loadExternalFunction(m, lib, "logAccessI32");
    logFunctions[INTEGER][S64] = loadExternalFunction(m, lib, "logAccessI64");
    ptrLogFunc = loadExternalFunction(m, lib, "logAccessPtr");
    stringLogFunc = loadExternalFunction(m, lib, "logAccessStaticString");
    allocLogFunc = loadExternalFunction(m, lib, "logAlloc");
    fnBeginLogFunc = loadExternalFunction(m, lib, "logFnBegin");
    fnEndLogFunc = loadExternalFunction(m, lib, "logFnEnd");
    initLogFunc = loadExternalFunction(m, lib, "logInit");
    exitLogFunc = loadExternalFunction(m, lib, "logExit");
    cerr << " done!" << endl;

    int i, j;
    for (i = 0; i < VALUE_TYPES_MAX; i++) {
        for (j = 0; j < VALUE_SIZES_MAX; j++) {
            if (logFunctions[i][j] == 0) {
                cerr << "Function not found, exiting\n";
                exit(-1);
            }
        }
    }
}


Function * LogFunctionManager::getLogFunction(Value *v, Function *parent) {
    Type *t = v->getType();
    int value_type;

    if (t->isFloatingPointTy()) {
        value_type = FLOAT;
        return logFunctions[FLOAT][S64];
    } else if (t->isIntegerTy()) {
        value_type = INTEGER;
    } else if (t->isPointerTy()) {
        value_type = INTEGER;
    } else {
        cerr << "Type not floating point or integer\n";
        return NULL;
    }

    int bitwidth = t->getScalarSizeInBits();
    if (t->isPointerTy()) {
        if (parent->getName().str().compare("__dinamite_tracepoint")) {
            return ptrLogFunc;
        } else {
            return stringLogFunc;
        }
    }

    int sizeidx;
    switch (bitwidth) {
        case 1:
        case 8:
            sizeidx = S8;
            break;
        case 16:
            sizeidx = S16;
            break;
        case 32:
            sizeidx = S32;
            break;
        case 64:
            sizeidx = S64;
            break;
        default:
            cerr << "Bit width not recognized: " << bitwidth << endl;
            exit(-1);
            break;  // :D
    }

    return logFunctions[value_type][sizeidx];
}

bool LogFunctionManager::isLogFunction(Function *f) {
    if (f->getName().equals(ptrLogFunc->getName())) return true;
    if (f->getName().equals(allocLogFunc->getName())) return true;
    int i, j;

    for (i = 0; i < VALUE_TYPES_MAX; i++) {
        for (j = 0; j < VALUE_SIZES_MAX; j++) {
            if (f->getName().equals(logFunctions[i][j]->getName())) {
                return true;
            }
        }
    }
    return false;
}
