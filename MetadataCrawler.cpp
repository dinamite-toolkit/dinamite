#include "MetadataCrawler.hpp"

#include <sstream>

void MetadataCrawler::traverseMetadata(MDNode *mdn, MDTraversal *traversal) {
    if (mdn == 0) {
        return;
    }
    if (visited.count(mdn) > 0) {
        return;
    } else {
        visited.insert(mdn);
    }
    unsigned int n_op = mdn->getNumOperands();
    traversal->traverse(mdn);
    for (int i = 0; i < n_op; i++) {
        if (mdn->getOperand(i)) {
            if (MDNode *child = dyn_cast<MDNode>(mdn->getOperand(i))) {
                traverseMetadata(child, traversal);
            }
        }
    }

}

void MetadataCrawler::crawlModule(Module &m) {
    NamedMDNode *nmd = m.getNamedMetadata("llvm.dbg.cu");
    crawlNMD(nmd, &typedefTraversal);
    crawlAllInstructions(m, &typedefTraversal);

    visited.clear();

    crawlNMD(nmd, &fieldNameTraversal);
    crawlAllInstructions(m, &fieldNameTraversal);
}

void MetadataCrawler::crawlNMD(NamedMDNode *nmd, MDTraversal *mdt) {
    if (nmd == NULL) { 
        return;
    }
    unsigned int n_op = nmd->getNumOperands();
    for (int i = 0; i < n_op; i++) {
        if (nmd->getOperand(i)) {
            if (MDNode *child = dyn_cast<MDNode>(nmd->getOperand(i))) {
                traverseMetadata(child, mdt);
            }
        }
    }

}

void MetadataCrawler::crawlAllInstructions(Module &m, MDTraversal *mdt) {
    for (Function &f : m) {
        for (BasicBlock &b : f) {
            for (Instruction &in : b) {
                if (CallInst* CI = dyn_cast<CallInst>(&in)){
                    if(Function *F = CI->getCalledFunction()){
                        if(F->getName().startswith("llvm.")){
                            for(unsigned i = 0, e = CI->getNumOperands(); i!=e; ++i){
                                if(MDNode *N = dyn_cast_or_null<MDNode>(CI->getOperand(i))){
                                    traverseMetadata(N, mdt);
                                }
                            }
                        }
                    }
                }

                SmallVector< std::pair< unsigned, MDNode * >, 4> mdvect;
                in.getAllMetadata(mdvect);

                for (int i = 0; i < mdvect.size(); i++) {
                    traverseMetadata(mdvect[i].second, mdt);

                }
            }
        }
    }
}

void TypedefTraversal::traverse(MDNode *mdn) {
    if (MDNode *dit = dyn_cast<MDNode>(mdn)) {
        if ( mdn->getNumOperands() > 0) {
            if (mdn->getOperand(0)){ 
                if (ConstantInt *civ = dyn_cast<ConstantInt>(mdn->getOperand(0))) {
                    unsigned int tag = civ->getSExtValue();
                    if (CHECK_TAG(tag, llvm::dwarf::DW_TAG_typedef)) {
                        if (mdn->getNumOperands() > 9) {
                            MDString *mds;
                            if ((mdn->getOperand(3)) && (mds = dyn_cast<MDString>(mdn->getOperand(3)))) {
                                string tdefname(mds->getString().str());
                                MDNode *structptr;
                                if (mdn->getOperand(9) && (structptr = dyn_cast<MDNode>(mdn->getOperand(9)))) {
                                    if (mdc->typedefMap.count(structptr) == 0) {
                                        mdc->typedefMap[structptr] = tdefname;
                                    }
                                }
                            } else { // has no name
                                MDNode *structptr;
                                if ((mdn->getOperand(9)) && (structptr = dyn_cast<MDNode>(mdn->getOperand(9)))) {
                                    if (mdc->typedefMap.count(structptr) == 0) {
                                        ostringstream oss;
                                        string tdefname = oss.str();
                                        mdc->typedefMap[structptr] = tdefname;
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }
    }
}

void FieldNameTraversal::traverse(MDNode *mdn) {
    if (MDNode *dit = dyn_cast<MDNode>(mdn)) {
        if ( mdn->getNumOperands() > 0) {
            if (mdn->getOperand(0)){ 
                if (ConstantInt *civ = dyn_cast<ConstantInt>(mdn->getOperand(0))) {
                    unsigned int tag = civ->getSExtValue();
                    if (CHECK_TAG(tag, llvm::dwarf::DW_TAG_class_type) || CHECK_TAG(tag, llvm::dwarf::DW_TAG_structure_type)) {
#ifdef DEBUG_PRINT
                        if (CHECK_TAG(tag, llvm::dwarf::DW_TAG_class_type)) {
                            cerr << "Class type!" <<endl;
                        } else {
                            cerr << "Struct type!" <<endl;
                        }
#endif
                        if ((mdn->getNumOperands() > 3) && (mdn->getOperand(3))) {
                            StringRef className = mdn->getOperand(3)->getName();
                            if (className.str() == "") {
                                if (mdc->typedefMap.count(mdn)) {
                                    className = StringRef(mdc->typedefMap[mdn]);
                                } else if ((mdn->getNumOperands() > 14) && (mdn->getOperand(14))) {
                                    string dmangleClass = demangle(mdn->getOperand(14)->getName().str().c_str());
                                    if (dmangleClass.find("typeinfo name for ") != string::npos) {
                                        dmangleClass.erase(0, strlen("typeinfo name for "));
                                    }
                                    className = StringRef(dmangleClass);
#ifdef DEBUG_PRINT
                                    cerr << "cname14 " << className.str() << endl;
#endif
                                } else {
                                    className = StringRef("<unknown>");
                                }
                            }

                            if (mdn->getNumOperands() > 10) {
                                if (mdn->getOperand(10)) {
                                    if (MDNode *fields = dyn_cast<MDNode>(mdn->getOperand(10))) {
                                        string sClassName(className.str());
                                        mdc->getFields(sClassName, fields);
                                    }
                                }
                            }
                        }
                    }
                } 
            }
        }
    }
}

void MetadataCrawler::getFields(string className, MDNode *fields) {
    unsigned int n_op = fields->getNumOperands();
    for (int i = 0; i < n_op; i++) {
        if (fields->getOperand(i)) {
            if (MDNode *f = dyn_cast<MDNode>(fields->getOperand(i))) {
                string fieldName;
                if ((f->getNumOperands() > 3) && (f->getOperand(3))) {
                    fieldName = f->getOperand(3)->getName().str();
                } else {
                    fieldName = "unknown";
                }
                fieldMap[className].push_back(fieldName);
                //cerr << className << "." << fieldName << endl;
            }
        }
    }
}

string MetadataCrawler::getFieldName(string structName, int fieldIdx) {
    if (fieldMap.count(structName) > 0) {
        if (fieldIdx >= fieldMap[structName].size()) {
            return "unknown";
        } else {
            return fieldMap[structName].at(fieldIdx);
        }
    }
    return "NA";
}
