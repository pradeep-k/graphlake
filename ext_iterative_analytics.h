#pragma once
#include <omp.h>
#include <algorithm>
#include "graph.h"
#include "wtime.h"
#include "mem_iterative_analytics.h"
#include "sgraph.h"
//#include "p_sgraph.h"

using std::min;

template<class T>
void
ext_hop1(vert_table_t<T>* graph_out, degree_t* degree_out, 
        snapshot_t* snapshot, index_t marker, edgeT_t<T>* edges,
        vid_t v_count)
{
    index_t         old_marker = 0;

    if (snapshot) { 
        old_marker = snapshot->marker;
    }
    
    srand(0);
    int query_count = 512;
    vid_t* query = (vid_t*)calloc(sizeof(vid_t), query_count);
    int i1 = 0;
    while (i1 < query_count) {
        query[i1] = rand()% v_count;
        if (degree_out[query[i1]] != 0) { ++i1; };
    }

    index_t          sum = 0;
    index_t         sum1 = 0;
    index_t         sum2 = 0;
    cout << "starting 1 HOP" << endl;
	double start = mywtime();
    
    #pragma omp parallel
    {
    degree_t      delta_degree = 0;
    degree_t    durable_degree = 0;
    degree_t        nebr_count = 0;
    degree_t      local_degree = 0;

    sid_t sid = 0;
    vid_t v   = 0;
    vert_table_t<T>* graph  = graph_out;
    delta_adjlist_t<T>* delta_adjlist;
    vunit_t<T>* v_unit = 0;
    T* adj_list = 0;
    T* local_adjlist = 0;

    #pragma omp for reduction(+:sum) schedule (static) nowait
    for (int i = 0; i < query_count; i++) {
        
        v = query[i];
        v_unit = graph[v].get_vunit();
        if (0 == v_unit) continue;

        durable_degree = v_unit->count;
        adj_list = v_unit->adj_list;
        delta_adjlist = v_unit->delta_adjlist;
        ++adj_list;
        nebr_count = degree_out[v];
        
        //traverse the delta adj list
        delta_degree = nebr_count - durable_degree;
        
        while (delta_adjlist != 0 && delta_degree > 0) {
            local_adjlist = delta_adjlist->get_adjlist();
            local_degree = delta_adjlist->get_nebrcount();
            degree_t i_count = min(local_degree, delta_degree);
            for (degree_t i = 0; i < i_count; ++i) {
                sid = get_nebr(local_adjlist, i);
                sum += sid;
            }
            delta_adjlist = delta_adjlist->get_next();
            delta_degree -= local_degree;
        }
        degree_t k_count = min(nebr_count, durable_degree);
        
        //traverse the adj list
        for (vid_t k = 0; k < k_count; ++k) {
            sid = get_nebr(adj_list, k);
            sum += sid;
        }
    }

    //on-the-fly snapshots should process this
    vid_t src, dst; 
    #pragma omp for reduction(+:sum1) nowait
    for (index_t j = old_marker; j < marker; ++j) {
        for (int i = 0; i < query_count; i++) {
            v = query[i];
            src = edges[j].src_id;
            dst = edges[j].dst_id;
            if (src == v) {
                sum1 += dst;
            }

            if (dst == v) {
                sum1 += src;
            }
        }
    }
    }
    sum += sum1;
    sum2 += sum;
    double end = mywtime();

    cout << "Sum = " << sum << " 1 Hop Time = " << end - start << endl;
}

template<class T>
void
ext_hop2(vert_table_t<T>* graph_out, degree_t* degree_out, 
        snapshot_t* snapshot, index_t marker, edgeT_t<T>* edges,
        vid_t v_count)
{
    index_t         old_marker = 0;
    if (snapshot) { 
        old_marker = snapshot->marker;
    }
    
    srand(0);
    int query_count = 512;
    hop2_t* query = (hop2_t*)calloc(sizeof(hop2_t), query_count); 
    int i1 = 0;
    while (i1 < query_count) {
        query[i1].vid = rand()% v_count;
        if (degree_out[query[i1].vid] != 0) { ++i1; };
    }

	double start = mywtime();

    #pragma omp parallel
    {    
    degree_t      delta_degree = 0;
    degree_t    durable_degree = 0;
    degree_t        nebr_count = 0;
    degree_t      local_degree = 0;

    sid_t sid = 0;
    vid_t v   = 0;
    vert_table_t<T>* graph  = graph_out;
    T* adj_list = 0;
    T* local_adjlist = 0;
    delta_adjlist_t<T>* delta_adjlist;
    vunit_t<T>* v_unit = 0;
    degree_t d = 0;
    vid_t* vlist = 0;
    
    //first hop------------------
    #pragma omp for schedule(static) nowait
    for (int q = 0; q < query_count; q++) {
        d = 0; 
        v = query[q].vid;
        nebr_count = degree_out[v];
        vlist = (vid_t*)calloc(sizeof(vid_t), nebr_count);
        query[q].vlist = vlist;
        v_unit = graph[v].get_vunit();
        if (0 == v_unit) continue;

        durable_degree = v_unit->count;
        adj_list = v_unit->adj_list;
        delta_adjlist = v_unit->delta_adjlist;
        ++adj_list;
        
        //traverse the delta adj list
        delta_degree = nebr_count - durable_degree;
        
        while (delta_adjlist != 0 && delta_degree > 0) {
            local_adjlist = delta_adjlist->get_adjlist();
            local_degree = delta_adjlist->get_nebrcount();
            degree_t i_count = min(local_degree, delta_degree);
            for (degree_t i = 0; i < i_count; ++i) {
                sid = get_nebr(local_adjlist, i);
                vlist[d] = sid;
                ++d;
            }
            delta_adjlist = delta_adjlist->get_next();
            delta_degree -= local_degree;
        }
        degree_t k_count = min(nebr_count, durable_degree);
        
        //traverse the adj list
        for (vid_t k = 0; k < k_count; ++k) {
            sid = get_nebr(adj_list, k);
            vlist[d] = sid;
            ++d;
        }
        __sync_fetch_and_add(&query[q].d, d);
    }
    
    //on-the-fly snapshots should process this

    vid_t src, dst;
    degree_t d1 = 0;
    #pragma omp for schedule(static) 
    for (index_t i = old_marker; i < marker; ++i) {
        src = edges[i].src_id;
        dst = edges[i].dst_id;
        for (int q = 0; q < query_count; q++) {
            vlist = query[q].vlist;
            v = query[q].vid;
            if (src == v) {
                d1 = __sync_fetch_and_add(&query[q].d, 1);
                vlist[d1] = dst;
            }

            if (dst == v) {
                d1 = __sync_fetch_and_add(&query[q].d, 1);
                vlist[d1] = src;
            }
        }
    }
    }
    
    //Second hop------------------
    index_t     sum = 0;
    index_t    sum1 = 0;
    index_t    sum2 = 0;
    
    #pragma omp parallel  
    {
    degree_t      delta_degree = 0;
    degree_t    durable_degree = 0;
    degree_t        nebr_count = 0;
    degree_t      local_degree = 0;
    sid_t sid = 0;
    vid_t v = 0;
    vert_table_t<T>* graph  = graph_out;
    T* adj_list = 0;
    T* local_adjlist = 0;
    delta_adjlist_t<T>* delta_adjlist;
    vunit_t<T>* v_unit = 0;
    vid_t* vlist = 0;
    degree_t d = 0;
    
    #pragma omp for schedule (static) reduction(+:sum) nowait
    for (int q = 0; q < query_count; q++) {
        vlist = query[q].vlist;
        d = query[q].d;

        for (vid_t j = 0; j < d; ++j) {
            v = vlist[j];
            v_unit = graph[v].get_vunit();
            if (0 == v_unit) continue;

            durable_degree = v_unit->count;
            adj_list = v_unit->adj_list;
            delta_adjlist = v_unit->delta_adjlist;
            ++adj_list;
            nebr_count = degree_out[v];
            
            //traverse the delta adj list
            delta_degree = nebr_count - durable_degree;
            
            while (delta_adjlist != 0 && delta_degree > 0) {
                local_adjlist = delta_adjlist->get_adjlist();
                local_degree = delta_adjlist->get_nebrcount();
                degree_t i_count = min(local_degree, delta_degree);
                for (degree_t i = 0; i < i_count; ++i) {
                    sid = get_nebr(local_adjlist, i);
                    sum += sid;
                }
                delta_adjlist = delta_adjlist->get_next();
                delta_degree -= local_degree;
            }
            degree_t k_count = min(nebr_count, durable_degree);
            
            //traverse the adj list
            for (vid_t k = 0; k < k_count; ++k) {
                sid = get_nebr(adj_list, k);
                sum += sid;
            }
        }
    }

    //on-the-fly snapshots should process this
    vid_t src, dst;
    #pragma omp for reduction(+:sum1)
    for (index_t i = old_marker; i < marker; ++i) {
        src = edges[i].src_id;
        dst = edges[i].dst_id;
        for (int q = 0; q < query_count; q++) {
            d = query[q].d;
            vlist = query[q].vlist;
            for (degree_t j = 0; j < d; ++j) {
                v = vlist[j];
                if (src == v) {
                    sum1 += dst;
                }

                if (dst == v) {
                    sum1 += src;
                }
            }
        }
    }
    }
        
    //if(vlist) free(vlist);
    //vlist = 0;
    sum2 += sum1;
    sum2 += sum; 

    double end = mywtime();
    free(query);
    cout << "Sum = " << sum2 << " 2 Hop Time = " << end - start << endl;
}

template<class T>
void
ext_bfs(onegraph_t<T>* sgraph_out, degree_t* degree_out, 
        onegraph_t<T>* sgraph_in, degree_t* degree_in,
        snapshot_t* snapshot, index_t marker, edgeT_t<T>* edges,
        vid_t v_count, uint8_t* status, sid_t root)
{
	int				level      = 1;
	int				top_down   = 1;
	sid_t			frontier   = 0;
    index_t         old_marker = 0;
    vert_table_t<T>* graph_out = sgraph_out->get_begpos();
    vert_table_t<T>* graph_in = sgraph_in->get_begpos();
    int etf_out = sgraph_out->etf;
    int etf_in  = sgraph_in->etf;


    if (snapshot) { 
        old_marker = snapshot->marker;
    }
    
	double start1 = mywtime();
	status[root] = level;
    
	do {
		frontier = 0;
		double start = mywtime();
		#pragma omp parallel reduction(+:frontier)
		{
            sid_t sid;
            degree_t durable_degree = 0;
            degree_t nebr_count     = 0;
            degree_t local_degree   = 0;
            degree_t delta_degree   = 0;
            T*       local_adjlist  = 0;
            T*            adj_list  = 0;
            vunit_t<T>*     v_unit  = 0;
            index_t         offset  = 0;
            
            delta_adjlist_t<T>*   delta_adjlist;;
            durable_adjlist_t<T>* durable_adjlist =  (durable_adjlist_t<T>*)malloc(4096);
            index_t buf_sz = 4096; 
            vert_table_t<T>* graph  = 0;
		    
            if (top_down) {
                graph  = graph_out;
				
                #pragma omp for nowait
				for (vid_t v = 0; v < v_count; v++) {
					if (status[v] != level) continue;
					v_unit = graph[v].get_vunit();
                    if (0 == v_unit) continue;

                    durable_degree = v_unit->count;
                    delta_adjlist = v_unit->delta_adjlist;
					nebr_count = degree_out[v];
                    
                    //traverse the delta adj list
                    delta_degree = nebr_count - durable_degree;
                    
                    while (delta_adjlist != 0 && delta_degree > 0) {
                        local_adjlist = delta_adjlist->get_adjlist();
                        local_degree = delta_adjlist->get_nebrcount();
                        degree_t i_count = min(local_degree, delta_degree);
                        for (degree_t i = 0; i < i_count; ++i) {
                            sid = get_nebr(local_adjlist, i);
                            if (status[sid] == 0) {
                                status[sid] = level + 1;
                                ++frontier;
                            }
                        }
                        delta_adjlist = delta_adjlist->get_next();
                        delta_degree -= local_degree;
                    }

                    degree_t k_count = min(nebr_count, durable_degree);
                    if (k_count == 0) continue;
                    offset = v_unit->offset;
                    index_t sz_to_read = (k_count)*sizeof(T) + sizeof(durable_adjlist_t<T>);
                    if (sz_to_read > buf_sz) {
                        free(durable_adjlist);
                        durable_adjlist =  (durable_adjlist_t<T>*)malloc(sz_to_read);
                        buf_sz = sz_to_read;
                    }
                    adj_list = durable_adjlist->get_adjlist();
                    pread(etf_out, durable_adjlist, sz_to_read, offset);
                    
                    //cout << "Vertex: " << v << " durable degree " << durable_degree << endl;
					//traverse the adj list
                    assert(durable_degree == durable_adjlist->get_nebrcount());
					for (degree_t k = 0; k < k_count; ++k) {
                        sid = get_nebr(adj_list, k);
						if (status[sid] == 0) {
							status[sid] = level + 1;
							++frontier;
						}
					}

				}
			} else {//bottom up
				graph = graph_in;
				
				#pragma omp for nowait
				for (vid_t v = 0; v < v_count; v++) {
					if (status[v] != 0 ) continue;
					v_unit = graph[v].get_vunit();
                    if (0 == v_unit) continue;

                    durable_degree = v_unit->count;
                    delta_adjlist = v_unit->delta_adjlist;
					
					nebr_count = degree_in[v];
                    
                    //traverse the delta adj list
                    delta_degree = nebr_count - durable_degree;
                    while (delta_adjlist != 0 && delta_degree > 0) {
                        local_adjlist = delta_adjlist->get_adjlist();
                        local_degree = delta_adjlist->get_nebrcount();
                        degree_t i_count = min(local_degree, delta_degree);
                        for (degree_t i = 0; i < i_count; ++i) {
                            sid = get_nebr(local_adjlist, i);
                            if (status[sid] == level) {
                                status[v] = level + 1;
                                ++frontier;
                                break;
                            }
                        }
                        delta_adjlist = delta_adjlist->get_next();
                        delta_degree -= local_degree;
                    }
					
                    if (status[v] == level + 1) continue;

					//traverse the adj list
                    degree_t k_count = min(nebr_count, durable_degree);
                    if (k_count == 0) continue;
                    offset = v_unit->offset;
                    index_t sz_to_read = (k_count)*sizeof(T) + sizeof(durable_adjlist_t<T>);
                    
                    if (sz_to_read > buf_sz) {
                        free(durable_adjlist);
                        durable_adjlist =  (durable_adjlist_t<T>*)malloc(sz_to_read);
                        buf_sz = sz_to_read;
                    }
                    adj_list = durable_adjlist->get_adjlist();
                    pread(etf_in, durable_adjlist, sz_to_read, offset);
					
                    assert(durable_degree == durable_adjlist->get_nebrcount());
                    for (degree_t k = 0; k < k_count; ++k) {
                        sid = get_nebr(adj_list, k);
						if (status[sid] == level) {
							status[v] = level + 1;
							++frontier;
							break;
						}
					}
				}
		    }

            //on-the-fly snapshots should process this
            //cout << "On the Fly" << endl;
            vid_t src, dst;
            #pragma omp for 
            for (index_t i = old_marker; i < marker; ++i) {
                src = edges[i].src_id;
                dst = edges[i].dst_id;
                if (status[src] == 0 && status[dst] == level) {
                    status[src] = level + 1;
                    ++frontier;
                    //cout << " " << src << endl;
                } 
                if (status[src] == level && status[dst] == 0) {
                    status[dst] = level + 1;
                    ++frontier;
                    //cout << " " << dst << endl;
                }
            }
        }

		double end = mywtime();
	
		cout << "Top down = " << top_down
		     << " Level = " << level
             << " Frontier Count = " << frontier
		     << " Time = " << end - start
		     << endl;
	
        //Point is to simulate bottom up bfs, and measure the trade-off    
		if ((frontier >= 0.002*v_count) || (level == 2)) {
			top_down = false;
		} else {
            top_down = true;
        }
		++level;
	} while (frontier);
		
    double end1 = mywtime();
    cout << "BFS Time = " << end1 - start1 << endl;

    for (int l = 1; l < level; ++l) {
        vid_t vid_count = 0;
        #pragma omp parallel for reduction (+:vid_count) 
        for (vid_t v = 0; v < v_count; ++v) {
            if (status[v] == l) ++vid_count;
        }
        cout << " Level = " << l << " count = " << vid_count << endl;
    }
}

template<class T>
void 
ext_pagerank(vert_table_t<T>* graph_in, degree_t* degree_in, degree_t* degree_out,
        snapshot_t* snapshot, index_t marker, edgeT_t<T>* edges,
        vid_t v_count, int iteration_count)
{
    index_t old_marker = 0;

	float* rank_array = 0 ;
	float* prior_rank_array = 0;
    float* dset = 0;
	
    double start = mywtime();
    
    if (snapshot) { 
        old_marker = snapshot->marker;
    }
    
    rank_array  = (float*)mmap(NULL, sizeof(float)*v_count, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB, 0, 0 );
    if (MAP_FAILED == rank_array) {
        cout << "Huge page alloc failed for rank array" << endl;
        rank_array = (float*)calloc(v_count, sizeof(float));
    }
    
    prior_rank_array  = (float*)mmap(NULL, sizeof(float)*v_count, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB, 0, 0 );
    if (MAP_FAILED == prior_rank_array) {
        cout << "Huge page alloc failed for prior rank array" << endl;
        prior_rank_array = (float*)calloc(v_count, sizeof(float));
    }
    
    dset  = (float*)mmap(NULL, sizeof(float)*v_count, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB, 0, 0 );
    
    if (MAP_FAILED == dset) {
        cout << "Huge page alloc failed for dset" << endl;
        dset = (float*)calloc(v_count, sizeof(float));
    }
	
	//initialize the rank, and get the degree information
    
    #pragma omp parallel
    { 
    degree_t degree = 0;
    float	inv_v_count = 0.15;//1.0f/vert_count;
    #pragma omp for
    for (vid_t v = 0; v < v_count; ++v) {
        degree = degree_out[v];
        if (degree != 0) {
            dset[v] = 1.0f/degree;
            prior_rank_array[v] = inv_v_count;//XXX
        } else {
            dset[v] = 0;
            prior_rank_array[v] = 0;
        }
    }
    }

	//let's run the pagerank
	for (int iter_count = 0; iter_count < iteration_count; ++iter_count) {
        double start1 = mywtime();
        #pragma omp parallel 
        {
            sid_t sid;
            degree_t      delta_degree = 0;
            degree_t durable_degree = 0;
            degree_t nebr_count = 0;
            degree_t local_degree = 0;

            vert_table_t<T>* graph  = 0;
            delta_adjlist_t<T>* delta_adjlist;
            T* adj_list = 0;
            T* local_adjlist = 0;

            vunit_t<T>* v_unit = 0;
            graph = graph_in;
            float rank = 0.0f; 
            
            #pragma omp for nowait
            for (vid_t v = 0; v < v_count; v++) {
                v_unit = graph[v].get_vunit();
                if (0 == v_unit) continue;

                durable_degree = v_unit->count;
                adj_list = v_unit->adj_list;
                delta_adjlist = v_unit->delta_adjlist;
                ++adj_list;
                
                nebr_count = degree_in[v];
                rank = 0.0f;
                
                //traverse the delta adj list
                delta_degree = nebr_count - durable_degree;
                while (delta_adjlist != 0 && delta_degree > 0) {
                    local_adjlist = delta_adjlist->get_adjlist();
                    local_degree = delta_adjlist->get_nebrcount();
                    degree_t i_count = min(local_degree, delta_degree);
                    for (degree_t i = 0; i < i_count; ++i) {
                        sid = get_nebr(local_adjlist, i);
                        rank += prior_rank_array[sid];
                    }
                    delta_adjlist = delta_adjlist->get_next();
                    delta_degree -= local_degree;
                }
                
                //traverse the adj list
                degree_t k_count = min(nebr_count, durable_degree);
                for (vid_t k = 0; k < k_count; ++k) {
                    sid = get_nebr(adj_list, k);
					rank += prior_rank_array[sid];
                    
                }
                //rank_array[v] = rank;
                qthread_dincr(rank_array + v, rank);
            }
        

            //on-the-fly snapshots should process this
            //cout << "On the Fly" << endl;
            vid_t src, dst;
            #pragma omp for 
            for (index_t i = old_marker; i < marker; ++i) {
                src = edges[i].src_id;
                dst = edges[i].dst_id;
                qthread_dincr(rank_array + src, prior_rank_array[dst]);
                qthread_dincr(rank_array + dst, prior_rank_array[src]);
            }

            if (iter_count != iteration_count -1) {
                #pragma omp for
                for (vid_t v = 0; v < v_count; v++ ) {
                    rank_array[v] = (0.15 + 0.85*rank_array[v])*dset[v];
                    prior_rank_array[v] = 0;
                } 
            } else { 
                #pragma omp for
                for (vid_t v = 0; v < v_count; v++ ) {
                    rank_array[v] = (0.15 + 0.85*rank_array[v]);
                    prior_rank_array[v] = 0;
                }
            }
        }
        swap(prior_rank_array, rank_array);
        double end1 = mywtime();
        cout << "Iteration Time = " << end1 - start1 << endl;
    }	
    double end = mywtime();

	cout << "PR Time = " << end - start << endl;
	cout << endl;
}


