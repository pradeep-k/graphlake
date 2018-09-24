#pragma once

#include "type.h"

void lubm_test(const string& typefile, const string& idir, const string& odir, int job);
void ldbc_test0(const string& conf_file, const string& idir, const string& odir);
void ldbc_test2(const string& odir);
void darshan_test0(const string& conf_file, const string& idir, const string& odir);

void netflow_test(const string& idir, const string& odir, int c);
void plain_test(vid_t v_count, const string& idir, const string& odir, int job);
