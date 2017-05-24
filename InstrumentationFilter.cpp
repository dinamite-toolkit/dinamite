#include "InstrumentationFilter.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>

using namespace json11;
using namespace std;

//#define INSFILT_DEBUG

void InstrumentationFilter::loadFilterDataEnv() {
    const char * val = ::getenv("DIN_FILTERS");
    if ((val == 0) || (strcmp(val,"") == 0)) {
        cerr << "DIN_FILTERS not set, allowing all instrumentation" << endl;
        return;
    }

    loadFilterData(val);
}

void InstrumentationFilter::loadFilterData(const char *filename) {

	cerr << "DIN_FILTERS with " << filename << " file " << endl;
	ifstream fin;
	fin.open(filename);
	if (!fin.is_open()) {
		cerr << "Error: Couldn't open " << filename <<
			", filters not loaded." << endl;
		return;
	}
	stringstream ss;
	ss << fin.rdbuf();
	string err;

	this->filters = Json::parse(ss.str(), err);
	if (!this->filters.is_null()) {
		loaded = true;
		cerr << "DINAMITE filtering set up successfully!" << endl;
	} else {
		cerr << "Error parsing filters!" << endl;
	}
}

string InstrumentationFilter::findBestFunctionMatch(string function_name) {
	Json jsFunc = filters["whitelist"]["function_filters"][function_name];
	if (!jsFunc.is_null()) {
		return function_name;
	}

	jsFunc = filters["whitelist"]["function_filters"];
	bool matched_star = false;

	for (auto it : jsFunc.object_items()) {
		if (it.first.compare("*") == 0) {
			matched_star = true;
		} else if (globMatch(it.first, function_name)) {
			return it.first;
		}
	}
	if (matched_star) {
		return "*";
	}
	return "";

}

bool InstrumentationFilter::checkFunctionFilter(string function_name,
						string event_type) {

	if (!loaded)
		return true; // No filters defined

	if (checkFunctionBlacklist(function_name)) {
#ifdef INSFILT_DEBUG
		cerr << "Function filter: Function " << function_name <<
			" blacklisted" << endl;
#endif
		return false;
	}

	if (function_name.compare("*") != 0) {
		if (checkFunctionFilter("*", event_type)) {
			return true;
		}
	}

	if  (filters["whitelist"]["function_filters"].object_items().size()
	     == 0) {
#ifdef INSFILT_DEBUG
		cerr << "Function filter: Function " << function_name <<
			" enabled, whitelist empty." << endl;
#endif
		return true;
	}

	string match_name = findBestFunctionMatch(function_name);
	Json eventarray =
		filters["whitelist"]["function_filters"][match_name]["events"];
	for (auto it : eventarray.array_items()) {
		if (event_type.compare(it.string_value()) == 0) {
#ifdef INSFILT_DEBUG
			cerr << "Function filter: Function " << function_name
			     << " event: " << event_type << " enabled as "
			     << match_name << endl;
#endif
			return true;
		}
	}
	return false;
}

bool InstrumentationFilter::checkFunctionArgFilter(string function_name, int arg) {
    if (!loaded) return false; // don't print args if we haven't enabled it explicitly

	string match_name = findBestFunctionMatch(function_name);
	Json argfilter = filters["whitelist"]["function_filters"][match_name]["arguments"];
    if (argfilter.is_array()) {
        for (auto it : argfilter.array_items()) {
            if (it.int_value() == arg) {
                return true;
            }
        }
    } else if (argfilter.is_string()) {
        if (argfilter.string_value().compare("*") == 0) {
            return true;
        }
    }

    return false;
}

bool InstrumentationFilter::checkFileFilter(string file_name) {
	if (!loaded)
		return true;

	if (checkFileBlacklist(file_name))
		return false;

	Json filearray = filters["whitelist"]["file_filters"];
	if (filearray.is_null())
		return true;

	if  (filearray.array_items().size() == 0)
		return true;

	for (auto it : filearray.array_items()) {
		if (globMatch("*" + it.string_value(), file_name)) {
			return true;
		}
	}
	return false;
}

bool InstrumentationFilter::checkFunctionBlacklist(string function_name) {
	if (!loaded)
		return false;

	Json functionarray = filters["blacklist"]["function_filters"];

	if (functionarray.is_null())
		return false;

	for (auto it : functionarray.array_items()) {
		if (globMatch("*" + it.string_value(), function_name))
			return true;
	}
	return false;
}

bool InstrumentationFilter::checkFileBlacklist(string file_name) {
	if (!loaded)
		return false;

	Json filearray = filters["blacklist"]["file_filters"];
	if (filearray.is_null())
		return false;

	for (auto it : filearray.array_items()) {
		if (globMatch("*" + it.string_value(), file_name))
			return true;
	}
	return false;
}

bool InstrumentationFilter::loopCheckEnabled() {
    if (filters["check_small_function_loops"].is_null())
	    return true;

#ifdef INSFILT_DEBUG
    cerr << "check_small_function_loops is " <<
	    filters["check_small_function_loops"].bool_value() << endl;
#endif
    return filters["check_small_function_loops"].bool_value();
}

bool InstrumentationFilter::checkFunctionSize(string function_name,
					      size_t size) {

	if ((size > 0) && (functions.count(function_name) == 0)) {
		/* Add to the list of encountered functions, mostly
		 * used for mangled stuff
		 */
		functions[function_name] = size;
	}

	if (filters["minimum_function_size"].is_null()) {
		return true;
	}
	if (size >= (size_t) filters["minimum_function_size"].int_value()) {
#ifdef INSFILT_DEBUG
		cerr << "Size filter: Function size " << function_name << " "
		     << size << ", greater than minimum function size" << endl;
#endif
		return true;
	}
	string bestMatch = findBestFunctionMatch(function_name);
	if ((bestMatch.compare("*") != 0) && (bestMatch.compare("") != 0)) {
#ifdef INSFILT_DEBUG
		cerr << "Size filter: Function " << function_name << " size: "
		     << size << ", matched against " << bestMatch << endl;
#endif
		return true;
	}

#ifdef INSFILT_DEBUG
        cerr << "Size filter: Function " << function_name << " size: " << size
	     << " doesn't match whitelist filters based on size." << endl;
#endif
	return false;
}

bool InstrumentationFilter::globMatch(string glob, string s) {

	/* both done, matched */
	if ((glob.length() == 0) && (s.length() == 0))
		return true;
        /* one is done, other has leftover characters */ 
	if (glob.length() * s.length() == 0) {
		if (glob.compare("*") == 0)
			return true;
		return false;
	}
	if (glob[0] == '*') { // recurse for both match cases
		return 
			globMatch(glob.substr(1), s.substr(1)) || 
			globMatch(glob, s.substr(1)) || 
			globMatch(glob.substr(1), s);
	}
	if (glob[0] != s[0]) {  // regular characters
		return false;
	} else {
		return globMatch(glob.substr(1), s.substr(1));
	}
}

fn_size_metrics InstrumentationFilter::getFunctionSizeMetric() {
	if (!filters["function_size_metric"].is_null()) {
		if (filters["function_size_metric"].string_value().compare("IR")
		    == 0) {
			return FN_SIZE_IR;
		}
		if (filters["function_size_metric"].string_value().compare(
			    "LOC_PATH") == 0) {
			return FN_SIZE_PATH;
		}
	}
	return FN_SIZE_LOC;
}

// some simple glob tests to verify
#define GLOBTEST(x, y) cerr << "TEST " << x << " " << y << ": " << globMatch(x,y) << endl;
void InstrumentationFilter::testGlob() {
    GLOBTEST("abc*", "abcd");
    GLOBTEST("a*bc", "abcd");
    GLOBTEST("*bc*", "abcd");
    GLOBTEST("a*f", "abcd");
    GLOBTEST("a**d", "abcd");
    GLOBTEST("a*c*d", "abcdd");
    GLOBTEST("*main*", "main");
    exit(-1);
}
