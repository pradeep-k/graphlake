#include "query_triple.h"

status_t
query_triple::execute()
{
	return 0;
}

void
query_triple::to_string()
{
	cout << "(";
	query_node* node = get_child();
	if (node) {
		node->to_string();
	}
	cout << sub_id << " " << pred_id << " " << obj_id << ")" << endl;
	node = get_sibling();
	while (node) {
		node->to_string();
		node = node->get_sibling();
	}

}
