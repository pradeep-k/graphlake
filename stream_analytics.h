#pragma once
#include <omp.h>
#include <algorithm>

#include "graph.h"
#include "wtime.h"

#include "sgraph.h"
//#include "p_sgraph.h"

typedef int cid_t;
class wcc_t {
  public:
    cid_t*  v_cid;
    cid_t*  c_cid;
    cid_t   c_count;
  public:
    wcc_t() {
        v_cid = 0;
        c_cid = 0;
        c_count = 0;
    }
};
const cid_t invalid_cid = -1;

template <class T>
void wcc_post_reg(stream_t<T>* streamh, vid_t v_count) {
    wcc_t* wcc = new wcc_t();
    wcc->v_cid =  (cid_t*)calloc(v_count, sizeof(cid_t));
    wcc->c_cid =  (cid_t*)calloc(v_count, sizeof(cid_t));
    memset(wcc->v_cid, invalid_cid, sizeof(cid_t)*v_count);
    memset(wcc->c_cid, invalid_cid, sizeof(cid_t)*v_count);
    streamh->set_algometa(wcc);
} 

inline void map_cid(cid_t c1, cid_t c, cid_t* c_cid)
{
    c_cid[c1] = c;
}

//This the union function
inline void union_cid()
{
}
inline cid_t find(cid_t x, cid_t* c_cid)
{
    cid_t prev;
     while (c_cid[x] != x) {
         prev = x;
         x = c_cid[x];
         c_cid[prev] = c_cid[x];
     }
     return x;
}
template <class T>
void wcc_edge(vid_t v0, vid_t v1, wcc_t* wcc)
{
    if (v0 == v1) return;

    cid_t c00 = invalid_cid, c11 = invalid_cid;
    cid_t c0 = wcc->v_cid[v0];
    cid_t c1 = wcc->v_cid[v1];
    int sw = (c0 != invalid_cid) + ((c1 != invalid_cid) << 1);
    cid_t m = 0;

    switch(sw) {
    case 0: //if ((c0 == invalid_cid) && (c1 == invalid_cid)) {
        m = __sync_fetch_and_add(&wcc->c_count, 1);
        wcc->v_cid[v1] = m;
        wcc->v_cid[v0] = m;
        map_cid(m, m, wcc->c_cid);
        //++t_front_count;
        break;
    case 1: //} else if(c1 == invalid_cid) {
        c00 = find(c0, wcc->c_cid);
        wcc->v_cid[v1] = c00;
        //++t_front_count;
        break;
    case 2: // } else if (c0 == invalid_cid) {
        c11 = find(c1, wcc->c_cid);
        wcc->v_cid[v0] = c11;
        //++t_front_count;
        break;
    case 3: // } else if (c0 != c1) {
        c00 = find(c0, wcc->c_cid); //wcc->c_cid[wcc->v_cid[v0]];
        c11 = find(c1, wcc->c_cid); //wcc->c_cid[wcc->v_cid[v1]];
        if (c00 < c11) {
            //if (cid[c1] == c1)
            wcc->v_cid[v1] = c00;
            //map_cid(c1, c00, wcc->c_cid);
            map_cid(c11, c00, wcc->c_cid);
            //++t_front_count;
        } else if (c00 >= c11) {
            //if (cid[c0] == c0)
            wcc->v_cid[v0] = c11;
            //map_cid(c0, c11, wcc->c_cid);
            map_cid(c00, c11, wcc->c_cid);
            //++t_front_count;
        }
        break;
    default:
        assert(0);
    }
}

template <class T>
void wcc_finalize(stream_t<T>* streamh)
{
    cid_t count = 0;
    wcc_t* wcc = (wcc_t*)streamh->get_algometa();
    #pragma omp parallel for reduction(+:count)
    for(cid_t i = 0; i < wcc->c_count; ++i) {
        count += (i == wcc->c_cid[i]);
    }
    
    cout << "WCC count = " << count << endl;
    cout << "wcc_group used <debug> =" << wcc->c_count << endl;
}

template <class T>
void do_stream_wcc(stream_t<T>* streamh)
{
    edgeT_t<sid_t>* edges = streamh->get_edges();
    index_t edge_count = streamh->get_edgecount();
    vid_t src, dst;
    
    wcc_t* wcc = (wcc_t*)streamh->get_algometa();

    for (index_t i = 0; i < edge_count; ++i) {
        src = edges[i].src_id;
        dst = get_dst(edges+i);
        wcc_edge<T>(src, dst, wcc);
    }
}
