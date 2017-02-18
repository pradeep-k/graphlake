#include <omp.h>
#include <sys/stat.h>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <sys/mman.h>
#include <asm/mman.h>

#include "wtime.h"
#include "graph.h"

using std::cout;
using std::endl;
using std::string;
using std::swap;
using std::sort;
using std::min;

#define mem_count 4096


ugraph_t* g = 0;


void ugraph_t::csr_from_file(string csrfile, vertex_t vert_count, csr_t* data)
{

	string file = csrfile + ".beg_pos";
    struct stat st_count;
    stat(file.c_str(), &st_count);
    assert(st_count.st_size == (vert_count +1)*sizeof(index_t));
	
	FILE* f = fopen(file.c_str(),"rb");
    assert(f != 0);
    data->beg_pos = (index_t*) malloc(st_count.st_size);
    assert(data->beg_pos);
    fread(data->beg_pos, sizeof(index_t), vert_count + 1, f);
    fclose(f);

	index_t edge_count = data->beg_pos[vert_count];
	file = csrfile + ".adj";
    struct stat st_edge;
    stat(file.c_str(), &st_edge);
    assert(st_edge.st_size == sizeof(vertex_t)*edge_count);
    
    f = fopen(file.c_str(), "rb");
    setbuf(f, 0);
    assert(f != 0);

    data->adj_list = (vertex_t*)mmap(NULL, st_edge.st_size, 
                       PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB|MAP_HUGE_2MB, 0 , 0);
    if (MAP_FAILED == data->adj_list) {
        data->adj_list = (vertex_t*) malloc(st_edge.st_size);
        cout << "Huge page allocatin failed" << endl;
    }

    assert(data->adj_list);
    fread(data->adj_list, sizeof(vertex_t), edge_count , f);
    fclose(f);
	
	data->vert_count = vert_count;
}

//It will not do a deep copy, memory will be used.
void
ugraph_t::init_from_csr(csr_t* data, int sorted)
{
	vertex_t		vert_count = data->vert_count;
	adj_list_t*		adj_list = (adj_list_t*)malloc(sizeof(adj_list_t)*vert_count);
	
	udata.vert_count	= vert_count;
	udata.adj_list		= adj_list;

	#pragma omp parallel num_threads(NUM_THDS)
	{
	vertex_t degree = 0;
	int leaf_count = 0;
	vertex_t* l_adj_list = 0;
	int level1_count = 0;
    int tid = 0;
	#pragma omp for
	for (vertex_t i = 0; i < vert_count; ++i) {
		degree = data->beg_pos[i + 1] - data->beg_pos[i];
		l_adj_list = data->adj_list + data->beg_pos[i];
		adj_list[i].degree = degree;
		
		if (!sorted) sort(l_adj_list, l_adj_list + degree);
		
		if (degree <= kinline_keys) {
			for (degree_t j = 0; j < degree; ++j) {
				adj_list[i].btree.inplace_keys[j] = l_adj_list[j];
			}
		} else if (degree <= kleaf_keys) {
			//adj_list[i].btree.leaf_node = (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
			adj_list[i].btree.leaf_node = alloc_leaf();
			adj_list[i].btree.leaf_node->count = degree;
			memcpy(adj_list[i].btree.leaf_node->keys, 
				   l_adj_list, 
				   degree*sizeof(vertex_t));
			
		} else {
            leaf_count = degree/kleaf_keys + (0 != degree % kleaf_keys);
			level1_count = leaf_count/kinner_values + (0 != leaf_count % kinner_values);
			
			//kinner_node_t* inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			kinner_node_t* inner_node = alloc_inner();
			udata.adj_list[i].btree.inner_node = inner_node;
			
			degree_t remaining = degree;
			int remaining_leaf = leaf_count;
			degree_t count = kleaf_keys;
			int inner_count = kinner_keys;
			kinner_node_t* prev = 0;
			kleaf_node_t* leaf_node = 0;
			
			for (int j = 0; j < level1_count; ++j) {
				inner_count = min(kinner_keys, remaining_leaf);
				remaining_leaf -= inner_count;
				inner_node->count = inner_count;
				inner_node->level = 1;
			
				for (int k = 0; k < inner_count; ++k) {
					//leaf_node = (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
					leaf_node = alloc_leaf();
					count = min(kleaf_keys, remaining);
					remaining -= count;
					
					leaf_node->count  = count;
					leaf_node->sorted = 1;
					
					memcpy(leaf_node->keys,
						   l_adj_list, count*sizeof(vertex_t));

					inner_node->values[k] = leaf_node;
					inner_node->keys[k] = leaf_node->keys[0];
					
					l_adj_list += count;
				}

				prev = inner_node;
                if (j != level1_count) {
                    inner_node = alloc_inner();
				    prev->next = inner_node;
                } else {
			        prev->next = 0;
                }
			}
		}
	}
	}
    //free(data->adj_list);
}

kleaf_node_t* ugraph_t::alloc_leaf()
{
    int tid = omp_get_thread_num();
    
    if (mem_info[tid].leaf_count == mem_count) {
        mem_info[tid].leaf_node_list = (kleaf_node_t*)calloc(
               mem_count, sizeof(kleaf_node_t));
        mem_info[tid].leaf_count = 0;
    }

    kleaf_node_t* leaf_node = mem_info[tid].leaf_node_list
                            + mem_info[tid].leaf_count;
    mem_info[tid].leaf_count += 1;
    return leaf_node;
}

kinner_node_t* ugraph_t::alloc_inner()
{
    int tid = omp_get_thread_num();

    if (mem_info[tid].inner_count == mem_count) {
        mem_info[tid].inner_node_list = (kinner_node_t*)calloc(
                mem_count, sizeof(kinner_node_t));
        mem_info[tid].inner_count = 0;
    }

    kinner_node_t* inner_node = mem_info[tid].inner_node_list
                                    + mem_info[tid].inner_count;
    mem_info[tid].inner_count += 1;

    return inner_node;
}


//It will not do a deep copy, memory will be used.
void
ugraph_t::init_from_csr2(csr_t* data, int sorted)
{
	vertex_t		vert_count = data->vert_count;
	adj_list_t*		adj_list = (adj_list_t*)malloc(sizeof(adj_list_t)*vert_count);
	
	udata.vert_count	= vert_count;
	udata.adj_list		= adj_list;

	#pragma omp parallel
	{
	vertex_t degree = 0;
	int leaf_count = 0;
	vertex_t* l_adj_list = 0;
	int level1_count = 0;
	#pragma omp for
	for (vertex_t i = 0; i < vert_count; ++i) {
		degree = data->beg_pos[i + 1] - data->beg_pos[i];
		l_adj_list = data->adj_list + data->beg_pos[i];
		adj_list[i].degree = degree;
		
		if (!sorted) sort(l_adj_list, l_adj_list + degree);
		
		if (degree <= kinline_keys) {
			for (degree_t j = 0; j < degree; ++j) {
				adj_list[i].btree.inplace_keys[j] = l_adj_list[j];
			}
		} else if (degree <= kleaf_keys) {
			adj_list[i].btree.leaf_node = (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
			adj_list[i].btree.leaf_node->count = degree;
			memcpy(adj_list[i].btree.leaf_node->keys, 
				   l_adj_list, 
				   degree*sizeof(vertex_t));
			
		} else {
			leaf_count = degree/kleaf_keys + (0 != degree % kleaf_keys);
			level1_count = leaf_count/kinner_values + (0 != leaf_count % kinner_values);
			
			kinner_node_t* inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			udata.adj_list[i].btree.inner_node = inner_node;
			
			degree_t remaining = degree;
			int remaining_leaf = leaf_count;
			degree_t count = kleaf_keys;
			int inner_count = kinner_keys;
			kinner_node_t* prev = 0;
			kleaf_node_t* leaf_node = 0;
			
			for (int j = 0; j < level1_count; ++j) {
				inner_count = min(kinner_keys, remaining_leaf);
				remaining_leaf -= inner_count;
				inner_node->count = inner_count;
				inner_node->level = 1;
			
				for (int k = 0; k < inner_count; ++k) {
				
					leaf_node = (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
					count = min(kleaf_keys, remaining);
					remaining -= count;
					
					leaf_node->count  = count;
					leaf_node->sorted = 1;
					
					memcpy(leaf_node->keys,
						   l_adj_list, count*sizeof(vertex_t));

					inner_node->values[k] = leaf_node;
					inner_node->keys[k] = leaf_node->keys[0];
					
					l_adj_list += count;
				}

				prev = inner_node;
				inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
				prev->next = inner_node;
			}

			//delete last allocation and the link
			free(inner_node);
			prev->next = 0;
		}

		//Make higher node
		degree_t level_count = level1_count;
		while (level_count > kinner_values) {
			degree_t remaining = level_count;
			level_count = level_count/kinner_values + (0 != level_count % kinner_values);
			kinner_node_t* tmp_inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			kinner_node_t* inner_node	  = udata.adj_list[i].btree.inner_node;
			int			   level		  = inner_node->level + 1; 
			
			udata.adj_list[i].btree.inner_node = tmp_inner_node;
			
			kinner_node_t* prev = 0;
			degree_t inner_count = kinner_keys;	
			
			for (int j = 0; j < level_count; ++j) {
				inner_count = min(kinner_keys, remaining);
				remaining -= inner_count;

				tmp_inner_node->count = inner_count;
				tmp_inner_node->level = level;
			
				for (int k = 0; k < inner_count; ++k) {
					tmp_inner_node->values[k] = inner_node;
					tmp_inner_node->keys[k] = inner_node->keys[0];
					inner_node = inner_node->next;	
				}

				prev = tmp_inner_node;
				tmp_inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
				prev->next = tmp_inner_node;
			}

			//delete last allocation and the link
			free(tmp_inner_node);
			prev->next = 0;
		}
	}
	}
}

void
ugraph_t::bfs(vertex_t root)
{
	vertex_t	vert_count = udata.vert_count;
	adj_list_t* adj_list   = udata.adj_list;
	index_t		edge_count = (1<< 28);
	
	uint8_t* status = (uint8_t*)calloc(sizeof(uint8_t), vert_count); //XXX
	//default status = INF
	
	int				level = 1;
	int				top_down = 1;
	vertex_t		frontier = 0;
	index_t			todo = 0;

	status[root] = 1;
	
	do {
		frontier = 0;
		todo = 0;
		double start = mywtime();
		if (top_down) {
			#pragma omp parallel num_threads(24) reduction (+:todo) reduction(+:frontier)
            {
            kinner_node_t*	inner_node = 0;
            vertex_t*		nebrs   = 0; 
	        vertex_t	    degree	= 0;
            int				count   = 0;
            #pragma omp for schedule (static)
			for (vertex_t v = 0; v < udata.vert_count; ++v) {
				if (status[v] != level) continue;
		
				//based on degree, we need to take alternate paths
				degree = adj_list[v].degree;
				todo += degree;
			
				if (degree <= kinline_keys) {//Path 1:
					vertex_t* nebrs = adj_list[v].btree.inplace_keys;
					count = degree;
					for (int j = 0; j < count; ++j) {
						if (status[nebrs[j]] == 0) {
							status[nebrs[j]] = level + 1;
							++frontier;
						}
					}

				} else if (degree <= kleaf_keys) {//Path 2;
					nebrs = adj_list[v].btree.leaf_node->keys;
					count = adj_list[v].btree.leaf_node->count;
					for (int j = 0; j < count; ++j) {
						if (status[nebrs[j]] == 0) {
							status[nebrs[j]] = level + 1;
							++frontier;
						}
					}

				} else {//Path 3:
					inner_node = udata.adj_list[v].btree.inner_node;
					while (inner_node) {
						for (int i = 0; i < inner_node->count; ++i) {
							nebrs = ((kleaf_node_t*)inner_node->values[i])->keys;
							count = ((kleaf_node_t*)inner_node->values[i])->count;
							for (int j = 0; j < count; ++j) {
								if (status[nebrs[j]] == 0 ) {
									status[nebrs[j]] = level + 1;
									++frontier;
								}
							}
						}
						inner_node = inner_node->next;
					}
				}
			}
            }
		} else { //bottom up
			#pragma omp parallel num_threads(24) reduction (+:todo) reduction(+:frontier) 
            {
            kinner_node_t*	inner_node = 0;
            vertex_t*		nebrs = 0; 
	        vertex_t	degree	   = 0;
            int				count = 0;
            #pragma omp for schedule (static)
			for (vertex_t v = 0; v < udata.vert_count; ++v) {
				if (status[v] != 0) continue;
		
				//based on degree, we need to take alternate paths
				degree = adj_list[v].degree;
				todo += degree;
			
				if (degree <= kinline_keys) {//Path 1:
					vertex_t* nebrs = adj_list[v].btree.inplace_keys;
					count = degree;
					for (int j = 0; j < count; ++j) {
						if (status[nebrs[j]] == level) {
							status[v] = level + 1;
							++frontier;
							break;

						}
					}

				} else if (degree <= kleaf_keys) {//Path 2;
					nebrs = adj_list[v].btree.leaf_node->keys;
					count = adj_list[v].btree.leaf_node->count;
					for (int j = 0; j < count; ++j) {
						if (status[nebrs[j]] == level) {
							status[v] = level + 1;
							++frontier;
							break;
						}
					}

				} else {//Path 3:
					inner_node = udata.adj_list[v].btree.inner_node;
					while (inner_node) {
						for (int i = 0; i < inner_node->count; ++i) {
							nebrs = ((kleaf_node_t*)inner_node->values[i])->keys;
							count = ((kleaf_node_t*)inner_node->values[i])->count;
							for (int j = 0; j < count; ++j) {
								if (status[nebrs[j]] == 0 ) {
									status[v] = level + 1;
									++frontier;
									break;
								}
							}
						}
						inner_node = inner_node->next;
					}
				}
			}
		}
        }
		double end = mywtime();
	
		cout << " Top down = " << top_down << "\t";
		cout << " Level = " << level;
		cout << " Time = " << end - start << "\t";
        cout << " Frontier Count = " << frontier << "\t";
        cout << " ToDo = " << todo;
		cout << endl;
		
		if (todo >= 0.03*edge_count || level >= 2) {
			top_down = false;
		}
		++level;
	} while(frontier);
}

void
ugraph_t::pagerank_async(int iteration_count)
{
	vertex_t	vert_count = udata.vert_count;
	adj_list_t* adj_list   = udata.adj_list;
	rank_t		inv_v_count= 1.0f/vert_count;
	rank_t*		pr		   = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	rank_t*		inv_degree = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	
	for (vertex_t v = 0; v < udata.vert_count; ++v) {
		pr[v] = inv_v_count;
		if (udata.adj_list[v].degree != 0) {
			inv_degree[v] = 1.0f/udata.adj_list[v].degree;
		}
	}

	
	for (int iter_count = 0; iter_count < iteration_count; ++iter_count) {
        #pragma omp parallel num_threads(NUM_THDS)
        {
        kinner_node_t*	inner_node = 0;
        rank_t			rank    = 0.0;
        vertex_t*		nebrs   = 0;
        int				count   = 0;
        vertex_t	    degree  = 0;
        #pragma omp for schedule (static) 
		for (vertex_t v = 0; v < vert_count; ++v) {
			degree = adj_list[v].degree;
			
			//based on degree, we need to take alternate paths
			
			if (degree <= kinline_keys) {//Path 1:
				nebrs = adj_list[v].btree.inplace_keys;
				count = degree;
				for (int j = 0; j < count; ++j) {
					rank += pr[nebrs[j]];
				}

			} else if (degree <= kleaf_keys) {//Path 2;
				nebrs = adj_list[v].btree.leaf_node->keys;
				count = adj_list[v].btree.leaf_node->count;
				for (int j = 0; j < count; ++j) {
					rank += pr[nebrs[j]];
				}

			} else {//Path 3:
				inner_node = udata.adj_list[v].btree.inner_node;
				while (inner_node) {
					for (int i = 0; i < inner_node->count; ++i) {
						nebrs = ((kleaf_node_t*)inner_node->values[i])->keys;
						count = ((kleaf_node_t*)inner_node->values[i])->count;
						for (int j = 0; j < count; ++j) {
							rank += pr[nebrs[j]];
						}
					}
					inner_node = inner_node->next;
				}
			}

			//end path

			//if (iter_count != iteration_count - 1) {
				rank = (0.15 + 0.85*rank)*inv_degree[v];
			//} else {
			//	rank = (0.15 + 0.85*rank);
			//}
			pr[v] = rank;//XXX
		}
        }
	}
	cout << "PR[0] = " << pr[0] << endl;
}

void
ugraph_t::pagerank(int iteration_count)
{
	vertex_t	vert_count = udata.vert_count;
	adj_list_t* adj_list   = udata.adj_list;
	rank_t		inv_v_count= 1.0f/vert_count;
	rank_t*		pr		   = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	rank_t*		prior_pr		   = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	//rank_t*		inv_degree = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	
	for (vertex_t v = 0; v < udata.vert_count; ++v) {
		prior_pr[v] = inv_v_count;
		/*if (udata.adj_list[v].degree != 0) {
			inv_degree[v] = 1.0f/udata.adj_list[v].degree;
		}*/
	}

	
	for (int iter_count = 0; iter_count < iteration_count; ++iter_count) {
        double start = mywtime();
        #pragma omp parallel num_threads(NUM_THDS)
        {
        kinner_node_t*	inner_node = 0;
        rank_t			rank    = 0.0f;
        rank_t      inv_degree  = 0.0f;
        vertex_t*		nebrs   = 0;
        int				count   = 0;
        vertex_t	    degree  = 0;
        #pragma omp for schedule (static) 
		for (vertex_t v = 0; v < vert_count; ++v) {
            
			degree = adj_list[v].degree;
            if (degree == 0) continue;
            inv_degree = 1.0f/degree;
			
			//based on degree, we need to take alternate paths
			
			if (degree <= kinline_keys) {//Path 1:
				nebrs = adj_list[v].btree.inplace_keys;
				count = degree;
				for (int j = 0; j < count; ++j) {
					rank += prior_pr[nebrs[j]];
				}

			} else if (degree <= kleaf_keys) {//Path 2;
				nebrs = adj_list[v].btree.leaf_node->keys;
				count = adj_list[v].btree.leaf_node->count;
				for (int j = 0; j < count; ++j) {
					rank += prior_pr[nebrs[j]];
				}

			} else {//Path 3:
				inner_node = udata.adj_list[v].btree.inner_node;
				while (inner_node) {
					for (int i = 0; i < inner_node->count; ++i) {
						nebrs = ((kleaf_node_t*)inner_node->values[i])->keys;
						count = ((kleaf_node_t*)inner_node->values[i])->count;
						for (int j = 0; j < count; ++j) {
							rank += prior_pr[nebrs[j]];
						}
					}
					inner_node = inner_node->next;
				}
			}

			//end path

			//if (iter_count != iteration_count - 1) {
				//rank = (0.15 + 0.85*rank)*inv_degree[v];
				rank = (0.15 + 0.85*rank)*inv_degree;
			//} else {
			//	rank = (0.15 + 0.85*rank);
			//}
			pr[v] = rank;//XXX
		}
        }
        swap(pr, prior_pr);
        double end = mywtime();
        cout << "Iteration Time = " << end - start << endl;
	}
	cout << "PR[0] = " << pr[0] << endl;
}

int main(int argc, char* argv[])
{
	g = new ugraph_t;
	g->init(argc, argv);
	return 0;
}

void ugraph_t::init(int argc, char* argv[])
{
    int o;
    int job = 0;
    uint32_t scale;
    int c = 0;
    string inputfile;
	vertex_t vert_count;
	int arg = -1;
    
	while ((o = getopt (argc, argv, "s:o:hi:j:c:m:v:a:")) != -1) {
        switch(o) {
            case 's': //scale
                scale = atoi(optarg);
                vert_count = (1L << scale);
                break;
            case 'v'://vert count
				sscanf(optarg, "%d", &vert_count);
                break;
            case 'i':
                inputfile = optarg;
                break;
            case 'j':
                job = atoi(optarg);
                break;
            case 'h':
                cout << "Coming soon" << endl;
                return;
            case 'a':
                arg = atoi(optarg);
                break;
            default:
               assert(0); 
        }
    }

    #pragma omp parallel num_threads(NUM_THDS)
    {
        int tid = omp_get_thread_num();
        mem_info[tid].inner_node_list = (kinner_node_t*)calloc(sizeof(kinner_node_t), 
                                                               mem_count);
        mem_info[tid].inner_count = 0;
        mem_info[tid].leaf_node_list = (kleaf_node_t*)calloc(sizeof(kleaf_node_t), 
                                                             mem_count);
        mem_info[tid].leaf_count = 0;
    }
    
	csr_t data;
    double start, end;
    start = mywtime();
	csr_from_file(inputfile, vert_count, &data);
    end = mywtime();
    cout << "Reading time = " << end - start << endl;
    start = mywtime();
	init_from_csr(&data, true);
    end = mywtime();
    cout << "Conversion time = " << end - start << endl;
	index_t tc_count = 0;
  
    switch(job) {
    case 0:
            start = mywtime();
            bfs(arg);
            end = mywtime();
            cout << "BFS time = " << end-start << endl;
            break;    
    case 1:
            start = mywtime();
            pagerank(arg);
            end = mywtime();
            cout << "PageRank time = " << end-start << endl;
            break;    
    case 2:
            start = mywtime();
            pagerank_async(arg);
            end = mywtime();
            cout << "PageRank time = " << end-start << endl;
            break;    
    case 3:
            start = mywtime();
            tc_count = tc();
            end = mywtime();
            cout << "TC time = " << end-start << endl;
            cout << "TC Count = " << tc_count << endl;
            break;    
    default:
            assert(0);
    }
	return ;
}

index_t ugraph_t::tc()
{
	vertex_t vert_count = udata.vert_count;
	adj_list_t* adj_list   = udata.adj_list;
	
	vertex_t v1, v2;
	degree_t degree;
	kinner_node_t*	inner_node = 0;
	vertex_t*		nebrs = 0;
	int				count = 0;
	index_t			tc_count = 0;

	for(vertex_t v = 0; v < vert_count; ++v) {
		degree = udata.adj_list[v].degree;
		if (degree <= kinline_keys) {//Path 1:
			nebrs = adj_list[v].btree.inplace_keys;
			count = degree;
			for (int j = 0; j < count; ++j) {
				v1 = v;
				v2 = nebrs[j];
				tc_count += intersection(v1, v2);
			}

		} else if (degree <= kleaf_keys) {//Path 2;
			nebrs = adj_list[v].btree.leaf_node->keys;
			count = adj_list[v].btree.leaf_node->count;
			for (int j = 0; j < count; ++j) {
				v1 = v;
				v2 = nebrs[j];
				tc_count += intersection(v1, v2);
			}

		} else {//Path 3:
			inner_node = udata.adj_list[v].btree.inner_node;
			while (inner_node) {
				for (int i = 0; i < inner_node->count; ++i) {
					nebrs = ((kleaf_node_t*)inner_node->values[i])->keys;
					count = ((kleaf_node_t*)inner_node->values[i])->count;
					for (int j = 0; j < count; ++j) {
						v1 = v;
						v2 = nebrs[j];
						tc_count += intersection(v1, v2);

					}
				}
				inner_node = inner_node->next;
			}
		}
	}
	return tc_count;
}

index_t ugraph_t::intersection(vertex_t v1, vertex_t v2)
{

	return 1;
}
