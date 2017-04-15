#pragma once
#include <stdint.h>
#include <limits.h>
#include <string>
#include "bitmap.h"

using std::string;


typedef uint32_t propid_t;
typedef uint64_t vid_t;
typedef uint64_t sid_t;
typedef uint64_t index_t;
typedef uint32_t tid_t;
typedef uint64_t sflag_t;
typedef uint16_t qid_t;
//typedef int32_t status_t;

#define VBIT 40
#define VMASK 0xffffffffff
#define THIGH_MASK 0xFFFFFF0000000000


#define TO_TID(sid) (sid >> VBIT)
#define TO_VID(sid)  (sid & VMASK)
#define TO_SUPER(tid) (((sid_t)(tid)) << VBIT)
#define TO_THIGH(sid) (sid & THIGH_MASK)

#define TID_TO_SFLAG(tid) (1L << tid)
#define WORD_COUNT(count) ((count + 63) >> 6)


#define INVALID_PID 0xFFFFFFFF
#define INVALID_TID 0xFFFFFF
#define INVALID_SID 0xFFFFFFFFFFFFFFFF

#define NO_QID 0xFFFF

class pinfo_t;


enum direction_t {
    eout = 0, 
    ein
};

enum status_t {
    eOK = 0,
    eInvalidPID,
    eInvalidVID,
    eQueryFail,
    eUnknown        
};

typedef union __univeral_type {
    char*    value_charp;
    uint64_t value_64b;
    uint8_t  value_8b;
    uint16_t value16b;
    tid_t    value_tid;
    vid_t    value_vid;
    sid_t    value_sid;
}univ_t;

typedef struct __sid_set_t {
public:
    int count;
    sid_t* sids;
} sid_set_t;

enum filter_fn_t {
    fn_out = 0,//simple lookup 
    fn_ein, //simple inward lookup
    //More coming soon such as regex
};

//required when src and dst both are variable 
//but one is populated from another query.
enum queryplan_t {
    eOutward = 0,
    eInward,
    eDefault,
};

enum traverse_t {
    eTransform = 0,
    eExtend,
};

class filter_info_t {
 public:
    //always one count
    pinfo_t* rgraph;

    qid_t count;
    //these could be more, controlled by count
    univ_t   value;
    filter_fn_t filter_fn;
    
    inline filter_info_t() {
        rgraph = 0;
        count = 0;
    }
 
 private:
    inline void set_filterobj(pinfo_t* a_graph, univ_t a_value, filter_fn_t fn) {
        rgraph = a_graph;
        value = a_value;
        filter_fn = fn;
    }
} ;

class type_filter_t {
 public:
     tid_t tid_value;
};

typedef struct __select_info_t {
    pinfo_t* rgraph;
    string name; 
} select_info_t;



class edge_t {
public:
    vid_t src_id;
    vid_t dst_id;
};

class rset_t;
//One vertex's neighbor information
class beg_pos_t {
 private:
     //count, flag for snapshot, XXX: smart pointer count
    //count in adj list
    index_t  count;

    //nebr list of one vertex
    sid_t*   adj_list;

 public:
    //used during initial setup only
    inline void set_adjlist(sid_t* a_adjlist) {
        adj_list = a_adjlist;
    }
    inline void set_count (vid_t a_count) {
        count = a_count;
    }

    inline void increment_count() {
        ++count;
    }
    inline void add_nebr(sid_t sid) {
        adj_list[count++] = sid;
    }

    //
    inline beg_pos_t() {
        count = 0;
        adj_list = 0;
    }
    inline vid_t get_count() {
        return count;
    } 
    inline sid_t* get_adjlist() {
        return adj_list;
    }
    friend class rset_t;
    //friend class sgraph_t;
};

//one type's graph
class sgraph_t {
public:
    //type id and count together
    sid_t      super_id;

    //array of adj list of vertices
    beg_pos_t* beg_pos;

public:
    //used during initial setup only
    inline void increment_count(vid_t vid) {
       beg_pos[vid].increment_count(); 
    }
    inline void add_nebr(vid_t vid, sid_t sid) {
        beg_pos[vid].add_nebr(sid);
    }
};

//one type's key-value store
class skv_t {
 public:
    sid_t super_id;
    sid_t* kv;
};


#define eStatusarray 0
#define eFrontiers  1
#define eAdjlist    2
#define eKV         3


class srset_t;

//one type's result set
class rset_t {
    private:
    //few MSB bits = type
    //rest bits = words count in status array, maximum count in other cases
    sid_t scount;

    //few MSB bits: identify union
    //rest bits = frontier count  XXX not correct
    sid_t count2;

    //Notes: in td transform, vlist is used (input), status_array is produced;
    // in bu transform, status array is used in both (input and output)
    //In extend, input is vlist, output is kv or adjlist
    union {
        uint64_t* status_array;
        vid_t*    vlist;
        beg_pos_t* adjlist;
        sid_t*    kv;
    };


    public:
    friend class srset_t;
    inline rset_t() {
        scount = 0;
        count2 = 0;
        status_array = 0;
    }
    inline vid_t* get_vlist() { return vlist;}
    inline beg_pos_t* get_graph() { return adjlist;}
    inline uint64_t* get_barray() {return status_array;} 
    inline sid_t* get_kv() { return kv;};
    inline vid_t get_vcount() {return TO_VID(count2);}
    inline int   get_uniontype() {return TO_TID(count2);}
    inline tid_t get_tid() {return TO_TID(scount);}
    inline vid_t get_wcount() {return TO_VID(scount);}
    
    inline vid_t set_status(vid_t vid) {
        if(!get_status(vid)) {
            status_array[word_offset(vid)] |= ((uint64_t) 1L << bit_offset(vid));
            ++count2;
            return 1L;
        }
        return 0L;
    }
	inline void add_frontier(sid_t sid) {
		vid_t index = TO_VID(count2);
		vlist[index] = sid;
		++count2;
	}
	
    inline void setup_frontiers(tid_t tid, vid_t max_count) {
		scount = TO_SUPER(tid) + max_count;
		count2 = TO_SUPER(1) + 0;
		vlist = (vid_t*)calloc(sizeof(sid_t), max_count);
	}
	
    inline void add_adjlist_ro(vid_t index, beg_pos_t* begpos) {
		adjlist[index].count = begpos->count;
		adjlist[index].adj_list = begpos->adj_list;
	}
    
    inline void add_kv(vid_t index, sid_t sid) {
		kv[index] = sid;
	}
    
   
    private:
    inline vid_t get_status(vid_t vid) {
        return status_array[word_offset(vid)] & ((uint64_t) 1L << bit_offset(vid));
    }
    void copy_setup(rset_t* iset, int union_type); 
    void print_result(select_info_t* select_info, qid_t select_count, vid_t pos);
    void print_adjlist(select_info_t* select_info, qid_t select_count, vid_t pos);
    void print_kv(select_info_t* select_info, qid_t select_count, vid_t pos);
    void print_barray(select_info_t* select_info, qid_t select_count);
    void print_vlist(select_info_t* select_info, qid_t select_count);
    
    void bitwise2vlist();
    
    inline void setup(sid_t super_id) {
        tid_t tid = TO_TID(super_id);
        vid_t w_count = WORD_COUNT(TO_VID(super_id));
        scount  = TO_SUPER(tid) + w_count;
        count2 = TO_SUPER(eStatusarray);
        status_array = (uint64_t*) calloc(sizeof(uint64_t*), w_count);
    }

        
};

class srset_t {
 public:
    //array of result sets
    rset_t*  rset; 
    
    filter_info_t* filter_info;
    
    //type filters
    tid_t*  tfilter;
    
    select_info_t* select_info;
    
    uint8_t filter_count;
    uint8_t filter_done;
    uint8_t tfilter_count;
    uint8_t select_count;
    
 private:
    sflag_t  flag;
    //Total result set count and total frontiers count
    //few MSB bits = index into rset
    //other bits = total frontier count. XXX not correct
    uint64_t ccount;

 public:
    inline srset_t() {
        flag = 0;
        ccount = 0;
        rset = 0;
        filter_info = 0;
        filter_done = 1;
        tfilter = 0;
        tfilter_count = 0;
    }
    void setup_select(qid_t a_count); 
    void create_select(qid_t index, const char* a_name, const char* prop_name);
    
    inline void set_filter(filter_info_t* info) {
        filter_info = info;
        filter_done = 0;
    } 

    inline void setup_tfilter(tid_t count) {
        tfilter_count = count;
        tfilter = new tid_t[count];
    }
    inline void create_tfilter(tid_t index, tid_t tid) {
        assert(index < tfilter_count);
        tfilter[index] = tid;
    }
    
	inline tid_t get_sindex(sid_t sid) {
		tid_t type_id = TO_TID(sid) + 1;
		sflag_t flag_mask = flag & ((1L << type_id) - 1);
		tid_t index = __builtin_popcountll(flag_mask) - 1;
		return index;
	}

    inline vid_t get_status(sid_t sid) {
		tid_t index = get_sindex(sid);
		vid_t vert_id = TO_VID(sid);
        return rset[index].get_status(vert_id);
    } 
    
    inline void add_frontier(sid_t sid) {
		tid_t index = get_sindex(sid);
		vid_t vert_id = TO_VID(sid);
        rset[index].add_frontier(vert_id);
    }
    
    inline vid_t set_status(sid_t sid) {
        if (tfilter_count > 0 && eOK != apply_typefilter(TO_TID(sid))) {
            return 0L;
        }
        tid_t index = get_sindex(sid);
        vid_t vert_id = TO_VID(sid);
        return rset[index].set_status(vert_id);
    }

    inline tid_t get_rset_count() {return TO_TID(ccount);}
    inline tid_t get_total_vcount() {return TO_VID(ccount);}
    
    void bitwise2vlist();
    void print_result(tid_t tid_pos, vid_t vid_pos);
    status_t apply_typefilter(tid_t tid);
  
    tid_t full_setup(sflag_t sflag);
    tid_t copy_setup(srset_t* iset, int union_type);
    
    
    //Dont call directly
    tid_t setup(sflag_t sflag); 

};


/////// Function//////////////
inline tid_t get_sindex(tid_t tid, sflag_t sflag)
{
    sflag_t flag_mask = sflag & ((1L << (tid +1)) - 1);
    tid_t        pos = __builtin_popcountll(flag_mask) - 1;
    return pos;
}

inline tid_t get_sindex(sid_t sid, sflag_t flag)
{
	tid_t type_id = TO_TID(sid) + 1;
	sflag_t flag_mask = flag & ((1L << type_id) - 1);
	tid_t index = __builtin_popcountll(flag_mask) - 1;
	return index;
}

