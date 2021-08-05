#include "bpt.h"
#include <stdlib.h>
#include <list>
#include <algorithm>

using std::swap;
using std::binary_search;
using std::lower_bound;
using std::upper_bound;
using std::begin;
using std::end;

index_t *find(internal_node_t &node,key_t &key)
{
    if(key){
        return upper_bound(begin(node.children),end(node.children)-1,key);
    }
    if(node.n > 1)
    {
        return node.children + node.n -2;
    }
    return begin(node);
}
record_t *find(leaf_node_t &node, const key_t &key) {
    return lower_bound(begin(node.children), end(node.children), key);
}

bpt::bpt(char* p,bool force_empty): fp(NULL),fp_level(0)
{
    bzero(path,sizeof(path));
    strcpy(path,p);

    if(!force_empty)
    {
        //如果不为0，代表文件已经出错。
        if(read(&meta,OFFSET_META)!= 0)
            force_empty = true;
    }
    if(force_empty)
    {
        open_file("w+");
        init_from_empty();
        close_file();
    }


}

