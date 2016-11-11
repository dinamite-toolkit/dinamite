#include "AllocDefs.hpp"

AllocDefinition *AllocDefManager::getAllocDef(Function *f) {
    for (auto it = allocDefs.begin(); it != allocDefs.end(); it++) {
        if (it->name.compare(f->getName().str()) == 0) {
            return &(*it);
        }
    }
    return NULL;
}

void AllocDefManager::loadAllocDefs() {
    ifstream file(getAllocInPath());
    string line;
    if (!file.is_open()) {
        cerr << "Error opening allocation definitions. Falling back to standard m/calloc.\n" << endl;

        AllocDefinition mallocDef;
        mallocDef.name = "malloc";
        mallocDef.sizeIdx = 0;
        mallocDef.numIdx = -1;
        mallocDef.addrIdx = -1;
        allocDefs.push_back(mallocDef);

        AllocDefinition callocDef;
        mallocDef.name = "calloc";
        mallocDef.sizeIdx = 1;
        mallocDef.numIdx = 0;
        mallocDef.addrIdx = -1;
        allocDefs.push_back(mallocDef);
    }
    while (getline(file, line)) {
        // trim perhaps?
        if (line[0] == '#') continue;

        AllocDefinition allocDef;
        istringstream iss(line);
        if (!(iss >> allocDef.name >> allocDef.numIdx >> allocDef.sizeIdx >> allocDef.addrIdx)) {
            cerr << "Error parsing allocation input, exiting\n" << endl;
            exit(-1);
        }
        cerr << "Adding allocdef " << allocDef.name << endl;
        allocDefs.push_back(allocDef);
    }
}
string AllocDefManager::getAllocInPath() {
    const char * val = ::getenv("ALLOC_IN");
    if ((val == 0) || (strcmp(val,"") == 0)) {
        cerr << "ALLOC_IN path not set, defaulting to ./alloc.in" << endl;
        return "./alloc.in";
    }
    else {
        string s = val;
        s += "/alloc.in";
        cerr << "alloc.in at " << s << endl;
        return s;
    }
}
