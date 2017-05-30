#include <assert.h>
#include <iostream>

#include "type.h"
#include "graph.h"

void rset_t::bitwise2vlist()
{
    int uniontype = get_uniontype();
    if (uniontype != eStatusarray) {
        return;
    }

    uint64_t* barray  = get_barray();
    vid_t     w_count = get_wcount();
    vid_t   list_count = get_vcount();
    vid_t*  vid_list = (vid_t*)calloc(sizeof(vid_t), list_count);
    list_count = 0; 
        
    vid_t     word, base, frontier;
    tid_t     pos, count;
    
    //Get the frontiers
    for (vid_t w = 0; w < w_count; w++) {
        if ( 0 == barray[w]) continue;
        
        word  = barray[w];
        count = __builtin_popcountll(word);
        base  = (w << 6);

        for (tid_t j = 0; j < count; ++j) {
            pos = __builtin_ctzll(word);
            word  ^= (1L << pos);//reset that position
            frontier = pos + base;
            vid_list[list_count] = frontier;
            ++list_count;
        }
    }
    free(barray);
    vlist = vid_list;
    scount = TO_THIGH(scount) + list_count;
    count2 = TO_SUPER(eFrontiers) + list_count;
}

void rset_t::copy_setup(rset_t* iset, int union_type) 
{
    assert(eStatusarray != TO_TID(iset->count2));
    vid_t v_count = iset->get_vcount();
    scount = TO_THIGH(iset->scount) + v_count;
    count2 = TO_SUPER(union_type) + v_count;
    switch(union_type) {
    case eFrontiers:
        vlist = (vid_t*)calloc(sizeof(vid_t), v_count);
        break;
    case eAdjlist:
        adjlist = (beg_pos_t*)calloc(sizeof(beg_pos_t), v_count);
        break;
    case eKV:
        kv = (sid_t*)calloc(sizeof(sid_t), v_count);
        break;
    default:
    assert(0);
    }
}

void rset_t::print_vlist(select_info_t* select_info, qid_t select_count)
{
    vid_t* varray = get_vlist();
    vid_t v_count = get_vcount();
    sid_t tid = get_tid();
    vid_t frontier;


    for (vid_t j = 0; j < v_count; ++j) {
        frontier = varray[j];
        for (int j = 0; j < select_count; ++j) {
            select_info[j].rgraph->print_raw_dst(tid, frontier, select_info[j].pid);
            cout << ",";
        }
    }
}

void rset_t::print_adjlist(select_info_t* select_info, qid_t select_count, vid_t pos)
{
    beg_pos_t* varray = get_graph();
    sid_t* v_adjlist = varray[pos].get_adjlist();
    vid_t  v_count = v_adjlist[0];
    ++v_adjlist;

    sid_t sid;
    vid_t frontier;
    tid_t tid;

    for (vid_t j = 0; j < v_count; ++j) {
        sid = v_adjlist[j];
        frontier = TO_VID(sid);
        tid = TO_TID(sid);
        
        for (int j = 0; j < select_count; ++j) {
            select_info[j].rgraph->print_raw_dst(tid, frontier, select_info[j].pid);
            cout << ",";
        }
    }
}

void rset_t::print_kv(select_info_t* select_info, qid_t select_count, vid_t pos)
{
    sid_t* kv = get_kv();
    sid_t sid = kv[pos];
    vid_t frontier = TO_VID(sid);
    tid_t tid = TO_TID(sid);

    for (int j = 0; j < select_count; ++j) {
        select_info[j].rgraph->print_raw_dst(tid, frontier, select_info[j].pid);
        cout << ",";
    }
}

void rset_t::print_barray(select_info_t* select_info, qid_t select_count)
{
    assert(eStatusarray == get_uniontype());

    uint64_t* barray  = get_barray();
    vid_t     w_count = get_wcount();
    tid_t         tid = get_tid();    
    
    vid_t     word, base, frontier;
    tid_t     pos, count;
    
    //Get the frontiers
    for (vid_t w = 0; w < w_count; w++) {
        if ( 0 == barray[w]) continue;
        
        word  = barray[w];
        count = __builtin_popcountll(word);
        base  = (w << 6);

        for (tid_t j = 0; j < count; ++j) {
            pos = __builtin_ctzll(word);
            word  ^= (1L << pos);//reset that position
            frontier = pos + base;
            //Print
            for (int j = 0; j < select_count; ++j) {
                select_info[j].rgraph->print_raw_dst(tid, frontier, select_info[j].pid);
                cout << ",";
            }
        }
    }
}

void rset_t::print_result(select_info_t* select_info, qid_t select_count, vid_t vid_pos)
{
    int uniontype = get_uniontype();

    if (uniontype == eFrontiers) {
        assert(0);
        print_vlist(select_info, select_count);
    } else if (uniontype == eAdjlist) {
        print_adjlist(select_info, select_count, vid_pos);
    } else if (uniontype == eKV) {
        print_kv(select_info, select_count, vid_pos);
    } else if (uniontype == eStatusarray) {
        assert(0);
        print_barray(select_info, select_count);;
    }
}

/********************************************/
void srset_t::bitwise2vlist()
{
    tid_t    iset_count = get_rset_count();
    
    for (tid_t i = 0; i < iset_count; ++i) {
        //apply other filters
        rset[i].bitwise2vlist();
    }
}

tid_t srset_t::full_setup(sflag_t a_flag) 
{
    sflag_t sflag = 0;
    if (tfilter_count) {
        for (tid_t i = 0; i < tfilter_count; ++i) {
            sflag |= a_flag & (1L << tfilter[i]);
        }
    } else {
        sflag = a_flag;
    }
    tid_t flag_count = __builtin_popcountll(sflag);
    tid_t t_count = g->get_total_types();
    ccount = TO_SUPER(flag_count);
    flag = (tid_t*) malloc(sizeof(tid_t)*t_count);
    memset(flag, INVALID_TID, sizeof(tid_t)*t_count);
    rset = (rset_t*)calloc(sizeof(rset_t), flag_count);
    
    tid_t pos = 0;
    sid_t super_id;
    for (tid_t i = 0; i < flag_count; ++i) {
        pos = __builtin_ctzll(sflag);
        sflag ^= (1L << pos);//reset that position
        super_id = g->get_type_scount(pos);
        flag[pos] = i;
        rset[i].setup(super_id);
       
    }
    return flag_count;
}

tid_t srset_t::full_setup(sgraph_t** sgraph) 
{
    tid_t t_count = g->get_total_types();
    flag = (tid_t*) malloc(sizeof(tid_t)*t_count);
    memset(flag, INVALID_TID, sizeof(tid_t)*t_count);
    
    tid_t flag_count = 0;
    
    if (tfilter_count) {
        for(tid_t i = 0; i < tfilter_count; i++) {
            if (0 != sgraph[tfilter[i]]) {
                flag[tfilter[i]] = flag_count;
                ++flag_count;
            }
        }
    } else {
        for(tid_t i = 0; i < t_count; i++) {
            if (0 != sgraph[i]) {
                flag[i] = flag_count;
                ++flag_count;
            }
        }
    }
    
    ccount = TO_SUPER(flag_count);
    rset = (rset_t*)calloc(sizeof(rset_t), flag_count);
    
    tid_t pos = 0;
    sid_t super_id;
    for (tid_t i = 0; i < t_count; ++i) {
        if (INVALID_TID == flag[i]) continue;
        super_id = g->get_type_scount(i);
        pos = flag[i];
        rset[pos].setup(super_id);
    }
    return flag_count;
}

tid_t srset_t::full_setup(skv_t** skv) 
{
    tid_t t_count = g->get_total_types();
    flag = (tid_t*) malloc(sizeof(tid_t)*t_count);
    memset(flag, INVALID_TID, sizeof(tid_t)*t_count);
    
    tid_t flag_count = 0;
    
    if (tfilter_count) {
        for(tid_t i = 0; i < tfilter_count; i++) {
            if (0 != skv[tfilter[i]]) {
                flag[tfilter[i]] = flag_count;
                ++flag_count;
            }
        }
    } else {
        for(tid_t i = 0; i < t_count; i++) {
            if (0 != skv[i]) {
                flag[i] = flag_count;
                ++flag_count;
            }
        }
    }
    
    ccount = TO_SUPER(flag_count);
    rset = (rset_t*)calloc(sizeof(rset_t), flag_count);
    
    tid_t pos = 0;
    sid_t super_id;
    for (tid_t i = 0; i < t_count; ++i) {
        if (INVALID_TID == flag[i]) continue;
        super_id = g->get_type_scount(i);
        pos = flag[i];
        rset[pos].setup(super_id);
    }
    return flag_count;
}

//Filter can not be applied to eAdjlist and eKV
tid_t srset_t::copy_setup(srset_t* iset, int union_type) 
{
    tid_t t_count = g->get_total_types();
    ccount = iset->ccount;

    tid_t flag_count = get_rset_count();
    flag = (tid_t*) malloc(sizeof(tid_t)*t_count);
    memcpy(flag, iset->flag, sizeof(tid_t)*t_count);
    rset = (rset_t*)calloc(sizeof(rset_t), flag_count);
    for (tid_t i = 0; i < flag_count; ++i) {
        rset[i].copy_setup(iset->rset + i, union_type);
    }
    return flag_count;
}

//Should not called directly
tid_t srset_t::setup(tid_t tid) 
{
    tid_t t_count = g->get_total_types();
    rset        = new rset_t;
    flag        = (tid_t*)malloc(sizeof(tid_t)*t_count);
    memset(flag, INVALID_TID, sizeof(tid_t)*t_count); 
    flag[tid] = 0;
    ccount = TO_SUPER(1);
    return 1;
}

void srset_t::setup_select(qid_t a_count) { 
    select_count = a_count;
    select_info  = new select_info_t[a_count]; 
}

void srset_t::create_select(qid_t index, const char* a_name, const char* prop_name) 
{
    select_info[index].name = gstrdup(a_name);
    
    if (0 == prop_name) {
        select_info[index].rgraph = g->v_graph;
        return;
    }

    propid_t pid = g->get_pid(prop_name);
    propid_t cf_id = g->get_cfid(pid);
    assert(INVALID_PID != pid);
    select_info[index].rgraph = g->cf_info[cf_id];
    select_info[index].pid = pid;
    
}


void srset_t::print_result(tid_t tid_pos, vid_t vid_pos)
{
    //pass filter_info
    rset[tid_pos].print_result(select_info, select_count, vid_pos);
}

status_t srset_t::apply_typefilter(tid_t tid)
{
    for (tid_t i = 0; i < tfilter_count; ++i) { 
        if (tid == tfilter[i]) return eOK;
    }
    return eQueryFail;
}

//////////////////////////////////////////////////////////
void skv_t::setup(tid_t tid)
{
    if (0 == super_id) {
        super_id = g->get_type_scount(tid);
        vid_t v_count = TO_VID(super_id);
        max_vcount = (v_count << 2);
        kv = (vid_t*)calloc(sizeof(vid_t), max_vcount);
    } else {
        super_id = g->get_type_scount(tid);
        vid_t v_count = TO_VID(super_id);
        if (max_vcount < v_count) {
            assert(0);
        }
    }
}

void sgraph_t::setup(tid_t tid)
{
    if(0 == super_id) {
        super_id = g->get_type_scount(tid);
        vid_t v_count = TO_VID(super_id);
        max_vcount = (v_count << 1);
        beg_pos = (beg_pos_t*)calloc(sizeof(beg_pos_t), max_vcount);
        nebr_count = (vid_t*)calloc(sizeof(vid_t), max_vcount);
    } else {
        super_id = g->get_type_scount(tid);
        vid_t v_count = TO_VID(super_id);
        if (max_vcount < v_count) {
            assert(0);
        }
    }
}

void sgraph_t::setup_adjlist()
{
    vid_t v_count = TO_VID(super_id);
    sid_t count;
    sid_t* adj_list = 0;
    vid_t v = 0;

    for (vid_t vid = 0; vid < v_count; ++vid) {
        adj_list = beg_pos[vid].adj_list;
        count = nebr_count[vid];

        if (adj_list && adj_list[0] != count) {
            beg_pos[vid].adj_list = log_beg + log_head;
            log_head += count + 1;
            memcpy(beg_pos[vid].adj_list, adj_list, adj_list[0]*sizeof(sid_t));
            
            dvt[v].vid = vid;
            dvt[v].degree = count;
            dvt[v].file_offset = log_head;
            ++v;
        } else {
            beg_pos[vid].adj_list = log_beg + log_head; //calloc(sizeof(vid_t), count+1);
            log_head += count + 1; 
            
            dvt[v].vid = vid;
            dvt[v].degree = count;
            dvt[v].file_offset = log_head;
            ++v;
        }
        nebr_count[vid] = beg_pos[vid].get_nebrcount();
    }
    dvt_count = v;
}

void sgraph_t::persist_edgelog(const string& etfile)
{
    //Make a copy
    sid_t wpos = log_wpos;
    
    //Update the mark
    log_wpos = log_head;
        
    //Write the file.
    if (etf == 0) {
        etf = fopen(etfile.c_str(), "a+b");//append/write + binary
        assert(etf != 0);
    }
    fwrite(log_beg+wpos, sizeof(sid_t), log_head-wpos, etf);
}

void sgraph_t::persist_vlog(const string& vtfile)
{
    //Make a copy
    sid_t count =  dvt_count;

    //update the mark
    dvt_count = 0;

    //Write the file
    if(vtf == 0) {
        vtf = fopen(vtfile.c_str(), "a+b");
        assert(vtf != 0);
    }
    fwrite(dvt, sizeof(disk_vtable_t), count, vtf);
}
