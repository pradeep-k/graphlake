#pragma once

template <class T>
class numkv_t {
 public:
    T* kv;
    tid_t  tid;
    
    int    vtf;   //vertex table file

 public:
    numkv_t(); 
 
 public: 
    void setup(tid_t tid); 
    void set_value(vid_t vid, T& value); 
    T get_value(vid_t vid);
    void persist_vlog();
    void persist_elog();
    void read_vtable();
    void read_etable();
    void file_open(const string& filename, bool trunc);
};
#include <string.h>
#include "graph.h"

template <class T>
numkv_t<T>::numkv_t()
{
    kv = 0;
    tid = 0;
    vtf = -1;
}

template <class T>
void numkv_t<T>::set_value(vid_t vid, T& value)
{
    kv[vid] = value;
}

template <class T>
T numkv_t<T>::get_value(vid_t vid)
{
    return kv[vid];
}

template <class T>
void numkv_t<T>::setup(tid_t t) 
{
    tid = t;
    vid_t v_count = g->get_type_scount(tid);
    kv = (T*)calloc(sizeof(T), v_count);
}

template <class T>
void numkv_t<T>::persist_vlog()
{
    vid_t v_count = g->get_type_vcount(tid);
    if (v_count != 0) {
        pwrite(vtf, kv, v_count*sizeof(sid_t), 0);
    }
}

template <class T>
void numkv_t<T>::file_open(const string& filename, bool trunc)
{
    char  file_ext[16];
    sprintf(file_ext,"%u",tid);
    
    string vtfile = filename + file_ext + ".vtable";
    if (trunc) {
        //vtf = fopen(vtfile.c_str(), "wb");
		vtf = open(vtfile.c_str(), O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);

    } else {
	    vtf = open(vtfile.c_str(), O_RDWR|O_CREAT, S_IRWXU);
        //vtf = fopen(vtfile.c_str(), "r+b");
    }
        
    assert(vtf != -1);
}

template <class T>
void numkv_t<T>::read_vtable()
{
    //read vtf 
    index_t size = fsize(vtf);
    if (size == -1L) { assert(0); }
    
    if (size != 0) {
        vid_t vcount = size/sizeof(sid_t);
        assert(vcount == g->get_type_vcount(tid));
        read(vtf, kv, sizeof(T)*vcount);
    }
}
