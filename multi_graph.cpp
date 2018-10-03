#include "multi_graph.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
using namespace rapidjson;

multi_graph_t::multi_graph_t()
{
}


void wls_schema()
{
    g->cf_info = new cfinfo_t*[3];
    g->p_info = new pinfo_t[3];
    
    pinfo_t*    p_info    = g->p_info;
    cfinfo_t*   info      = 0;
    const char* longname  = 0;
    const char* shortname = 0;
    
    longname = "gtype";
    shortname = "gtype";
    g->add_property(longname);
    p_info->populate_property(longname, shortname);
    info = new typekv_t;
    g->add_columnfamily(info);
    info->create_columns();
    info->add_column(p_info);
    ++p_info;
    
    typekv_t* typekv = g->get_typekv();
    typekv->manual_setup(1<<28, "process");//processes are tid 0
    typekv->manual_setup(1<<20, "user");//users are tid 1
    
    longname = "proc2parent";
    shortname = "proc2parent";
    g->add_property(longname);
    p_info->populate_property(longname, shortname);
    info = new unigraph<sid_t>;
    g->add_columnfamily(info);
    info->create_columns();
    info->add_column(p_info);
    info->flag1 = 1;
    info->flag2 = 1;
    ++p_info;
    
    longname = "user2proc";
    shortname = "user2proc";
    g->add_property(longname);
    p_info->populate_property(longname, shortname);
    info = new unigraph<wls_dst_t>;
    g->add_columnfamily(info);
    info->create_columns();
    info->add_column(p_info);
    info->flag1 = 2;
    info->flag2 = 1;
    ++p_info;
    
    
    g->prep_graph_baseline();
    g->file_open(true);
   // g->make_graph_baseline();
   // g->store_graph_baseline(); 
}

inline index_t parse_wls_line(char* line) 
{
    if (line[0] == '%') {
        return eNotValid;
    }
    
    edgeT_t<wls_dst_t> wls;
    edgeT_t<sid_t> edge;
    
    Document d;
    d.Parse(line);
    string log_host;

    Value::ConstMemberIterator itr = d.FindMember("ProcessID");
    if (itr != d.MemberEnd()) {
        string proc_id = itr->value.GetString();
        log_host = d["LogHost"].GetString();
        proc_id += "@";
        proc_id += log_host + "@";
        proc_id += d["LogonID"].GetString();
        //wls.dst_id.first = strtol(proc_id.c_str(), NULL, 0); 
        wls.dst_id.first = g->type_update(proc_id.c_str(), 0);//"process" are type id 0.
    } else {
        return eNotValid;
    }

    string user_name = d["UserName"].GetString();
    user_name += "@";
    
    itr = d.FindMember("DomainName");
    if (itr != d.MemberEnd()) {
        user_name += d["DomainName"].GetString();
        wls.src_id = g->type_update(user_name.c_str(), 1);//"user" are type id 1.
    } else {
        return eNotValid;
    }
    

    //Value& s = d["Time"];
    //int i = s.GetInt();
    
    wls.dst_id.second.time = d["Time"].GetInt();
    wls.dst_id.second.event_id = d["EventID"].GetInt();

    string logon_id = d["LogonID"].GetString();
    wls.dst_id.second.logon_id = strtol(logon_id.c_str(), NULL, 0); 
    
    //insert
    pgraph_t<wls_dst_t>* pgraph = (pgraph_t<wls_dst_t>*)g->get_sgraph(2);
    pgraph->batch_edge(wls);

    itr = d.FindMember("ParentProcessID");
    if (itr != d.MemberEnd()) {
        string proc_id = itr->value.GetString();
        proc_id += "@";
        proc_id += log_host;
        proc_id += logon_id;
        //edge.dst_id = strtol(proc_id.c_str(), NULL, 0); 
        edge.dst_id = g->type_update(proc_id.c_str(), 0);//"process" are type id 0.
        edge.src_id = wls.dst_id.first;

        //insert
        pgraph_t<sid_t>* pgraph = (pgraph_t<sid_t>*)g->get_sgraph(1);
        pgraph->batch_edge(edge);
    }
    
    return eOK;
}

index_t parsefile_and_multi_insert(const string& textfile, const string& ofile) 
{
    FILE* file = fopen(textfile.c_str(), "r");
    assert(file);
    
    index_t icount = 0;
	char sss[512];
    char* line = sss;

    while (fgets(sss, sizeof(sss), file)) {
        line = sss;
        if (eOK == parse_wls_line(line)) {
            icount++;
        }
    }
    
    fclose(file);
    return 0;
}

void read_idir_text(const string& idirname, const string& odirname, 
                    parse_fn_t parsefile_and_insert)
{
    struct dirent *ptr;
    DIR *dir;
    int file_count = 0;
    string filename;
    string ofilename;

    //Read graph files
    double start = mywtime();
    dir = opendir(idirname.c_str());
    while (NULL != (ptr = readdir(dir))) {
        if (ptr->d_name[0] == '.') continue;
        filename = idirname + "/" + string(ptr->d_name);
        ofilename = odirname + "/" + string(ptr->d_name);
        cout << "ifile= "  << filename << endl 
                <<" ofile=" << ofilename << endl;

        file_count++;
        parsefile_and_insert(filename, ofilename);
        double end = mywtime();
        cout <<" Time = "<< end - start;
    }
    closedir(dir);
    return;
}


void multi_graph_t::prep_graph_fromtext(const string& idirname, const string& odirname, 
                                        parse_fn_t parsefile_fn)
{
    //-----
    g->create_snapthread();
    usleep(1000);
    //-----
    //Batch and Make Graph
    double start = mywtime();
    read_idir_text(idirname, odirname, parsefile_fn);    
    double end = mywtime ();
    cout << "Batch Update Time = " << end - start << endl;
    
    /*
    blog_t<T>* blog = ugraph->blog;
    index_t marker = blog->blog_head;

    //----------
    if (marker != blog->blog_marker) {
        ugraph->create_marker(marker);
    }

    //Wait for make graph
    while (blog->blog_tail != blog->blog_head) {
        usleep(1);
    }
    */
    end = mywtime();
    cout << "Make graph time = " << end - start << endl;
    //---------
    
}