#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "edges.h"



status_t 
labeled_edges_t::initial_setup(degree_t a_degree)
{
	degree = a_degree;
	return 0;
}

kinner_node_t*
create_higher_inner_node(kinner_node_t** inner_node, int count, int level)
{
	int level_count = 1; 
	if (count > kinner_values) {
		level_count = count/kinner_values + (0!= level_count % kinner_values);
	}
		
	kinner_node_t* tmp_node = 0;

	if (level_count > 1) {
		kinner_node_t** higher_inner_node = (kinner_node_t**)calloc(
							sizeof(kinner_node_t*), level_count); 
		int k = 0;
		for (int i = 0; i < level_count - 1; ++i) {
			tmp_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			tmp_node->level = level;
			for (int j = 0; j < kinner_values; ++j) {
				tmp_node->values[j] = (void*)inner_node[k++];
			}
			higher_inner_node[i] = tmp_node;
		}
		//last higher node.
		tmp_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
		tmp_node->level = level;
		for (int j = 0; k < count; ++k, ++j) {
			tmp_node->values[j] = (void*)inner_node[k];
		}
		higher_inner_node[level_count - 1] = tmp_node;

		//XXX
		tmp_node = create_higher_inner_node(higher_inner_node, level_count, level+1);
		free(higher_inner_node);
		return tmp_node;

	} else {//just one node
		tmp_node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
		tmp_node->level = level;
		for (int k = 0; k < count; ++k) {
			tmp_node->values[k] = (void*)inner_node[k];
		}
		return tmp_node;	
	}
}

status_t 
kbtree_t::initial_setup(degree_t a_degree)
{
	degree = a_degree;
	if (degree <=2 ) return 0;
	else if (degree <= kleaf_keys) {
		btree.leaf_node = (kleaf_node_t*)calloc(sizeof(kleaf_node_t), degree);
		return 0;
	}

	//Degree count decides the level of b-tree.
	int leafnode_count = a_degree/kleaf_keys + (0!= (a_degree % kleaf_keys));
	//edges_tree = (kbtree_t*)malloc(sizeof(kbtree_t));
	kinner_node_t* inner_node = 0;
	
	int level = 1;	
	
	int level_count = 1;
	if (leafnode_count > kinner_values) {
		level_count = level_count/kinner_values 
				+ (0 != level_count % kinner_values);
	}
	kinner_node_t* node = 0; 

	if (level_count > 1) {
		kinner_node_t** higher_inner_node = (kinner_node_t**)calloc(
							sizeof(kinner_node_t*), level_count);

		for (int m = 0; m < level_count -1; ++m) {
			node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
			node->level = level;
			for (int n = 0; n < leafnode_count; ++n) {
				node->values[n] = malloc(sizeof(kleaf_node_t));
			}
			higher_inner_node[m] = node; 
		}
		//last higher node
		node = (kinner_node_t*)malloc( sizeof(kinner_node_t));
		node->level = level;
		for (int n = 0; n < leafnode_count; ++n) {
			node->values[n] = malloc(sizeof(kleaf_node_t));
		}
		higher_inner_node[level_count-1] = node;
		
		//XXX
		inner_node = create_higher_inner_node(higher_inner_node, level_count, level+1);
		free(higher_inner_node);
	} else {
		kinner_node_t* node = (kinner_node_t*)malloc(sizeof(kinner_node_t));
		node->level = level;
		for (int n = 0; n < leafnode_count; ++n) {
			node->values[n] = malloc(sizeof(kleaf_node_t));
		}
		inner_node = node;
	}
		
	btree.inner_node = inner_node;
	//btree.leaf_node_node = 0;

	return 0;
}

status_t 
kbtree_t::split_leaf(kleaf_node_t* leaf_node1, key_t key, split_info_t* split_info)
{
	int				count		= leaf_node1->count;
	kleaf_node_t*	leaf_node2	= (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
	
	leaf_node2->sorted = 1;
	
	//assert(leaf_node1->count == count);
	
	int i = 0;
	while (i < count && key > leaf_node1->keys[i]) {
		++i;
	}

	//Lets break the first leaf in half
	int half = (count >> 1);
	if ( i <= half) { //If new key in the first half
		memcpy(leaf_node2->keys, leaf_node1->keys + half, (count - half)*sizeof(key_t));
		leaf_node2->count = count - half;

		memmove(leaf_node1->keys + i+1, leaf_node1->keys + i, (half - i)*sizeof(key_t));
		leaf_node1->keys[i] = key;
		leaf_node1->count = half + 1;

	} else {//new key in second half
		memcpy(leaf_node2->keys, leaf_node1->keys + half, (i-half)*sizeof(key_t));
		leaf_node2->keys[i-half] = key;
		memcpy(leaf_node2->keys + i - half + 1, leaf_node1->keys + i, (count - i)*sizeof(key_t));
		leaf_node2->count = count - half + 1;
		
		leaf_node1->count = half;
		
	}

	//establish the sibling relationship
	leaf_node2->next = leaf_node1->next;
	leaf_node1->next = leaf_node2;

	split_info->key = leaf_node2->keys[0];
	split_info->value = leaf_node2;

	return 0;
}

void 
kbtree_t::insert_in_leaf(kleaf_node_t* leaf_node1, key_t key)
{
	int		count = leaf_node1->count;
	int		i	  = 0;
	assert(count < kleaf_keys);
	
	while (i < count && key > leaf_node1->keys[i]) {
		++i;
	}
	memmove(leaf_node1 + i + 1, leaf_node1 + i, (count - i )*sizeof(key_t));
	leaf_node1->keys[i] = key;
	leaf_node1->count += 1;

	return;
}

void kbtree_t::insert_inplace(key_t key)
{	int count = degree;
	int i	  = 0;
	assert(count < kinplace_keys);

	while (i < count && key > inplace_keys[i]) {
		++i;
	}
	memmove(inplace_keys + i + 1, inplace_keys + i, (count - i)*sizeof(key_t));
	inplace_keys[i] = key;

	//increment will be done in caller function.
	return ;
}

//second argument is in/out both
status_t
kbtree_t::split_innernode(kinner_node_t* inner_node1, int i, split_info_t* split_info)
{
	kinner_node_t* inner_node2 = (kinner_node_t*)malloc(sizeof(inner_node_t));
	int count = inner_node2->count;
	key_t key = split_info->key;
	void* value = split_info->value;
	
	//Lets break the first leaf in half
	int half = (count >> 1);
	if (i <= half) { //If new key in the first half
		memcpy(inner_node2->keys, inner_node1->keys + half, (count - half)*sizeof(key_t));
		memcpy(inner_node2->values, inner_node1->values + half, (count + 1 - half)*sizeof(void*));
		inner_node2->count = count - half;

		memmove(inner_node1->keys + i+1, inner_node1->keys + i, (half - i)*sizeof(key_t));
		inner_node1->keys[i] = key;
		inner_node1->values[i] = value;
		inner_node1->count = half + 1;

	} else {//new key in second half
		memcpy(inner_node2->keys, inner_node1->keys + half, (i-half)*sizeof(key_t));
		memcpy(inner_node2->values, inner_node1->values + half, (i-half)*sizeof(void*));
		inner_node2->keys[i-half] = key;
		inner_node2->values[i-half] = value;
		memcpy(inner_node2->keys + i - half + 1, inner_node1->keys + i, (count - i)*sizeof(key_t));
		memcpy(inner_node2->values + i - half + 1, inner_node1->values + i, (count - i)*sizeof(void*));
		inner_node2->count = count - half + 1;
		
		inner_node1->count = half;
		
	}

	return 0;
}

typedef struct __traverse_info_t {
	void* inner_node;
	int i;
} traverse_info;

status_t 
kbtree_t::insert_traverse(kinner_node_t* root, key_t key)
{
	kinner_node_t*	tmp_inner_node = root;
	traverse_info	kstack[8];

	int		level	= tmp_inner_node->level;
	int		top		= 0;
	int		i		= 0;

	while (level >= 1) {
		while (i < tmp_inner_node->count && key > tmp_inner_node->keys[i]) {
			++i;
		}

		kstack[top].inner_node = tmp_inner_node;
		kstack[top].i = i;
		tmp_inner_node = (kinner_node_t*)tmp_inner_node->values[i];
		++top;
		--level;
		i = 0;
	}
	
	//Now we have leaf node with us.
	kleaf_node_t* leaf_node = (kleaf_node_t*)(tmp_inner_node);
	int			  key_count = leaf_node->count;
	
	if (key_count == kleaf_keys) {
		split_info_t split_info;
		int count = 0;
		split_leaf(leaf_node, key, &split_info);
		
		//XXX
		while (top > 0) {
			--top;
			tmp_inner_node = (kinner_node_t*)kstack[top].inner_node;
			i = kstack[top].i;
			count = tmp_inner_node->count;
			if (count == kinner_keys) { //split this higher node as well.
				split_innernode(tmp_inner_node, i, &split_info);

			} else { //insert the new key in the higher node
				memmove(tmp_inner_node->keys + i + 1, tmp_inner_node->keys + i, (count - i)*sizeof(key_t));
				tmp_inner_node->keys[i] = split_info.key;
				tmp_inner_node->values[i] = split_info.value;
				return 0;
			}
		}
		//If we did not return above, we need to create an additional level now
		kinner_node_t* new_root = (kinner_node_t*) malloc(sizeof(kinner_node_t));
		
		new_root->count = 1;
		new_root->level = 1;
		new_root->keys[0] = split_info.key;
		new_root->values[0] = btree.inner_node;
		new_root->values[1] = split_info.value;
		
		//set up the hihger node pointers
		btree.inner_node = new_root;
		return 0;

	} else {
		insert_in_leaf(leaf_node, key);
	}
	return 0;
}

status_t 
kbtree_t::split_inplace(key_t key) 
{
	int i = 0;
	while(i < degree && key > inplace_keys[i]) {
		++i;
	}
	if (i == degree && key == inplace_keys[i]) {
		return keyExist;
	}
	
	kleaf_node_t* leaf_node = (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
	leaf_node->count = degree + 1;
	leaf_node->sorted = 1;
	memcpy(leaf_node->keys, inplace_keys, i*sizeof(key_t));
	leaf_node->keys[i] = key;
	memcpy(leaf_node->keys + i + 1, inplace_keys + i, (degree - i)*sizeof(key_t));
	btree.leaf_node = leaf_node;
	return 0;
}

status_t 
kbtree_t::insert(key_t key)
{
	int count = degree;
	++degree;

	if (count < inplace_knode) {
		insert_inplace(key);
	} else if (count == inplace_knode) {
		split_inplace(key);
	} else if (count < kleaf_keys) {
		insert_in_leaf(btree.leaf_node, key);
	} else if (count == kleaf_keys) {
		split_info_t split_info;
		split_leaf(btree.leaf_node, key, &split_info);
	
		//setup the higher level keys and pointers
		kinner_node_t* tmp_inner_node = (kinner_node_t*) malloc(sizeof(kinner_node_t));
		
		tmp_inner_node->count = 1;
		tmp_inner_node->level = 1;
		tmp_inner_node->keys[0] = split_info.key;
		tmp_inner_node->values[0] = btree.leaf_node;
		tmp_inner_node->values[1] = split_info.value;
		
		//set up the hihger node pointers
		btree.inner_node = tmp_inner_node;
	
	} else {//Some hihger level node already exists
		insert_traverse(btree.inner_node, key);
	}

	return 0;
}
    
//non-zero return means found.
int 
kbtree_t::search(key_t key)
{
	int count = degree;
	int		i = 0;

	if (count <= inplace_knode) {
		while (i < count && key > inplace_keys[i]) {
			++i;
		}
		return (key == inplace_keys[i]);

	} else if (count <= kleaf_keys) {
		//Now we have leaf node with us.
		kleaf_node_t* leaf_node = btree.leaf_node;
		int			  key_count = leaf_node->count;
		
		while (i < key_count && key > leaf_node->keys[i]) {
			++i;
		}
		return (key == leaf_node->keys[i]);
	} else {
		kinner_node_t*	tmp_inner_node = btree.inner_node;
		int		level	= tmp_inner_node->level;
		
		while (level >= 1) {
			while (i < tmp_inner_node->count && key > tmp_inner_node->keys[i]) {
				++i;
			}
			tmp_inner_node = (kinner_node_t*)tmp_inner_node->values[i];
			--level;
			i = 0;
		}
		
		//Now we have leaf node with us.
		kleaf_node_t* leaf_node = (kleaf_node_t*)(tmp_inner_node);
		int			  key_count = leaf_node->count;
		
		while (i < key_count && key > leaf_node->keys[i]) {
			++i;
		}
		return (key == leaf_node->keys[i]);
	}
}
/*--------------------------------------------------------------------------------*/
status_t 
btree_t::insert_inplace1(key_t key, value_t value)
{
	//First insertion
	assert(degree == 0);
	pair.key = key;
	pair.value = value;
	return 0;
}

status_t 
btree_t::insert_inplace2(key_t key, value_t value)
{
	assert(pair.key == key);
	if (0 == value_count) {
		if (pair.value == value) return 1;//duplicate entry
		values[0] = (kbtree_t*)malloc(sizeof(kbtree_t));
		values[0]->insert(pair.value);
		values[0]->insert(value);
		pair.value = 0;
		value_count = 1;
	} else if (1 == value_count) {
		return values[0]->insert(value);
	}
	return 0;
}

status_t
btree_t::insert_in_leaf1(leaf_node_t* leaf_node1, key_t key, value_t value)
{
	int		count = leaf_node1->count;
	int		i	  = 0;
	assert(count < leaf_keys);
	
	while (i < count && key > leaf_node1->keys[i]) {
		++i;
	}

	if ((i < count) && (key == leaf_node->keys[i])) {//key exists
		int vindex = 0;
		if (leaf_node1->flag & (1 << i)) {//more than one value is prsent
			vindex = leaf_node1->values[i];
			return values[vindex]->insert(value);
		} else { // only one value exist
			if (leaf_node1->values[i] == value) return keyvalueExist;

			vindex = value_count++;
			values[vindex]->insert(value);
			values[vindex]->insert(leaf_node1->values[i]);

			leaf_node1->flag |= (1 << i);
		}
		return keyExist;

	} else { //key doesnot exist
		memmove(leaf_node1->keys + i + 1, leaf_node1->keys + i, (count - i )*sizeof(key_t));
		memmove(leaf_node1->values + i + 1, leaf_node1->values + i, (count - i )*sizeof(value_t));
		
		leaf_node1->keys[i] = key;
		leaf_node1->values[i] = value;
		leaf_node1->count += 1;

		//Flag update
		leaf_node1->flag = (leaf_node1->flag & (~(1 << i) -1)) 
						      + (1 << i) 
							  + (leaf_node1->flag & ((1 << i) -1));
		return 0;
	}

}

status_t
btree_t::insert_in_leaf2(leaf_node_t* leaf_node1, key_t key, value_t value, split_info_t* split_info)
{
	int		count = leaf_node1->count;
	int		i	  = 0;
	assert(count == leaf_keys);
	
	while (i < count && key > leaf_node1->keys[i]) {
		++i;
	}
	if (i == count) {// key doesn't exist and split leaf
		split_leaf(leaf_node1, key, value, i, split_info);
		return splitLeaf;

	} else if (key == leaf_node->keys[i]) {//key exists
		int vindex = 0;
		if (leaf_node1->flag & (1 << i)) {//more than one value is prsent
			vindex = leaf_node1->values[i];
			return values[vindex]->insert(value);
		} else { // only one value exist
			if (leaf_node1->values[i] == value) return keyvalueExist;

			vindex = value_count++;
			values[vindex]->insert(value);
			values[vindex]->insert(leaf_node1->values[i]);

			leaf_node1->flag |= (1 << i);
		}
		return keyExist;
	
	} else { //key doesnot exist
		memmove(leaf_node1->keys + i + 1, leaf_node1->keys + i, (count - i )*sizeof(key_t));
		memmove(leaf_node1->values + i + 1, leaf_node1->values + i, (count - i )*sizeof(value_t));
		
		leaf_node1->keys[i] = key;
		leaf_node1->values[i] = value;
		leaf_node1->count += 1;

		//Flag update
		leaf_node->flag = (leaf_node1->flag & ~((1 << i) -1)) + 
							(1 << i) + 
							(leaf_node1->flag & ((1 << i) -1));
		return 0;
	}

	return 0;
}

status_t 
btree_t::insert(key_t key, value_t value)
{
	int count = degree;
	status_t ret = 0;

	if (count < inline_keys) {
		ret = insert_inplace1(key, value);
	} else if (count == inline_keys && pair.key == key ) {
		//same key but one more edge
		ret = insert_inplace2(key, value);
	} else if (count < leaf_keys) {
		ret = insert_in_leaf1(leaf_node, key, value);
	} else if (count == leaf_keys) {
		split_info_t split_info;
		ret = insert_in_leaf2(leaf_node, key, value, &split_info);
		if (ret == splitLeaf) {
			//setup the higher level keys and pointers
			inner_node_t* tmp_inner_node = (inner_node_t*) malloc(sizeof(inner_node_t));
			
			tmp_inner_node->count = 1;
			tmp_inner_node->level = 1;
			tmp_inner_node->keys[0] = split_info.key;
			tmp_inner_node->values[0] = leaf_node;
			tmp_inner_node->values[1] = split_info.value;
			
			//set up the hihger node pointers
			inner_node = tmp_inner_node;
			ret = 0;
		}
	} else { //Some hihger level node already exists
		ret = insert_traverse(inner_node, key, value);
	}
	
	degree += (ret == 0);

	return 0;
}
status_t 
btree_t::insert_traverse(inner_node_t* root, key_t key, value_t value)
{
	inner_node_t*	tmp_inner_node = root;
	traverse_info	kstack[8];

	int		level	= tmp_inner_node->level;
	int		top		= 0;
	int		i		= 0;
	int		ret		= 0;

	while (level >= 1) {
		while (i < tmp_inner_node->count && key > tmp_inner_node->keys[i]) {
			++i;
		}

		kstack[top].inner_node = tmp_inner_node;
		kstack[top].i = i;
		tmp_inner_node = (inner_node_t*)tmp_inner_node->values[i];
		++top;
		--level;
		i = 0;
	}
	
	//Now we have leaf node with us.
	leaf_node_t* leaf_node = (leaf_node_t*)(tmp_inner_node);
	int			  key_count = leaf_node->count;

	
	if (key_count == leaf_keys) {
		int count = 0;
		split_info_t split_info;
		ret = insert_in_leaf2(leaf_node, key, value, &split_info);
		if (ret != splitLeaf)	return 0;
		//XXX
		while (top > 0) {
			--top;
			tmp_inner_node = (inner_node_t*)kstack[top].inner_node;
			i = kstack[top].i;
			count = tmp_inner_node->count;
			if (count == inner_keys) { //split this higher node as well.
				split_innernode(tmp_inner_node, i, &split_info);

			} else { //insert the new key in the higher node
				memmove(tmp_inner_node->keys + i + 1, tmp_inner_node->keys + i, (count - i)*sizeof(key_t));
				tmp_inner_node->keys[i] = split_info.key;
				tmp_inner_node->values[i] = split_info.value;
				return 0;
			}
		}
		//If we did not return above, we need to create an additional level now
		inner_node_t* new_root = (inner_node_t*) malloc(sizeof(kinner_node_t));
		
		new_root->count = 1;
		new_root->level = 1;
		new_root->keys[0] = split_info.key;
		new_root->values[0] = inner_node;
		new_root->values[1] = split_info.value;
		
		//set up the hihger node pointers
		inner_node = new_root;
		return 0;

	} else {
		insert_in_leaf1(leaf_node, key, value);
	}
	return 0;
}

status_t 
btree_t::split_leaf(leaf_node_t* leaf_node1, key_t key, value_t value, int i, split_info_t* split_info)
{
	int				count		= leaf_node1->count;
	kleaf_node_t*	leaf_node2	= (kleaf_node_t*)malloc(sizeof(kleaf_node_t));
	
	leaf_node2->sorted = 1;
	
	//assert(leaf_node1->count == count);
	
	//Lets break the first leaf in half
	int half = (count >> 1);
	if ( i <= half) { //If new key in the first half
		memcpy(leaf_node2->keys, leaf_node1->keys + half, (count - half)*sizeof(key_t));
		leaf_node2->count = count - half;

		memmove(leaf_node1->keys + i+1, leaf_node1->keys + i, (half - i)*sizeof(key_t));
		leaf_node1->keys[i] = key;
		leaf_node1->count = half + 1;

	} else {//new key in second half
		memcpy(leaf_node2->keys, leaf_node1->keys + half, (i-half)*sizeof(key_t));
		leaf_node2->keys[i-half] = key;
		memcpy(leaf_node2->keys + i - half + 1, leaf_node1->keys + i, (count - i)*sizeof(key_t));
		leaf_node2->count = count - half + 1;
		
		leaf_node1->count = half;
		
	}

	//establish the sibling relationship
	//leaf_node2->next = leaf_node1->next;
	//leaf_node1->next = leaf_node2;

	split_info->key = leaf_node2->keys[0];
	split_info->value = leaf_node2;

	return 0;
}

status_t
btree_t::split_innernode(inner_node_t* inner_node1, int i, split_info_t* split_info)
{
	inner_node_t* inner_node2 = (inner_node_t*)malloc(sizeof(inner_node_t));
	int count = inner_node2->count;
	key_t key = split_info->key;
	void* value = split_info->value;
	
	//Lets break the first leaf in half
	int half = (count >> 1);
	if (i <= half) { //If new key in the first half
		memcpy(inner_node2->keys, inner_node1->keys + half, (count - half)*sizeof(key_t));
		memcpy(inner_node2->values, inner_node1->values + half, (count + 1 - half)*sizeof(void*));
		inner_node2->count = count - half;

		memmove(inner_node1->keys + i+1, inner_node1->keys + i, (half - i)*sizeof(key_t));
		inner_node1->keys[i] = key;
		inner_node1->values[i] = value;
		inner_node1->count = half + 1;

	} else {//new key in second half
		memcpy(inner_node2->keys, inner_node1->keys + half, (i-half)*sizeof(key_t));
		memcpy(inner_node2->values, inner_node1->values + half, (i-half)*sizeof(void*));
		inner_node2->keys[i-half] = key;
		inner_node2->values[i-half] = value;
		memcpy(inner_node2->keys + i - half + 1, inner_node1->keys + i, (count - i)*sizeof(key_t));
		memcpy(inner_node2->values + i - half + 1, inner_node1->values + i, (count - i)*sizeof(void*));
		inner_node2->count = count - half + 1;
		
		inner_node1->count = half;
		
	}

	return 0;
}
