#include "graph.h"

#include "query_triple.h"
#include "query_triplewithfilter.h"


void test1()
{
    const char* src = "<http://www.Department10.University1.edu/UndergraduateStudent2>"; 
    //const char* pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#advisor>"; 
    const char* pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#takesCourse>"; 
    //const char* dst = "<http://www.Department10.University1.edu/FullProfessor1>" ;
    
    query_clause query;
    query_whereclause qwhere;
    
    
    query_triple qt;
    qt.set_src(src);
    qt.set_pred(pred);
    qt.set_dst("?x", 0);
    qt.set_traverse(eTransform);
    qt.set_query(&query);

    qwhere.add_child(&qt);
    query.add_whereclause(&qwhere);
    query.setup_qid(1, 1);

    srset_t* srset = query.get_srset(0);
    srset->setup_select(1);
    srset->create_select(0, "?x", 0);
    
    g->run_query(&query);
}

void test2()
{
    /* select ?x ?Y1
     * where {
     *   ?x rdf:type    ub:FullProfessor.
     *   ?x ub:worksFor <http://www.Department0.University0.edu>">.
     *   ?y ub:publicationAuthor ?x.
     *   ?y ub:name ?Y1.
     *
     *
     */
    const char* pred1 = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#worksFor>";
    const char* dst1 = "<http://www.Department0.University0.edu>";
    
    const char* pred = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
    const char* dst  = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#FullProfessor>";
    
    const char* pred2 = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#publicationAuthor>";
    const char* name_pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#name>";
    
    query_whereclause qwhere;
    query_clause query;
    query_triple qt1;
    query_triple qt2;
    srset_t* srset;
    
    //other setup
    query.add_whereclause(&qwhere);
    query.setup_qid(2, 2);
    
    //first query
    qt1.set_src("?x", 0);
    qt1.set_pred(pred1);
    qt1.set_dst(dst1);
    qt1.set_query(&query);
    qt1.set_traverse(eTransform);
    qwhere.add_child(&qt1);
    
    srset = query.get_srset(0);
    srset->setup_select(1);
    srset->create_select(0, "?x", 0);
    
    //Get the filter details
    filter_info_t filter_info;
    propid_t type_pid = g->get_pid(pred);
    filter_info.rgraph = g->p_info[type_pid];
    if (eOK != g->p_info[0]->get_encoded_value(dst, &filter_info.value)) {
        assert(0);
    }
    filter_info.filter_fn = fn_out;
    srset->set_filter(&filter_info);

    //second query
    qt2.set_src("?y", 1);
    qt2.set_pred(pred2);
    qt2.set_dst("?x", 0);
    qt2.set_qplan(eInward);
    qt2.set_traverse(eExtend);
    qt2.set_query(&query);
    qwhere.add_child(&qt2);
    
    srset = query.get_srset(1);
    srset->setup_select(1);
    srset->create_select(0, "?y", name_pred);

    g->run_query(&query);

}

void lubm_1()
{
/*    
# Query1
# This query bears large input and high selectivity. It queries about just one class and
# one property and does not assume any hierarchy information or inference.
    PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#>
    PREFIX ub: <http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#>
    SELECT ?X  WHERE {
     ?X rdf:type ub:GraduateStudent .
     ?X ub:takesCourse <http://www.Department0.University0.edu/GraduateCourse0>
    }
*/
    const char* pred = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
    const char* dst  = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#GraduateStudent>";
    //const char* dst  = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#UndergraduateStudent>";
    
    const char* pred1 = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#takesCourse>";
    const char* dst1 = "<http://www.Department0.University0.edu/GraduateCourse0>";
    
    query_whereclause qwhere;
    query_clause query;
    
    query_triple qt1;
    qt1.set_src("?x", 0);
    qt1.set_pred(pred1);
    qt1.set_dst(dst1);
    qt1.set_traverse(eTransform);
    qt1.set_query(&query);

    qwhere.add_child(&qt1);
    query.add_whereclause(&qwhere);
    query.setup_qid(1,1);
    
    //Get the filter details
    filter_info_t filter_info;
    propid_t type_pid = g->get_pid(pred);
    filter_info.rgraph = g->p_info[type_pid];
    if (eOK != g->p_info[0]->get_encoded_value(dst, &filter_info.value)) {
        assert(0);
    }
    filter_info.filter_fn = fn_out;
    srset_t* srset = query.get_srset(0);
    srset->set_filter(&filter_info);
    
    g->run_query(&query);
}


void lubm_4()
{

/* Query 4
 * SELECT ?X, ?Y1, ?Y2, ?Y3
        WHERE
        {?X rdf:type ub:Professor .
        ?X ub:worksFor <http://www.Department0.University0.edu> .
        ?X ub:name ?Y1 .
        ?X ub:emailAddress ?Y2 .
        ?X ub:telephone ?Y3}
*/
    const char* pred = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
    const char* dst  = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#FullProfessor>";
    
    const char* pred1 = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#worksFor>";
    const char* dst1 = "<http://www.Department0.University0.edu>";
    
    const char* name_pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#name>";
    const char* email_pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#emailAddress>";
    const char* telephone_pred = "<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#telephone>";
    
    query_whereclause qwhere;
    query_clause query;
    
    query_triple qt1;
    qt1.set_src("?x", 0);
    qt1.set_pred(pred1);
    qt1.set_dst(dst1);
    qt1.set_traverse(eTransform);
    qt1.set_query(&query);
    
    qwhere.add_child(&qt1);
    query.add_whereclause(&qwhere);
    query.setup_qid(1,1);
    
    //Get the filter details
    filter_info_t filter_info;
    propid_t type_pid = g->get_pid(pred);
    filter_info.rgraph = g->p_info[type_pid];
    if (eOK != g->p_info[0]->get_encoded_value(dst, &filter_info.value)) {
        assert(0);
    }
    filter_info.filter_fn = fn_out;
    srset_t* srset = query.get_srset(0);
    srset->set_filter(&filter_info);

    select_info_t select_info[3];
    select_info[0].name = gstrdup("Y1");
    select_info[1].name = gstrdup("Y2");
    select_info[2].name = gstrdup("Y3");

    select_info[0].rgraph = g->p_info[g->get_pid(name_pred)];
    select_info[1].rgraph = g->p_info[g->get_pid(email_pred)];
    select_info[2].rgraph = g->p_info[g->get_pid(telephone_pred)];
    

    g->run_query(&query);
}

void lubm() 
{
    test1();
    test2();
    /*lubm_1();
    lubm_4();
    */
}

