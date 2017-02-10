#include <omp.h>
#include <algorithm>
#include <iostream>
#include "graph.h"

using std::cout;
using std::endl;

using std::sort;
using std::min;

//It will not do a deep copy, memory will be used.
void
ugraph_t::init_from_csr(csr_t* data, int sorted)
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
		if (!sorted) sort(l_adj_list, l_adj_list + degree);
		if (degree <= kinline_keys) {
			for (degree_t j = 0; j < degree; ++j) {
				adj_list[i].btree.inplace_keys[j] = l_adj_list[j];
			}
		} else if (degree <= kleaf_keys) {
			adj_list[i].btree.leaf_node = (kleaf_node_t*)l_adj_list;
		} else {
			leaf_count = degree/kleaf_keys; + (0 != degree % kleaf_keys);
			level1_count = leaf_count/kinner_values + (0 != leaf_count/kinner_values);
			kleaf_node_t* leaf_node = (kleaf_node_t*)l_adj_list;
			kinner_node_t* inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			udata.adj_list[i].btree.inner_node = inner_node;
			kinner_node_t* prev = inner_node;
			for (int j = 0; j < level1_count; ++j) {
				for (int k = 0; k < leaf_count; ++k) {
					inner_node->values[k] = leaf_node;
					inner_node->keys[k] = leaf_node->keys[0];
					leaf_node += kleaf_keys;
				}
				prev = inner_node;
				inner_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
				prev->next = inner_node;
			}
			//delete last allocation and the link
			free(inner_node);
			prev->next = 0;
		}
	}
	}
}
void
ugraph_t::bfs(vertex_t root)
{
	vertex_t	vert_count = udata.vert_count;
	vertex_t	degree	   = 0;
	adj_list_t* adj_list   = udata.adj_list;
	
	int* status = 0; //XXX
	//default status = INF
	
	kinner_node_t*	inner_node = 0;
	kleaf_node_t*	leaf_node = 0;
	vertex_t*		nebrs = 0; 
	int				level = 0;
	int				count = 0;
	vertex_t		frontier = 0;
	
	while(frontier) {
		frontier = 0;

		for (vertex_t v = 0; v < udata.vert_count; ++v) {
			if (status[v] == level) continue;
	
			//based on degree, we need to take alternate paths
			degree = adj_list[v].degree;
		
			if (degree <= kinline_keys) {//Path 1:
				vertex_t* nebrs = adj_list[v].btree.inplace_keys;
				count = degree;
				for (int j = 0; j < count; ++j) {
					if (status[nebrs[j]] == level) {
						status[nebrs[j]] = level + 1;
						++frontier;
					}
				}

			} else if (degree <= kleaf_keys) {//Path 2;
				nebrs = adj_list[v].btree.leaf_node->keys;
				count = adj_list[v].btree.leaf_node->count;
				for (int j = 0; j < leaf_node->count; ++j) {
					if (status[nebrs[j]] == level) {
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
						for (int j = 0; j < leaf_node->count; ++j) {
							if (status[nebrs[j]] == level) {
								status[nebrs[j]] = level + 1;
								++frontier;
							}
						}
					}
					inner_node = inner_node->next;
				}
			}
			//end path
			cout << "Level Count = " << level << " Frontier Count = " << frontier << endl;

		}
	}
}


void
ugraph_t::pagerank(int iteration_count)
{
	vertex_t	vert_count = udata.vert_count;
	vertex_t	degree	   = 0;
	adj_list_t* adj_list   = udata.adj_list;
	rank_t*		pr		   = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	rank_t*		inv_degree = (rank_t*)malloc(sizeof(rank_t)*vert_count);
	
	for (vertex_t v = 0; v < udata.vert_count; ++v) {
		if (udata.adj_list[v].degree != 0) {
			inv_degree[v] = 1.0f/udata.adj_list[v].degree;
		}
	}

	kinner_node_t*	inner_node = 0;
	kleaf_node_t*	leaf_node = 0;
	rank_t			rank = 0.0;
	vertex_t*		nebrs = 0;
	int				count = 0;
	
	for (int iter_count = 0; iter_count < iteration_count; ++iter_count) {
		for (vertex_t v = 0; v < udata.vert_count; ++v) {
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

			if (iter_count != iteration_count - 1) {
				rank = (0.15 + 0.85*rank)*inv_degree[v];
			} else {
				rank = (0.15 + 0.85*rank);
			}
			pr[v] = rank;//XXX
		}
	}
}

int main(int argc, char* argv[])
{
	return 0;
}
