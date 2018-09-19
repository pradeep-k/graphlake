#pragma once

#include <omp.h>
#include <sys/mman.h>
#include <asm/mman.h>
#include <unistd.h>
#include <fcntl.h>


#include "type.h"
#include "graph.h"
#include "wtime.h"

//for each range
template <class T>
class global_range_t {
  public:
      index_t count;
      edgeT_t<T>* edges;
};

//for each thread 
class thd_local_t {
  public:
      //For each thread
      vid_t* vid_range;
      vid_t  range_end;
};

template <class T>
class pgraph_t: public cfinfo_t {
    public:
        union {
            onekv_t<T>** skv_out;
            onegraph_t<T>** sgraph_out;
            onegraph_t<T>** sgraph; 
        };
        union {
            onekv_t<T>** skv_in;
            onegraph_t<T>** sgraph_in;
        };
        
        //intermediate classification buffer
        edgeT_t<T>* edge_buf_out;
        edgeT_t<T>* edge_buf_in;
        index_t edge_buf_count;

        //circular edge log buffer
        blog_t<T>*  blog;

 public:    
    inline pgraph_t() { 
        sgraph = 0;
        sgraph_in = 0;
        
        edge_buf_out = 0;
        edge_buf_in = 0;
        edge_buf_count = 0;
        
        blog = new blog_t<T>;
    
    }

    inline void alloc_edgelog(index_t count) {
        if (blog->blog_beg) {
            free(blog->blog_beg);
            blog->blog_beg = 0;
        }

        blog->blog_count = count;
        blog->blog_mask = count - 1;
        //blog->blog_beg = (edgeT_t<T>*)mmap(0, sizeof(edgeT_t<T>)*blog->blog_count, 
        //PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB, 0, 0);
        //if (MAP_FAILED == blog->blog_beg) {
        //    cout << "Huge page alloc failed for edge log" << endl;
        //}
        if (posix_memalign((void**)&blog->blog_beg, 2097152, 
                           blog->blog_count*sizeof(edgeT_t<T>))) {
            perror("posix memalign batch edge log");
        }
    }
    
    inline void alloc_edge_buf(index_t total) {
        index_t total_edge_count = 0;
        if (0 == sgraph_in) {
            total_edge_count = (total << 1);
            if (0 == edge_buf_count) {
                edge_buf_out = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_count = total_edge_count;
            } else if (edge_buf_count < total_edge_count) {
                free(edge_buf_out);
                edge_buf_out = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_count = total_edge_count;
            }
        } else {
            total_edge_count = total;
            if (0 == edge_buf_count) {
                edge_buf_out = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_in = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_count = total_edge_count;
            } else if (edge_buf_count < total_edge_count) {
                free(edge_buf_out);
                free(edge_buf_in);
                edge_buf_out = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_in = (edgeT_t<T>*)malloc(total_edge_count*sizeof(edgeT_t<T>));
                edge_buf_count = total_edge_count;
            }
        }
    }

    inline void free_edge_buf() {
        if (edge_buf_out) {    
            free(edge_buf_out);
            edge_buf_out = 0;
        }
        if (edge_buf_in) {
            free(edge_buf_in);
            edge_buf_in = 0;
        }
        edge_buf_count = 0;
    }

    status_t batch_update(const string& src, const string& dst, propid_t pid = 0) {
        return eOK;
    }
    status_t batch_edge(edgeT_t<T> edge) {
        status_t ret = eOK;

        index_t index = __sync_fetch_and_add(&blog->blog_head, 1L);
        
        //Check if we are overwritting the unarchived data, if so sleep
        while (index - blog->blog_tail >= blog->blog_count) {
            usleep(10);
        }
        
        index_t size = ((index - blog->blog_marker) & BATCH_MASK);
        index_t index1 = (index & blog->blog_mask);
        
        //inform archive thread about threshold being crossed
        if ((0 == size) && (index != blog->blog_marker)) {
            blog->blog_beg[index1] = edge;
            create_marker(index + 1);
            //cout << "Will create a snapshot now " << endl;
            return eEndBatch;
            //ret = eEndBatch;
        } 
        
        /*
        else if ((index - blog->blog_tail) == blog->blog_count - 1000) {
            blog->blog_beg[index1] = edge;
            create_marker(index + 1);
            cout << "About OOM" << endl;
            return eOOM;
        } else if ((index - blog->blog_tail) >= blog->blog_count) {
            //block
            assert(0);
            return eOOM;
        }*/

        blog->blog_beg[index1] = edge;
        
        //if (ret != eEndBatch) {
        //    blog->blog_beg[index1] = edge;
        //}
            
        //----
        //Make the edge log durable
        //if ((index != blog->blog_wmarker) && 
        //    ((index - blog->blog_wmarker) % W_SIZE) == 0) {
        //    create_wmarker(index);
        //}
    
        return ret; 
    }
    
    //called from w thread 
    status_t write_edgelog() {
        index_t w_marker = blog->blog_head;
        index_t w_tail = blog->blog_wtail;
        index_t w_count = w_marker - w_tail;
        if (w_count == 0) return eNoWork;

        index_t actual_tail = w_tail & blog->blog_mask;
        index_t actual_marker = w_marker & blog->blog_mask;
        
        if (actual_tail < actual_marker) {
            //write and update tail
            //fwrite(blog->blog_beg + w_tail, sizeof(edgeT_t<T>), w_count, wtf);
            write(wtf, blog->blog_beg + actual_tail, sizeof(edgeT_t<T>)*w_count);
        }
        else {
            write(wtf, blog->blog_beg + actual_tail, sizeof(edgeT_t<T>)*(blog->blog_count - actual_tail));
            write(wtf, blog->blog_beg, sizeof(edgeT_t<T>)*actual_marker);
        }
        blog->blog_wtail = w_marker;
        //fsync();
        return eOK;
    }
    
    //Called from front end thread
   // void create_wmarker(index_t marker) {
   //     pthread_mutex_lock(&g->w_mutex);
   //     if (marker > blog->blog_wmarker) {
   //         blog->blog_wmarker = marker;
   //     }
   //     pthread_cond_signal(&g->w_condition);
   //     pthread_mutex_unlock(&g->w_mutex);
   //     cout << "WMarker queued." << endl;
   // }
    
   // status_t write_edgelog() {
   //     index_t w_marker = 0;
   //     index_t w_tail = 0;
   //     index_t w_count = 0;
   //     pthread_mutex_lock(&g->w_mutex);
   //     w_marker = blog->blog_wmarker;
   //     pthread_mutex_unlock(&g->w_mutex);
   //     w_tail = blog->blog_wtail;
   //     w_count = w_marker - w_tail;
   //     if (w_count) {
   //         //write and update tail
   //         //fwrite(blog->blog_beg + w_tail, sizeof(edgeT_t<T>), w_count, wtf);
   //         write(wtf, blog->blog_beg + w_tail, sizeof(edgeT_t<T>)*w_count);
   //         blog->blog_wtail = w_marker;
   //         return eOK;
   //     }
   //     return eNoWork;
   // }
    
    void create_marker(index_t marker) {
        pthread_mutex_lock(&g->snap_mutex);
        index_t m_index = __sync_fetch_and_add(&q_head, 1L);
        q_beg[m_index % q_count] = marker;
        pthread_cond_signal(&g->snap_condition);
        pthread_mutex_unlock(&g->snap_mutex);
        //cout << "Marker queued. position = " << m_index % q_count << " " << marker << endl;
    } 
    
    //called from snap thread 
    status_t move_marker(index_t& snap_marker) {
        pthread_mutex_lock(&g->snap_mutex);
        index_t head = q_head;
        //Need to read marker and set the blog_marker;
        if (q_tail == head) {
            pthread_mutex_unlock(&g->snap_mutex);
            //cout << "Marker NO dequeue. Position = " << head <<  endl;
            return eNoWork;
        }
        
        index_t m_index = head - 1;
        index_t marker = q_beg[m_index % q_count];
        q_tail = head;
        blog->blog_marker = marker;
        snap_marker = blog->blog_marker;
        
        /*
        index_t m_index = __sync_fetch_and_add(&q_tail, 1L);
        index_t marker = q_beg[m_index % q_count];
        blog->blog_marker = marker;
        snap_marker = blog->blog_marker;
        */

        pthread_mutex_unlock(&g->snap_mutex);
        //cout << "working on snapshot" << endl;
        //cout << "Marker dequeue. Position = " << m_index % q_count << " " << marker << endl;
        return eOK;
    }

    index_t update_marker() { 
        blog->blog_tail = blog->blog_marker;
        return blog->blog_tail;
    }

 public:
    onegraph_t<T>** prep_sgraph(sflag_t ori_flag, onegraph_t<T>** a_sgraph);
    onekv_t<T>**    prep_skv(sflag_t ori_flag, onekv_t<T>** a_skv);
    void            prep_sgraph_internal(onegraph_t<T>** sgraph);
    
    void make_graph_d(); 
    void make_graph_u();

    void estimate_classify (vid_t* vid_range, vid_t* vid_range_in, vid_t bit_shift);
    void prefix_sum (global_range_t<T>* global_range, thd_local_t* thd_local,
                    vid_t range_count, vid_t thd_count, edgeT_t<T>* edge_buf);
    void work_division (global_range_t<T>* global_range, thd_local_t* thd_local,
                    vid_t range_count, vid_t thd_count, index_t equal_work);
    void classify (vid_t* vid_range, vid_t* vid_range_in, vid_t bit_shift, 
            global_range_t<T>* global_range, global_range_t<T>* global_range_in);
    void calc_degree_noatomic (onegraph_t<T>** sgraph, global_range_t<T>* global_range, 
                      vid_t j_start, vid_t j_end);
    virtual void fill_adjlist_noatomic (onegraph_t<T>** sgraph, global_range_t<T>* global_range, 
                      vid_t j_start, vid_t j_end);
    
    void store_sgraph(onegraph_t<T>** sgraph, bool clean = false);
    void store_skv(onekv_t<T>** skv);
    
    void read_sgraph(onegraph_t<T>** sgraph);
    void read_skv(onekv_t<T>** skv);
    
    void file_open_sgraph(onegraph_t<T>** sgraph, const string& odir, const string& postfix, bool trunc);
    void file_open_skv(onekv_t<T>** skv, const string& odir, const string& postfix, bool trunc);
   
    void update_count(onegraph_t<T>** sgraph);
    void calc_edge_count(onegraph_t<T>** sgraph_out, onegraph_t<T>** sgraph_in); 
    void calc_edge_count_out(onegraph_t<T>** p_sgraph_out);
    void calc_edge_count_in(onegraph_t<T>** sgraph_in);
    void fill_adj_list(onegraph_t<T>** sgraph_out, onegraph_t<T>** sgraph_in);
    void fill_adj_list_in(onekv_t<T>** skv_out, onegraph_t<T>** sgraph_in); 
    void fill_adj_list_out(onegraph_t<T>** sgraph_out, onekv_t<T>** skv_in); 
    void fill_skv(onekv_t<T>** skv_out, onekv_t<T>** skv_in);
    
    status_t query_adjlist_td(onegraph_t<T>** sgraph, srset_t* iset, srset_t* oset);
    status_t query_kv_td(onekv_t<T>** skv, srset_t* iset, srset_t* oset);
    status_t query_adjlist_bu(onegraph_t<T>** sgraph, srset_t* iset, srset_t* oset);
    status_t query_kv_bu(onekv_t<T>** skv, srset_t* iset, srset_t* oset);
    
    status_t extend_adjlist_td(onegraph_t<T>** skv, srset_t* iset, srset_t* oset);
    status_t extend_kv_td(onekv_t<T>** skv, srset_t* iset, srset_t* oset);

};
template <class T>
class ugraph: public pgraph_t<T> {
 public:
    using pgraph_t<T>::sgraph;
    using pgraph_t<T>::flag1;
    using pgraph_t<T>::flag2;
    using pgraph_t<T>::flag1_count;
    using pgraph_t<T>::flag2_count;
    using pgraph_t<T>::blog;

    using pgraph_t<T>::read_sgraph;
    using pgraph_t<T>::prep_sgraph;
    using pgraph_t<T>::file_open_sgraph;
    using pgraph_t<T>::calc_edge_count;
    using pgraph_t<T>::prep_sgraph_internal;
    using pgraph_t<T>::fill_adj_list;
    using pgraph_t<T>::store_sgraph;
    using pgraph_t<T>::extend_adjlist_td;
    using pgraph_t<T>::query_adjlist_td;
    using pgraph_t<T>::query_adjlist_bu;


 public:
    static cfinfo_t* create_instance();
    
    void incr_count(sid_t src, sid_t dst, int del = 0);
    void add_nebr(sid_t src, sid_t dst, int del = 0);
    void prep_graph_baseline();
    void calc_degree();
    void make_graph_baseline();
    void create_snapshot();
    void store_graph_baseline(bool clean = false);
    void read_graph_baseline();
    void file_open(const string& odir,  bool trunc);
    
    //status_t transform(srset_t* iset, srset_t* oset, direction_t direction);
    //virtual status_t extend(srset_t* iset, srset_t* oset, direction_t direction);
};

template <class T>
class dgraph: public pgraph_t<T> {
 public:
    using pgraph_t<T>::sgraph_in;
    using pgraph_t<T>::sgraph_out;
    using pgraph_t<T>::flag1;
    using pgraph_t<T>::flag2;
    using pgraph_t<T>::flag1_count;
    using pgraph_t<T>::flag2_count;
    using pgraph_t<T>::blog;
    
    using pgraph_t<T>::prep_sgraph;
    using pgraph_t<T>::read_sgraph;
    using pgraph_t<T>::file_open_sgraph;
    using pgraph_t<T>::calc_edge_count;
    using pgraph_t<T>::prep_sgraph_internal;
    using pgraph_t<T>::fill_adj_list;
    using pgraph_t<T>::store_sgraph;
    using pgraph_t<T>::extend_adjlist_td;
    using pgraph_t<T>::query_adjlist_td;
    using pgraph_t<T>::query_adjlist_bu;

 public:
    static cfinfo_t* create_instance();
    
    void incr_count(sid_t src, sid_t dst, int del = 0);
    void add_nebr(sid_t src, sid_t dst, int del = 0);
    void prep_graph_baseline();
    void calc_degree();
    void make_graph_baseline();
    void create_snapshot();
    void store_graph_baseline(bool clean = false);
    void read_graph_baseline();
    void file_open(const string& odir,  bool trunc);
    
    //status_t transform(srset_t* iset, srset_t* oset, direction_t direction);
    //virtual status_t extend(srset_t* iset, srset_t* oset, direction_t direction);
};

template <class T>
class many2one: public pgraph_t<T> {
 public:
    using pgraph_t<T>::sgraph_in;
    using pgraph_t<T>::skv_out;
    using pgraph_t<T>::flag1;
    using pgraph_t<T>::flag2;
    using pgraph_t<T>::flag1_count;
    using pgraph_t<T>::flag2_count;
    using pgraph_t<T>::blog;
    
    using pgraph_t<T>::read_sgraph;
    using pgraph_t<T>::read_skv;
    using pgraph_t<T>::prep_sgraph;
    using pgraph_t<T>::prep_skv;
    using pgraph_t<T>::file_open_sgraph;
    using pgraph_t<T>::file_open_skv;
    using pgraph_t<T>::calc_edge_count_in;
    using pgraph_t<T>::prep_sgraph_internal;
    using pgraph_t<T>::fill_adj_list_in;
    using pgraph_t<T>::store_sgraph;
    using pgraph_t<T>::store_skv;
    using pgraph_t<T>::extend_adjlist_td;
    using pgraph_t<T>::extend_kv_td;
    using pgraph_t<T>::query_adjlist_td;
    using pgraph_t<T>::query_adjlist_bu;
    using pgraph_t<T>::query_kv_td;
    using pgraph_t<T>::query_kv_bu;

 public:
    static cfinfo_t* create_instance();
    
    void incr_count(sid_t src, sid_t dst, int del = 0);
    void add_nebr(sid_t src, sid_t dst, int del = 0);
    void prep_graph_baseline();
    void calc_degree();
    void make_graph_baseline();
    void create_snapshot();
    void store_graph_baseline(bool clean = false);
    void read_graph_baseline();
    void file_open(const string& odir,  bool trunc);
    
    //status_t transform(srset_t* iset, srset_t* oset, direction_t direction);
    //virtual status_t extend(srset_t* iset, srset_t* oset, direction_t direction);
};

template <class T>
class one2one: public pgraph_t<T> {
 public:
    using pgraph_t<T>::skv_in;
    using pgraph_t<T>::skv_out;
    using pgraph_t<T>::flag1;
    using pgraph_t<T>::flag2;
    using pgraph_t<T>::flag1_count;
    using pgraph_t<T>::flag2_count;
    using pgraph_t<T>::blog;
    
    using pgraph_t<T>::read_skv;
    using pgraph_t<T>::prep_skv;
    using pgraph_t<T>::file_open_skv;
    using pgraph_t<T>::store_skv;
    using pgraph_t<T>::fill_skv;
    using pgraph_t<T>::extend_kv_td;
    using pgraph_t<T>::query_kv_td;
    using pgraph_t<T>::query_kv_bu;

 public:
    static cfinfo_t* create_instance();
    
    void incr_count(sid_t src, sid_t dst, int del = 0);
    void add_nebr(sid_t src, sid_t dst, int del = 0);
    void prep_graph_baseline();
    void calc_degree();
    void make_graph_baseline();
    void create_snapshot();
    void store_graph_baseline(bool clean = false);
    void read_graph_baseline();
    void file_open(const string& odir,  bool trunc);
    
    //status_t transform(srset_t* iset, srset_t* oset, direction_t direction);
    //virtual status_t extend(srset_t* iset, srset_t* oset, direction_t direction);
};

template <class T>
class one2many: public pgraph_t<T> {
 public:
    using pgraph_t<T>::skv_in;
    using pgraph_t<T>::sgraph_out;
    using pgraph_t<T>::flag1;
    using pgraph_t<T>::flag2;
    using pgraph_t<T>::flag1_count;
    using pgraph_t<T>::flag2_count;
    using pgraph_t<T>::blog;
    
    using pgraph_t<T>::read_sgraph;
    using pgraph_t<T>::read_skv;
    using pgraph_t<T>::prep_sgraph;
    using pgraph_t<T>::prep_skv;
    using pgraph_t<T>::file_open_sgraph;
    using pgraph_t<T>::file_open_skv;
    using pgraph_t<T>::calc_edge_count_out;
    using pgraph_t<T>::prep_sgraph_internal;
    using pgraph_t<T>::fill_adj_list_out;
    using pgraph_t<T>::store_sgraph;
    using pgraph_t<T>::store_skv;
    using pgraph_t<T>::extend_adjlist_td;
    using pgraph_t<T>::extend_kv_td;
    using pgraph_t<T>::query_adjlist_td;
    using pgraph_t<T>::query_adjlist_bu;
    using pgraph_t<T>::query_kv_td;
    using pgraph_t<T>::query_kv_bu;

 public:
    static cfinfo_t* create_instance();
    
    void incr_count(sid_t src, sid_t dst, int del = 0);
    void add_nebr(sid_t src, sid_t dst, int del = 0);
    void prep_graph_baseline();
    void calc_degree();
    void make_graph_baseline();
    void create_snapshot();
    void store_graph_baseline(bool clean = false);
    void read_graph_baseline();
    void file_open(const string& odir,  bool trunc);
    
    //status_t transform(srset_t* iset, srset_t* oset, direction_t direction);
    //virtual status_t extend(srset_t* iset, srset_t* oset, direction_t direction);
};


typedef ugraph<sid_t> ugraph_t;
typedef dgraph<sid_t> dgraph_t;
typedef one2many<sid_t> one2many_t;
typedef many2one<sid_t> many2one_t;
typedef one2one<sid_t> one2one_t;

typedef ugraph<lite_edge_t> p_ugraph_t;
typedef dgraph<lite_edge_t> p_dgraph_t;
typedef one2many<lite_edge_t> p_one2many_t;
typedef many2one<lite_edge_t> p_many2one_t;
typedef one2one<lite_edge_t> p_one2one_t;

/*****************************/
#include "onegraph.h"

template <class T>
onegraph_t<T>** pgraph_t<T>::prep_sgraph(sflag_t ori_flag, onegraph_t<T>** sgraph)
{
    tid_t   pos = 0;//it is tid
    
    sflag_t      flag = ori_flag;
    tid_t  flag_count = __builtin_popcountll(flag);
    
    for(tid_t i = 0; i < flag_count; i++) {
        pos = __builtin_ctzll(flag);
        flag ^= (1L << pos);//reset that position
        if (0 == sgraph[pos]) {
            sgraph[pos] = new onegraph_t<T>;
        }
        sgraph[pos]->setup(pos);
    }
    return sgraph;
}

//prefix sum, allocate adj list memory then reset the count
template <class T>
void pgraph_t<T>::prep_sgraph_internal(onegraph_t<T>** sgraph)
{
    tid_t  t_count   = g->get_total_types();
    
    vid_t  v_count   = 0;
    vid_t  portion   = 0;
    vid_t  vid_start = 0;
    vid_t  vid_end   = 0;
    vid_t  total_thds  = omp_get_num_threads();
    vid_t         tid  = omp_get_thread_num();  
    
    for(tid_t i = 0; i < t_count; i++) {
        if (0 == sgraph[i]) continue;
        //sgraph[i]->setup_adjlist();
        
        v_count = sgraph[i]->get_vcount();
        portion = v_count/total_thds;
        vid_start = portion*tid;
        vid_end   = portion*(tid + 1);
        if (tid == total_thds - 1) {
            vid_end = v_count;
        }

        sgraph[i]->setup_adjlist_noatomic(vid_start, vid_end);
        #pragma omp barrier
    }
}

template <class T>
void pgraph_t<T>::store_sgraph(onegraph_t<T>** sgraph, bool clean /*= false*/)
{
    if (sgraph == 0) return;
    
    tid_t    t_count = g->get_total_types();
    
    // For each file.
    for (tid_t i = 0; i < t_count; ++i) {
        if (sgraph[i] == 0) continue;
		sgraph[i]->handle_write(clean);
        /*
		sgraph[i]->persist_elog(etfile);
        sgraph[i]->persist_slog(stfile);
        sgraph[i]->persist_vlog(vtfile);
		*/
    }
}

template <class T>
void pgraph_t<T>::file_open_sgraph(onegraph_t<T>** sgraph, const string& dir, const string& postfix, bool trunc)
{
    if (sgraph == 0) return;
    
    char name[8];
    string  basefile = dir + col_info[0]->p_name;
    string  filename;
    string  wtfile; 

    // For each file.
    tid_t    t_count = g->get_total_types();
    for (tid_t i = 0; i < t_count; ++i) {
        if (0 == sgraph[i]) continue;

        sprintf(name, "%d", i);
        filename = basefile + name + postfix ; 
        sgraph[i]->file_open(filename, trunc);
        
        wtfile = filename + ".elog";
		if (trunc) {
            wtf = open(wtfile.c_str(), O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
        } else {
		    wtf = open(wtfile.c_str(), O_RDWR|O_CREAT, S_IRWXU);
        }
    }
}

template <class T>
void pgraph_t<T>::read_sgraph(onegraph_t<T>** sgraph)
{
    if (sgraph == 0) return;
    
    tid_t    t_count = g->get_total_types();
    
    // For each file.
    for (tid_t i = 0; i < t_count; ++i) {
        if (sgraph[i] == 0) continue;
        sgraph[i]->read_vtable();
        //sgraph[i]->read_stable(stfile);
        //sgraph[i]->read_etable(etfile);
    }
}

//estimate edge count
template <class T>
void pgraph_t<T>::calc_edge_count(onegraph_t<T>** sgraph_out, onegraph_t<T>** sgraph_in) 
{
    sid_t     src, dst;
    vid_t     vert1_id, vert2_id;
    tid_t     src_index, dst_index;
    edgeT_t<T>* edges = blog->blog_beg;
    index_t index = 0;
   
    #pragma omp for
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = get_sid(edges[index].dst_id);
        
        src_index = TO_TID(src);
        dst_index = TO_TID(dst);
        vert1_id = TO_VID(src);
        vert2_id = TO_VID(dst);

        if (!IS_DEL(src)) { 
            sgraph_out[src_index]->increment_count(vert1_id);
            sgraph_in[dst_index]->increment_count(vert2_id);
        } else { 
            sgraph_out[src_index]->decrement_count(vert1_id);
            sgraph_in[dst_index]->decrement_count(vert2_id);
        }
    }
}

//estimate edge count
template <class T>
void pgraph_t<T>::calc_edge_count_out(onegraph_t<T>** sgraph_out)
{
    sid_t     src;
    vid_t     vert1_id;
    tid_t     src_index;
    edgeT_t<T>* edges = blog->blog_beg;
    
    index_t index = 0;
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        src_index = TO_TID(src);
        vert1_id = TO_VID(src);
        if (!IS_DEL(src)) {
            sgraph_out[src_index]->increment_count(vert1_id);
        } else {
            sgraph_out[src_index]->decrement_count(vert1_id);
        }
    }
}
//estimate edge count
template <class T>
void pgraph_t<T>::calc_edge_count_in(onegraph_t<T>** sgraph_in)
{
    sid_t     src, dst;
    vid_t     vert2_id;
    tid_t     dst_index;
    edgeT_t<T>* edges = blog->blog_beg;
    
    /*
    edgeT_t<T>*  edges;
    index_t   count;
    for (int j = 0; j <= batch_count; ++j) { 
        edges = (edgeT_t<T>*)batch_info[j].buf;
        count = batch_info[j].count;
    }*/
    
    index_t index = 0;
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = get_sid(edges[index].dst_id);
        dst_index = TO_TID(dst);
        vert2_id = TO_VID(dst);
        if (!IS_DEL(src)) {
            sgraph_in[dst_index]->increment_count(vert2_id);
        } else {
            sgraph_in[dst_index]->decrement_count(vert2_id);
        }
    }
}

template <class T>
void pgraph_t<T>::fill_adj_list(onegraph_t<T>** sgraph_out, onegraph_t<T>** sgraph_in)
{
    sid_t     src, dst2; 
    T         src2, dst;
    vid_t     vert1_id, vert2_id;
    tid_t     src_index, dst_index;
    
    edgeT_t<T>*   edges = blog->blog_beg;

    index_t index = 0;
    #pragma omp for
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = edges[index].dst_id;
        
        src_index = TO_TID(src);
        vert1_id = TO_VID(src);
        sgraph_out[src_index]->add_nebr(vert1_id, dst);
        
        dst2 = get_sid(dst);
        set_sid(src2, src);
        set_weight(src2, dst);

        dst_index = TO_TID(dst2);
        vert2_id = TO_VID(dst2);
        sgraph_in[dst_index]->add_nebr(vert2_id, src2);
    }
}

template <class T>
void pgraph_t<T>::fill_adj_list_in(onekv_t<T>** skv_out, onegraph_t<T>** sgraph_in) 
{
    sid_t src, dst2;
    T     src2, dst;
    vid_t     vert1_id, vert2_id;
    tid_t     src_index, dst_index;
    edgeT_t<T>*   edges = blog->blog_beg;
    
    index_t index = 0;
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = edges[index].dst_id;
        
        src_index = TO_TID(src);
        vert1_id = TO_VID(src);
        skv_out[src_index]->set_value(vert1_id, dst);
        
        dst2 = get_sid(dst);
        set_sid(src2, src);
        set_weight(src2, dst);
        
        dst_index = TO_TID(dst2);
        vert2_id = TO_VID(dst2);
        sgraph_in[dst_index]->add_nebr(vert2_id, src2);
    }
}

template <class T>
void pgraph_t<T>::fill_adj_list_out(onegraph_t<T>** sgraph_out, onekv_t<T>** skv_in) 
{
    sid_t   src, dst2;
    T       src2, dst;
    vid_t   vert1_id, vert2_id;
    tid_t   src_index, dst_index; 
    edgeT_t<T>*   edges = blog->blog_beg;
    
    index_t index = 0;
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = edges[index].dst_id;
        
        src_index = TO_TID(src);
        vert1_id = TO_VID(src);
        sgraph_out[src_index]->add_nebr(vert1_id, dst);
        
        dst2 = get_sid(dst);
        set_sid(src2, src);
        set_weight(src2, dst);
        
        dst_index = TO_TID(dst2);
        vert2_id = TO_VID(dst2);
        skv_in[dst_index]->set_value(vert2_id, src2); 
    }
}

template <class T>
void pgraph_t<T>::fill_skv(onekv_t<T>** skv_out, onekv_t<T>** skv_in)
{
    sid_t src, dst2;
    T     src2, dst;
    vid_t     vert1_id, vert2_id;
    tid_t     src_index, dst_index;
    edge_t*   edges = blog->blog_beg;
    
    index_t index = 0;
    for (index_t i = blog->blog_tail; i < blog->blog_marker; ++i) {
        index = (i & blog->blog_mask);
        src = edges[index].src_id;
        dst = edges[index].dst_id;
        
        src_index = TO_TID(src);
        vert1_id = TO_VID(src);
        skv_out[src_index]->set_value(vert1_id, dst); 
        
        dst2 = get_sid(dst);
        set_sid(src2, src);
        set_weight(src2, dst);
        
        dst_index = TO_TID(dst2);
        vert2_id = TO_VID(dst2);
        skv_in[dst_index]->set_value(vert2_id, src2); 
    }
}


/******************** super kv *************************/
template <class T>
void pgraph_t<T>::read_skv(onekv_t<T>** skv)
{
    if (skv == 0) return;

    tid_t       t_count = g->get_total_types();
    
    // For each file.
    for (tid_t i = 0; i < t_count; ++i) {
        if (skv[i] == 0) continue;
        skv[i]->read_kv();
    }
}

template <class T>
void pgraph_t<T>::file_open_skv(onekv_t<T>** skv, const string& dir, const string& postfix, bool trunc)
{
    if (skv == 0) return;

    char name[8];
    tid_t       t_count = g->get_total_types();
    
    //base name using relationship type
    string basefile = dir + col_info[0]->p_name;
    string vtfile;

    // For each file.
    for (tid_t i = 0; i < t_count; ++i) {
        if (skv[i] == 0) continue;
        sprintf(name, "%d", i);
        vtfile = basefile + name + postfix;
        skv[i] = new onekv_t<T>;
        skv[i]->setup(i);

        skv[i]->file_open(vtfile, trunc);
    }
}

template <class T>
void pgraph_t<T>::store_skv(onekv_t<T>** skv)
{
    if (skv == 0) return;

    tid_t       t_count = g->get_total_types();

    // For each file.
    for (tid_t i = 0; i < t_count; ++i) {
        if (skv[i] == 0) continue;

        skv[i]->persist_kvlog();
    }
}

//super bins memory allocation
template <class T>
onekv_t<T>** pgraph_t<T>::prep_skv(sflag_t ori_flag, onekv_t<T>** skv)
{
    tid_t   pos  = 0;
    sflag_t flag       = ori_flag;
    tid_t   flag_count = __builtin_popcountll(flag);

    for(tid_t i = 0; i < flag_count; i++) {
        pos = __builtin_ctz(flag);
        flag ^= (1L << pos);//reset that position
        if (0 == skv[pos]) {
            skv[pos] = new onekv_t<T>;
        }
        skv[pos]->setup(pos);
    }
    return skv;
}

/**********************************************************/

template <class T>
status_t pgraph_t<T>::query_adjlist_td(onegraph_t<T>** sgraph, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        vid_t v_count = rset->get_vcount();
        vid_t* vlist = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == sgraph[tid]) continue;
        vert_table_t<T>* graph = sgraph[tid]->get_begpos();

        
        //Get the frontiers
        vid_t     frontier;
        for (vid_t v = 0; v < v_count; v++) {
            frontier = vlist[v];
            T* adj_list = graph[frontier].get_adjlist();
            vid_t nebr_count = get_nebrcount1(adj_list);
            ++adj_list;
            
            //traverse the adj list
            for (vid_t k = 0; k < nebr_count; ++k) {
                oset->set_status(get_nebr(adj_list, k));
            }
        }
    }
    return eOK;
}

template <class T>
status_t pgraph_t<T>::query_kv_td(onekv_t<T>** skv, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        vid_t v_count = rset->get_vcount();
        vid_t* vlist = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == skv[tid]) continue;
        T* kv = skv[tid]->get_kv(); 

        //Get the frontiers
        vid_t     frontier;
        for (vid_t v = 0; v < v_count; v++) {
            frontier = vlist[v];
            oset->set_status(get_nebr(kv, frontier));
        }
    }
    return eOK;
}

//sgraph_in and oset share the same flag.
template <class T>
status_t pgraph_t<T>::query_adjlist_bu(onegraph_t<T>** sgraph, srset_t* iset, srset_t* oset)
{
    rset_t* rset = 0;
    tid_t   tid  = 0;
    tid_t oset_count = oset->get_rset_count();

    for (tid_t i = 0; i < oset_count; ++i) {
        
        //get the graph where we will traverse
        rset = oset->rset + i;
        tid  = rset->get_tid();
        if (0 == sgraph[tid]) continue; 

        vert_table_t<T>* graph = sgraph[tid]->get_begpos(); 
        vid_t    v_count = sgraph[tid]->get_vcount();
        
        
        for (vid_t v = 0; v < v_count; v++) {
            //traverse the adj list
            T* adj_list = graph[v].get_adjlist();
            vid_t nebr_count = get_nebrcount1(adj_list);
            ++adj_list;
            for (vid_t k = 0; k < nebr_count; ++k) {
                if (iset->get_status(get_nebr(adj_list, k))) {
                    rset->set_status(v);
                    break;
                }
            }
        }
    }
    return eOK;
}

template <class T>
status_t pgraph_t<T>::query_kv_bu(onekv_t<T>** skv, srset_t* iset, srset_t* oset) 
{
    rset_t*  rset = 0;
    tid_t    tid  = 0;
    tid_t    oset_count = oset->get_rset_count();
    for (tid_t i = 0; i < oset_count; ++i) {

        //get the graph where we will traverse
        rset = oset->rset + i;
        tid  = rset->get_tid(); 
        if (0 == skv[tid]) continue;

        T* kv = skv[tid]->get_kv(); 
        sid_t   v_count = skv[tid]->get_vcount();
        
        for (vid_t v = 0; v < v_count; ++v) {
            if (iset->get_status(get_nebr(kv, v))) {
                rset->set_status(v);
                break;
            }
        }
    }
    return eOK;
}

template <class T> 
status_t pgraph_t<T>::extend_adjlist_td(onegraph_t<T>** sgraph, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;
    rset_t*        rset2 = 0;

    iset->bitwise2vlist();
    //prepare the output 1,2;
    oset->copy_setup(iset, eAdjlist);

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        rset2 = oset->rset + i;
        vid_t v_count = rset->get_vcount();
        sid_t* varray = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == sgraph[tid]) continue;
        vert_table_t<T>* graph = sgraph[tid]->get_begpos(); 
        
        for (vid_t v = 0; v < v_count; v++) {
            rset2->add_adjlist_ro(v, graph+varray[v]);
        }
    }
    return eOK;
}

template <class T>
status_t pgraph_t<T>::extend_kv_td(onekv_t<T>** skv, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;
    rset_t*       rset2 = 0;

    iset->bitwise2vlist();
    //prepare the output 1,2;
    oset->copy_setup(iset, eKV);

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        rset2 = oset->rset + i;
        vid_t v_count = rset->get_vcount();
        sid_t* varray = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t     tid = rset->get_tid();
        if (0 == skv[tid]) continue;
        T*  graph = skv[tid]->get_kv(); 
        
        for (vid_t v = 0; v < v_count; v++) {
            rset2->add_kv(v, get_nebr(graph, varray[v]));
        }
    }
    return eOK;
}

/************* Semantic graphs  *****************/
template <class T> 
void dgraph<T>::prep_graph_baseline()
{
    this->alloc_edgelog(1 << BLOG_SHIFT);
    flag1_count = __builtin_popcountll(flag1);
    flag2_count = __builtin_popcountll(flag2);

    //super bins memory allocation
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph_out) {
        sgraph_out  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    prep_sgraph(flag1, sgraph_out);    
    
    if (0 == sgraph_in) {
        sgraph_in  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    prep_sgraph(flag2, sgraph_in);
}

template <class T> 
void dgraph<T>::calc_degree()
{
    //estimate edge count
    calc_edge_count(sgraph_out, sgraph_in);
}

//We assume that no new vertex type is defined
template <class T> 
void dgraph<T>::make_graph_baseline()
{
    this->make_graph_d();
    
    /*
    if (blog->blog_tail >= blog->blog_marker) return;
    double start, end;
    start = mywtime(); 
    #pragma omp parallel num_threads(THD_COUNT)
    {
    calc_edge_count(sgraph_out, sgraph_in);
    }
    end = mywtime();
    cout << "degree time = " << end-start << endl;
    
    //prefix sum then reset the count
    start = mywtime(); 
    #pragma omp parallel num_threads(THD_COUNT)
    {
    prep_sgraph_internal(sgraph_out);
    prep_sgraph_internal(sgraph_in);
    }
    end = mywtime();
    cout << "prep time = " << end-start << endl;
    
    //populate and get the original count back
    start = mywtime(); 
    #pragma omp parallel num_threads(THD_COUNT)
    {
    fill_adj_list(sgraph_out, sgraph_in);
    }
    end = mywtime();
    cout << "fill  time = " << end-start << endl;
    
    //blog->blog_tail = blog->blog_marker;
    */
}

template <class T> 
void dgraph<T>::store_graph_baseline(bool clean)
{
    //#pragma omp parallel num_threads(THD_COUNT)
    {
    store_sgraph(sgraph_out, clean);
    store_sgraph(sgraph_in, clean);
    }
}

template <class T> 
void dgraph<T>::file_open(const string& odir, bool trunc)
{
    string postfix = "out";
    file_open_sgraph(sgraph_out, odir, postfix, trunc);
    postfix = "in";
    file_open_sgraph(sgraph_in, odir, postfix, trunc);
}

template <class T> 
void dgraph<T>::read_graph_baseline()
{
    tid_t   t_count    = g->get_total_types();
    
    if (0 == sgraph_out) {
        sgraph_out  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    read_sgraph(sgraph_out);
    
    if (0 == sgraph_in) {
        sgraph_in  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    read_sgraph(sgraph_in);
}

/*******************************************/
template <class T> 
void ugraph<T>::prep_graph_baseline()
{
    this->alloc_edgelog( 1 << BLOG_SHIFT);
    flag1 = flag1 | flag2;
    flag2 = flag1;

    flag1_count = __builtin_popcountll(flag1);
    flag2_count = flag1_count;

    //super bins memory allocation
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph) {
        sgraph  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    prep_sgraph(flag1, sgraph); 
}

template <class T> 
void ugraph<T>::calc_degree()
{
    //estimate edge count
    calc_edge_count(sgraph, sgraph);
}

template <class T> 
void ugraph<T>::make_graph_baseline()
{
    this->make_graph_u();
    
    /*
    if (blog->blog_tail >= blog->blog_marker) return;
   
    double start, end;
    start = mywtime(); 
    
    #pragma omp parallel   num_threads(THD_COUNT)  
    {
        calc_edge_count(sgraph, sgraph);
        #pragma omp master 
        {
            end = mywtime();
            cout << " calc degree time = " << end - start << endl;
        }
        prep_sgraph_internal(sgraph);
        #pragma omp master 
        {
            end = mywtime();
            cout << " prep time = " << end - start << endl;
        }
        fill_adj_list(sgraph, sgraph);
        #pragma omp master 
        {
            end = mywtime();
            cout << " fill adj time = " << end - start << endl;
        }
    }
    end = mywtime();
    cout << "Make graph time = " << end - start << endl;
    //blog->blog_tail = blog->blog_marker;  
   */ 
}

template <class T> 
void ugraph<T>::store_graph_baseline(bool clean)
{
    double start, end;
    start = mywtime(); 
    store_sgraph(sgraph, clean);
    end = mywtime();
    cout << "store graph time = " << end - start << endl;
}

template <class T> 
void ugraph<T>::file_open(const string& odir, bool trunc)
{
    string postfix = "";
    file_open_sgraph(sgraph, odir, postfix, trunc);
}

template <class T> 
void ugraph<T>::read_graph_baseline()
{
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph) {
        sgraph  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    read_sgraph(sgraph);
}

/***************************************/
template <class T> 
void many2one<T>::prep_graph_baseline()
{
    this->alloc_edgelog(1 << BLOG_SHIFT);
    flag1_count = __builtin_popcountll(flag1);
    flag2_count = __builtin_popcountll(flag2);

    //super bins memory allocation
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph_in) {
        sgraph_in  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    prep_sgraph(flag2, sgraph_in);
    
    if (0 == skv_out) {
        skv_out  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }

    skv_out  = prep_skv(flag1, skv_out);
}

template <class T> 
void many2one<T>::calc_degree()
{
    //estimate edge count
    calc_edge_count_in(sgraph_in);
}

template <class T> 
void many2one<T>::make_graph_baseline()
{
    if (blog->blog_tail >= blog->blog_marker) return;
    
    calc_edge_count_in(sgraph_in);
    
    //prefix sum then reset the count
    prep_sgraph_internal(sgraph_in);

    //populate and get the original count back
    //handle kv_out as well.
    fill_adj_list_in(skv_out, sgraph_in);
    //update_count(sgraph_in);
    blog->blog_tail = blog->blog_marker;  
}

template <class T> 
void many2one<T>::store_graph_baseline(bool clean)
{
    store_skv(skv_out);
    store_sgraph(sgraph_in);
}

template <class T> 
void many2one<T>::file_open(const string& odir, bool trunc)
{
    string postfix = "in";
    file_open_sgraph(sgraph_in, odir, postfix, trunc);
    postfix = "out";
    file_open_skv(skv_out, odir, postfix, trunc);
}

template <class T> 
void many2one<T>::read_graph_baseline()
{
    tid_t   t_count = g->get_total_types();
    
    if (0 == skv_out) {
        skv_out  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    read_skv(skv_out);
    
    if (0 == sgraph_in) {
        sgraph_in  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    read_sgraph(sgraph_in);
}

/*******************************************/
template <class T> 
void one2many<T>::prep_graph_baseline()
{
    this->alloc_edgelog(1 << BLOG_SHIFT);
    flag1_count = __builtin_popcountll(flag1);
    flag2_count = __builtin_popcountll(flag2);

    //super bins memory allocation
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph_out) {
        sgraph_out  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    prep_sgraph(flag1, sgraph_out);
    
    if (0 == skv_in) {
        skv_in  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    
    skv_in   = prep_skv(flag2, skv_in);
}
    
template <class T> 
void one2many<T>::calc_degree()
{
    //estimate edge count
    calc_edge_count_out(sgraph_out);
}
    
template <class T> 
void one2many<T>::make_graph_baseline()
{
    if (blog->blog_tail >= blog->blog_marker) return;
    
    calc_edge_count_out(sgraph_out);
    
    //prefix sum then reset the count
    prep_sgraph_internal(sgraph_out);

    //populate and get the original count back
    //handle kv_in as well.
    fill_adj_list_out(sgraph_out, skv_in);
    //update_count(sgraph_out);
    blog->blog_tail = blog->blog_marker;  
    
}

template <class T> 
void one2many<T>::store_graph_baseline(bool clean)
{
    store_sgraph(sgraph_out);
    store_skv(skv_in);
}

template <class T> 
void one2many<T>::file_open(const string& odir, bool trunc)
{
    string postfix = "out";
    file_open_sgraph(sgraph_out, odir, postfix, trunc);
    postfix = "in";
    file_open_skv(skv_in, odir, postfix, trunc);
}

template <class T> 
void one2many<T>::read_graph_baseline()
{
    tid_t   t_count = g->get_total_types();
    
    if (0 == sgraph_out) {
        sgraph_out  = (onegraph_t<T>**) calloc (sizeof(onegraph_t<T>*), t_count);
    }
    read_sgraph(sgraph_out);
    
    if (0 == skv_in) {
        skv_in  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    read_skv(skv_in);
}

/************************************************/
template <class T> 
void one2one<T>::prep_graph_baseline()
{
    flag1_count = __builtin_popcountll(flag1);
    flag2_count = __builtin_popcountll(flag2);
    tid_t   t_count    = g->get_total_types();

    //super bins memory allocation
    
    if (0 == skv_in) {
        skv_in  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    skv_in  = prep_skv(flag2, skv_in);
    
    if (0 == skv_out) {
        skv_out  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    skv_out = prep_skv(flag1, skv_out);
}

template <class T> 
void one2one<T>::calc_degree()
{
}

template <class T> 
void one2one<T>::make_graph_baseline()
{
    if (blog->blog_tail >= blog->blog_marker) return;

    //handle kv_out as well as kv_in.
    fill_skv(skv_out, skv_in);
    
}

template <class T> 
void one2one<T>::store_graph_baseline(bool clean)
{
    store_skv(skv_out);
    store_skv(skv_in);
}

template <class T> 
void one2one<T>::file_open(const string& odir, bool trunc)
{
    string postfix = "out";
    file_open_skv(skv_out, odir, postfix, trunc);
    postfix = "in";
    file_open_skv(skv_in, odir, postfix, trunc);
}

template <class T> 
void one2one<T>::read_graph_baseline()
{
    tid_t   t_count    = g->get_total_types();
    
    if (0 == skv_out) {
        skv_out  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    read_skv(skv_out);
    
    if (0 == skv_in) {
        skv_in  = (onekv_t<T>**) calloc (sizeof(onekv_t<T>*), t_count);
    }
    read_skv(skv_in);
}

template <class T> 
cfinfo_t* ugraph<T>::create_instance()
{
    return new ugraph_t;
}

template <class T> 
cfinfo_t* dgraph<T>::create_instance()
{
    return new dgraph_t;
}

template <class T> 
cfinfo_t* one2one<T>::create_instance()
{
    return new one2one_t;
}

template <class T> 
cfinfo_t* one2many<T>::create_instance()
{
    return new one2many_t;
}

template <class T> 
cfinfo_t* many2one<T>::create_instance()
{
    return new many2one_t;
}
//////
template <class T> 
void ugraph<T>::incr_count(sid_t src, sid_t dst, int del /*= 0*/)
{
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    if (!del) { 
        sgraph[src_index]->increment_count(vert1_id);
        sgraph[dst_index]->increment_count(vert2_id);
    } else { 
        sgraph[src_index]->decrement_count(vert1_id);
        sgraph[dst_index]->decrement_count(vert2_id);
    }
}

template <class T> 
void dgraph<T>::incr_count(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        sgraph_out[src_index]->increment_count(vert1_id);
        sgraph_in[dst_index]->increment_count(vert2_id);
    } else { 
        sgraph_out[src_index]->decrement_count(vert1_id);
        sgraph_in[dst_index]->decrement_count(vert2_id);
    }
}

template <class T> 
void one2one<T>::incr_count(sid_t src, sid_t dst, int del /*= 0*/)
{
}

template <class T> 
void one2many<T>::incr_count(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        sgraph_out[dst_index]->increment_count(vert2_id);
    } else { 
        skv_in[dst_index]->decrement_count(vert2_id);
    }
}

template <class T> 
void many2one<T>::incr_count(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    
    vid_t vert1_id = TO_VID(src);
    
    if (!del) { 
        skv_out[src_index]->increment_count(vert1_id);
    } else { 
        sgraph_in[src_index]->decrement_count(vert1_id);
    }
}

template <class T> 
void ugraph<T>::add_nebr(sid_t src, sid_t dst, int del /*= 0*/)
{
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    if (!del) { 
        sgraph[src_index]->add_nebr(vert1_id, dst);
        sgraph[dst_index]->add_nebr(vert2_id, src);
    } else { 
        sgraph[src_index]->del_nebr(vert1_id, dst);
        sgraph[dst_index]->del_nebr(vert2_id, src);
    }
}

template <class T> 
void dgraph<T>::add_nebr(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        sgraph_out[src_index]->add_nebr(vert1_id, dst);
        sgraph_in[dst_index]->add_nebr(vert2_id, src);
    } else { 
        sgraph_out[src_index]->del_nebr(vert1_id, dst);
        sgraph_in[dst_index]->del_nebr(vert2_id, src);
    }
}

template <class T> 
void one2one<T>::add_nebr(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        skv_out[src_index]->set_value(vert1_id, dst);
        skv_in[dst_index]->set_value(vert2_id, src);
    } else { 
        skv_out[src_index]->set_value(vert1_id, dst);
        skv_in[dst_index]->set_value(vert2_id, src);
    }
}

template <class T> 
void many2one<T>::add_nebr(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        sgraph_in[dst_index]->add_nebr(vert2_id, src);
        skv_out[src_index]->set_value(vert1_id, dst);
    } else { 
        sgraph_in[dst_index]->del_nebr(vert2_id, src);
        skv_out[src_index]->set_value(vert1_id, dst);
    }
}

template <class T> 
void one2many<T>::add_nebr(sid_t src, sid_t dst, int del /*= 0*/)
{
    tid_t src_index = TO_TID(src);
    tid_t dst_index = TO_TID(dst);
    
    vid_t vert1_id = TO_VID(src);
    vid_t vert2_id = TO_VID(dst);
    
    if (!del) { 
        sgraph_out[src_index]->add_nebr(vert1_id, dst);
        skv_in[dst_index]->set_value(vert2_id, src);
    } else { 
        sgraph_out[src_index]->del_nebr(vert1_id, dst);
        skv_in[dst_index]->set_value(vert2_id, src);
    }
}
/////
template <class T> 
void ugraph<T>::create_snapshot()
{
    update_count(sgraph);
}

template <class T> 
void dgraph<T>::create_snapshot()
{
    update_count(sgraph_out);
    update_count(sgraph_in);
}

template <class T> 
void one2one<T>::create_snapshot()
{
    return;
}

template <class T> 
void one2many<T>::create_snapshot()
{
    update_count(sgraph_out);
}

template <class T> 
void many2one<T>::create_snapshot()
{
    update_count(sgraph_in);
}

/*
template <class T>
status_t ugraph<T>::extend(srset_t* iset, srset_t* oset, direction_t direction)
{
    return extend_adjlist_td(sgraph, iset, oset);
}

//due to many2one structure, we give preference to bottom up approach
template <class T>
status_t many2one<T>::extend(srset_t* iset, srset_t* oset, direction_t direction)
{
    if (direction == eout) {
        return extend_kv_td(skv_out,  iset, oset);
    } else {
        assert(direction == ein);
        return extend_adjlist_td(sgraph_in, iset, oset);
    }
    return eOK;
}

template <class T>
status_t dgraph<T>::extend(srset_t* iset, srset_t* oset, direction_t direction)
{
    if (direction == eout) {
        return extend_adjlist_td(sgraph_out, iset, oset);
    } else {
        assert(direction == ein);
        return extend_adjlist_td(sgraph_in, iset, oset);
    }
    return eOK;
}

template <class T>
status_t one2one<T>::extend(srset_t* iset, srset_t* oset, direction_t direction)
{
    if (direction == eout) {
        return extend_kv_td(skv_out, iset, oset);
    } else {
        assert(direction == ein);
        return extend_kv_td(skv_in, iset, oset);
    }
    return eOK;
}

template <class T>
status_t one2many<T>::extend(srset_t* iset, srset_t* oset, direction_t direction)
{
    if (direction == eout) {
        return extend_adjlist_td(sgraph_out, iset, oset);
    } else {
        assert(direction == ein);
        return extend_kv_td(skv_in, iset, oset);
    }
    return eOK;
}

////////
template <class T>
status_t ugraph<T>::transform(srset_t* iset, srset_t* oset, direction_t direction)
{
    //prepare the output 1,2;
    oset->full_setup(sgraph);
    int total_count = 0;
    
    if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
        return query_adjlist_td(sgraph, iset, oset);
    } else { //bottom up approach
        return query_adjlist_bu(sgraph, iset, oset);
    }
    return eOK;
}

//due to many2one structure, we give preference to bottom up approach
template <class T>
status_t many2one<T>::transform(srset_t* iset, srset_t* oset, direction_t direction)
{
    int total_count = 0;

    if (direction == eout) {
        oset->full_setup(sgraph_in);
        total_count = 0;
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_kv_td(skv_out, iset, oset);
        } else { //bottom up approach
            return query_adjlist_bu(sgraph_in, iset, oset);
        }
    } else {
        assert(direction == ein);
        oset->full_setup(skv_out);
        total_count = 0;
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_adjlist_td(sgraph_in, iset, oset);
        } else { //bottom up approach 
            return query_kv_bu(skv_out, iset, oset);
        }
    }
    return eOK;
}

template <class T>
status_t dgraph<T>::transform(srset_t* iset, srset_t* oset, direction_t direction)
{
    int total_count = 0;
    if (direction == eout) {
        total_count = 0;
        oset->full_setup(sgraph_in);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_adjlist_td(sgraph_out, iset, oset);
        } else { //bottom up approach
            return query_adjlist_bu(sgraph_in, iset, oset);
        }
    } else {
        assert(direction == ein);
        total_count = 0;
        oset->full_setup(sgraph_out);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_adjlist_td(sgraph_in, iset, oset);
        } else { //bottom up approach 
            return query_adjlist_bu(sgraph_out, iset, oset);
        }
    }
    return eOK;
}

template <class T>
status_t one2one<T>::transform(srset_t* iset, srset_t* oset, direction_t direction)
{
    int total_count = 0;
    if (direction == eout) {
        total_count = 0;
        oset->full_setup(skv_in);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_kv_td(skv_out, iset, oset);
        } else { //bottom up approach
            return query_kv_bu(skv_in, iset, oset);
        }
    } else {
        assert(direction == ein);
        total_count = 0;
        oset->full_setup(skv_out);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_kv_td(skv_in, iset, oset);
        } else { //bottom up approach 
            return query_kv_bu(skv_out, iset, oset);
        }
    }
    return eOK;
}

template <class T>
status_t one2many<T>::transform(srset_t* iset, srset_t* oset, direction_t direction)
{
    int total_count = 0;
    if (direction == eout) {
        total_count = 0;
        oset->full_setup(skv_in);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_adjlist_td(sgraph_out, iset, oset);
        } else { //bottom up approach
            return query_kv_bu(skv_in, iset, oset);
        }
    } else {
        assert(direction == ein);
        total_count = 0;
        oset->full_setup(sgraph_out);
        if (iset->get_total_vcount() <= bu_factor*total_count) { //top down approach
            return query_kv_td(skv_in, iset, oset);
        } else { //bottom up approach 
            return query_adjlist_bu(sgraph_out, iset, oset);
        }
    }
    return eOK;
}
*/

/*
template <class T>
void pgraph_t<T>::update_count(onegraph_t<T>** sgraph)
{
    tid_t       t_count = g->get_total_types();
    
    for(tid_t i = 0; i < t_count; i++) {
        if (0 == sgraph[i]) continue;
        sgraph[i]->update_count();
    }

}
*/
/////////// QUERIES ///////////////////////////
/*
status_t pgraph_t::query_adjlist_td(sgraph_t** sgraph, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        vid_t v_count = rset->get_vcount();
        vid_t* vlist = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == sgraph[tid]) continue;
        beg_pos_t* graph = sgraph[tid]->get_begpos();

        
        //Get the frontiers
        vid_t     frontier;
        for (vid_t v = 0; v < v_count; v++) {
            frontier = vlist[v];
            sid_t* adj_list = graph[frontier].get_adjlist();
            vid_t nebr_count = adj_list[0];
            ++adj_list;
            
            //traverse the adj list
            for (vid_t k = 0; k < nebr_count; ++k) {
                oset->set_status(adj_list[k]);
            }
        }
    }
    return eOK;
}
status_t pgraph_t::query_kv_td(skv_t** skv, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        vid_t v_count = rset->get_vcount();
        vid_t* vlist = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == skv[tid]) continue;
        sid_t* kv = skv[tid]->get_kv(); 

        //Get the frontiers
        vid_t     frontier;
        for (vid_t v = 0; v < v_count; v++) {
            frontier = vlist[v];
            oset->set_status(kv[frontier]);
        }
    }
    return eOK;
}

//sgraph_in and oset share the same flag.
status_t pgraph_t::query_adjlist_bu(sgraph_t** sgraph, srset_t* iset, srset_t* oset)
{
    rset_t* rset = 0;
    tid_t   tid  = 0;
    tid_t oset_count = oset->get_rset_count();

    for (tid_t i = 0; i < oset_count; ++i) {
        
        //get the graph where we will traverse
        rset = oset->rset + i;
        tid  = rset->get_tid();
        if (0 == sgraph[tid]) continue; 

        beg_pos_t* graph = sgraph[tid]->get_begpos(); 
        vid_t    v_count = sgraph[tid]->get_vcount();
        
        
        for (vid_t v = 0; v < v_count; v++) {
            //traverse the adj list
            sid_t* adj_list = graph[v].get_adjlist();
            vid_t nebr_count = adj_list[0];
            ++adj_list;
            for (vid_t k = 0; k < nebr_count; ++k) {
                if (iset->get_status(adj_list[k])) {
                    rset->set_status(v);
                    break;
                }
            }
        }
    }
    return eOK;
}

status_t pgraph_t::query_kv_bu(skv_t** skv, srset_t* iset, srset_t* oset) 
{
    rset_t*  rset = 0;
    tid_t    tid  = 0;
    tid_t    oset_count = oset->get_rset_count();
    for (tid_t i = 0; i < oset_count; ++i) {

        //get the graph where we will traverse
        rset = oset->rset + i;
        tid  = rset->get_tid(); 
        if (0 == skv[tid]) continue;

        vid_t*       kv = skv[tid]->get_kv(); 
        sid_t   v_count = skv[tid]->get_vcount();
        
        for (vid_t v = 0; v < v_count; ++v) {
            if (iset->get_status(kv[v])) {
                rset->set_status(v);
                break;
            }
        }
    }
    return eOK;
}

//////extend functions ------------------------
status_t 
pgraph_t::extend_adjlist_td(sgraph_t** sgraph, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;
    rset_t*        rset2 = 0;

    iset->bitwise2vlist();
    //prepare the output 1,2;
    oset->copy_setup(iset, eAdjlist);

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        rset2 = oset->rset + i;
        vid_t v_count = rset->get_vcount();
        sid_t* varray = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t        tid = rset->get_tid();
        if (0 == sgraph[tid]) continue;
        beg_pos_t* graph = sgraph[tid]->get_begpos(); 
        
        for (vid_t v = 0; v < v_count; v++) {
            rset2->add_adjlist_ro(v, graph+varray[v]);
        }
    }
    return eOK;
}

status_t 
pgraph_t::extend_kv_td(skv_t** skv, srset_t* iset, srset_t* oset)
{
    tid_t    iset_count = iset->get_rset_count();
    rset_t*        rset = 0;
    rset_t*       rset2 = 0;

    iset->bitwise2vlist();
    //prepare the output 1,2;
    oset->copy_setup(iset, eKV);

    for (tid_t i = 0; i < iset_count; ++i) {
        rset = iset->rset + i;
        rset2 = oset->rset + i;
        vid_t v_count = rset->get_vcount();
        sid_t* varray = rset->get_vlist();
        
        //get the graph where we will traverse
        tid_t     tid = rset->get_tid();
        if (0 == skv[tid]) continue;
        sid_t*  graph = skv[tid]->get_kv(); 
        
        for (vid_t v = 0; v < v_count; v++) {
            rset2->add_kv(v, graph[varray[v]]);
        }
    }
    return eOK;
}
*/
#include "sgraph2.h"
