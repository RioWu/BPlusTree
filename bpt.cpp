#include "bpt.h"
#include <stdlib.h>
#include <list>
#include <algorithm>

using std::begin;
using std::binary_search;
using std::end;
using std::lower_bound;
using std::swap;
using std::upper_bound;

//构造函数
bpt::bpt(char *p, bool force_empty) : fp(NULL), fp_level(0)
{
    bzero(path, sizeof(path));
    strcpy(path, p);

    if (!force_empty)
    {
        //如果不为0，代表文件已经出错。
        if (read(&meta, OFFSET_META) != 0)
            force_empty = true;
    }
    if (force_empty)
    {
        open_file("w+");
        init_from_empty();
        close_file();
    }
}
void bpt::init_from_empty()
{
    //初始化b+树元数据
    bzero(&meta, sizeof(meta_t));
    meta.order = BP_ORDER;
    meta.value_size = sizeof(value_t);
    meta.key_size = sizeof(key_t);
    meta.height = 1;
    meta.slot = OFFSET_BLOCK;

    //初始化根结点
    internal_node_t root;
    root.next = root.prev = root.parent = 0;
    meta.root_offset = alloc(&root);

    //初始化一个空的叶结点
    leaf_node_t leaf;
    leaf.next = leaf.prev = 0;
    leaf.parent = meta.root_offset;
    meta.leaf_offset = root.children[0].child = alloc(&leaf);

    //保存上述数据至磁盘
    write(&meta, OFFSET_META);
    write(&root, meta.root_offset);
    write(&leaf, root.children[0].child);
}

//在内部结点/叶子结点中查找对应key值对应的下标
index_t *find(internal_node_t &node, key_t &key)
{
    if (key)
    {
        return upper_bound(begin(node.children), end(node.children), key);
    }
    if (node.n > 1)
    {
        return node.children + node.n - 2;
    }
    return begin(node);
}
record_t *find(leaf_node_t &node, const key_t &key)
{
    return lower_bound(begin(node.children), end(node.children), key);
}

//找到该key值对应的叶子结点
off_t bpt::search_leaf(off_t index, key_t &key)
{
    internal_node_t node;
    read(&node, index);

    index_t *i = upper_bound(begin(node.children), end(node.children), key);
    return i->child;
}
//找到该key值对应的叶子结点的父结点
off_t bpt::search_index(key_t &key)
{
    off_t org = meta.root_offset;
    int height = meta.height;
    while (height > 1)
    {
        internal_node_t node;
        read(&node, org);

        index_t *i = upper_bound(begin(node.children), end(node.children), key);
        org = i->child;
        --height;
    }
    return org;
}
//查找索引key，并将结果（若查找到）存至value内
int bpt::search(key_t &key, value_t *value)
{
    leaf_node_t leaf;
    read(&leaf, search_leaf(key));

    record_t *record = find(leaf, key);
    if (record != leaf.children + leaf.n)
    {
        //找到了该数据
        *value = record->value;
        return keycmp(record->key, key);
    }
    else
    {
        //未找到该数据
        return -1;
    }
}

//范围查找，以left为最小，right为最大，结果存至数组values内。
int bpt::search_range(key_t *left, key_t &right,
                      value_t *values, size_t max, bool *next)
{
    if (left == NULL || keycmp(*left, right) > 0)
        return -1;
    off_t off_left = search_leaf(*left);
    off_t off_right = search_leaf(right);
    off_t off = off_left;
    size_t i = 0;
    record_t *b, *e;

    leaf_node_t leaf;
    while (off != off_right && off != 0 & i < max)
    {
        read(&leaf, off);

        //刚开始
        if (off_left == off)
            b = find(leaf, *left);
        else
            b = begin(leaf.children);

        e = leaf.children + leaf.n;
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;
        off = leaf.next;
    }

    //最后一个叶子结点
    if (i < max)
    {
        read(&leaf, off_right);

        b = find(leaf, *left);
        e = upper_bound(begin(leaf.children), end(leaf.children), right);
        for (; b != e && i < max; ++b, ++i)
            values[i] = b->value;
    }

    if (next != NULL)
    {
        if (i == max && b != e)
        {
            *next = true;
            *left = b->key;
        }
        else
        {
            *next = false;
        }
    }
    return i;
}

