#include <algorithm>
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <assert.h>
#include <string>
#include <map>
#include "graph.h"


using std::ifstream;


void graph::prep_graph(string idirname)
{
    struct dirent *ptr;
    DIR *dir;
    dir = opendir(idirname.c_str());
    string subject, predicate, object, useless_dot;
    int file_count = 0;
    
    //Read graph file
    while (NULL != (ptr = readdir(dir))) {
        if (ptr->d_name[0] == '.') continue;
        
        ifstream file((idirname + "/" + string(ptr->d_name)).c_str());
        int nt_count= 0;
        file_count++;
        file >> subject >> predicate >> object >> useless_dot;
        file >> subject >> predicate >> object >> useless_dot;
        propid_t pid;
        map<string, propid_t>::iterator str2pid_iter;
        while (file >> subject >> predicate >> object >> useless_dot) {
            str2pid_iter = str2pid.find(predicate);
            if (str2pid_iter == str2pid.end()) assert(0);
            pid = str2pid_iter->second;
            p_info[pid]->batch_update(subject, object);
            ++nt_count;
        }
    }

    //make graph
    for (int i = 0; i < p_count; i++) {
        p_info[i]->make_graph_baseline();
    }

    //Store graph
    for (int i = 0; i < p_count; i++) {
        p_info[i]->store_graph_baseline(idirname);
    }
}


void ontology_lubm()
{
    g->p_info = new p_info_t*[32];
    p_info_t* info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#advisor>", "advisor");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#affiliatedOrganizationOf>", "affiliatedOrganizationOf");

    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#affiliateOf>","affiliateOf");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#degreeFrom>", "degreeFrom");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#doctoralDegreeFrom>","doctoralDegreeFrom");
    
    //inference, inverse of degree from
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#hasAlumnus>");
    
    info = new one2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#headOf>", "headOf");
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#listedCourse>","listedCourse");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#mastersDegreeFrom>","mastersDegreeFrom");
    
    //inference, inverse of memberof
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#member>", "member");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#memberOf>", "memberOf");
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#orgPublication>", "orgPublication");
    
    info = new dgraph_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#publicationAuthor>", "publicationAuthor");
    
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#publicationResearch>", "publicationResearch");
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#researchProject>", "researchProject");
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#softwareDocumentation>", "softwareDocumentation");
    
    info = new dgraph_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#takesCourse>", "takesCourse");
    
    info = new one2many_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#teacherOf>", "teacherOf");
    
    info = new one2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#teachingAssistantOf>", "teachingAssistantOf");
    
    //info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#tenured>", "tenured");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#undergraduateDegreeFrom>", "undergraduateDegreeFrom");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#worksFor>", "worksFor");
    
    info = new many2one_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#subOrganizationOf>", "subOrganizationOf");
    
    
    /*********************************************/
    info = new enum8kv_t;
    info->populate_property("<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>", "type");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#publicationDate>", "publicationDate");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#softwareVersion>", "softwareVersion");
    
    
    info = new int8kv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#age>", "age");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#emailAddress>", "emailAddress");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#name>", "name");
    
    info = new int64kv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#officeNumber>", "officeNumber");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#researchInterest>", "researchInterest");
    
    info = new int64kv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#telephone>", "telephone");
    
    info = new stringkv_t;
    info->populate_property("<http://www.lehigh.edu/~zhp2/2004/0401/univ-bench.owl#title>", "title");
}


