#pragma once

#include <string>
#include "type.h"
using std::string;

class darshan_manager {
 public:
    static void prep_graph(const string& conf_file, const string& idir, const string& odir);
    static void prep_vtable(const string& filename, const string& odir);
};
