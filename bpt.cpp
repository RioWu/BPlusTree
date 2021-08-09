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

//两个辅助函数，用于获取结点内元素数组的首地址，以及尾地址向后一位的地址。
template <class T>
typename T::child_t begin(T &node)
{
    return node.children;
}

template <class T>
typename T::child_t end(T &node)
{
    return node.children + n;
}
//在内部结点查找第一个大于key值的对应元素下标
index_t *find(internal_node_t &node, key_t &key)
{
    if (key)
    {
        return upper_bound(begin(node), end(node), key);
    }
    //由于index_t数组的最后一个元素是空，所以当我们查找空key值时（用于合并内部结点），应该返回倒数第二个位置。
    if (node.n > 1)
    {
        return node.children + node.n - 2;
    }
    return begin(node.children);
}
//在叶子结点中查找第一个小于等于key值的对应元素下标
record_t *find(leaf_node_t &node, const key_t &key)
{
    return lower_bound(begin(node), end(node), key);
}

//根据内部结点偏移量以及key值找到对应的叶子结点
off_t bpt::search_leaf(off_t index, key_t &key)
{
    internal_node_t node;
    read(&node, index);

    index_t *i = upper_bound(begin(node), end(node), key);
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

        index_t *i = upper_bound(begin(node), end(node), key);
        org = i->child;
        --height;
    }
    return org;
}
//从根结点开始查找
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

//从最小关键字起顺序查找，即从叶子结点出发查找。
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
        e = upper_bound(begin(leaf), end(leaf), right);
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
int bpt::remove(key_t &key)
{
    internal_node_t parent;
    leaf_node_t leaf;

    //找到父结点
    off_t parent_off = search_index(key);
    read(&parent, parent_off);

    //找到当前结点
    index_t *where = find(parent, key);
    off_t offset = where->child;
    read(&leaf, offset);

    //核实当前结点的正确性
    if (!binary_search(begin(leaf), end(leaf), key))
        return -1;

    size_t min_n = meta.leaf_node_num == 1 ? 0 : meta.order / 2;
    assert(leaf.n >= min_n && leaf.n <= meta.order);

    //删除该key值
    record_t *to_delete = find(leaf, key);
    std::copy(to_delete + 1, end(leaf), to_delete);
    //std::copy(要拷贝元素的首地址，要拷贝元素的最后一个地址的下一个地址，要拷贝的目的地的首地址)
    leaf.n--;

    //合并或者借用其他结点的key值
    if (leaf.n < min_n)
    {
        //先尝试从左兄弟结点借
        bool borrowed = false;
        if (leaf.prev != 0)
            borrowed = borrow_key(false, leaf);

        //再尝试从右兄弟借
        if (!borrowed && leaf.next != 0)
            borrowed = borrow_key(true, leaf);

        //若都无法借，则尝试合并
        if (!borrowed)
        {
        }
    }
    else
    {
        write(&leaf, offset);
    }
    return 0;
}
//内部结点的借操作
bool bpt::borrow_key(bool from_right, internal_node_t &borrower, off_t offset)
{
    typedef typename internal_node_t::child_t child_t;

    off_t lender_off = from_right ? borrower.next : borrower.prev;
    internal_node_t lender;
    read(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2)
    {
        child_t where_to_lend, where_to_put;
        internal_node_t parent;

        //从右兄弟结点中借多余的key值，并且更新父结点。
        if (from_right)
        {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);

            read(&parent, borrower.parent);
            child_t where = lower_bound(begin(parent), end(parent),
                                        (end(borrower) - 1)->key);
            where->key = where_to_lend->key;
            write(&parent, borrower.parent);
        }
        //从左兄弟结点中借多余的key值，并且更新父结点。
        else
        {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);

            read(&parent, lender.parent);
            child_t where = find(parent, begin(lender)->key);
            where->key = (where_to_lend - 1)->key;
            write(&parent, lender.parent);
        }
        //更新borroer结点
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        //std::copy_backward(要拷贝元素的首地址，要拷贝元素的最后一个地址的下一个地址，要拷贝目的地的尾地址的下一个地址)
        *where_to_put = *where_to_lend;
        borrower.n++;

        //更新lender结点
        reset_index_children_parent(where_to_lend, where_to_lend + 1, offset);
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        write(&lender, lender_off);
        return true;
    }
    return false;
}
//叶子结点的借操作
bool bpt::borrow_key(bool from_right, leaf_node_t &borrower)
{
    off_t lender_off = from_right ? borrower.next : borrower.prev;
    leaf_node_t lender;
    read(&lender, lender_off);

    assert(lender.n >= meta.order / 2);
    if (lender.n != meta.order / 2)
    {
        typename leaf_node_t::child_t where_to_lend, where_to_put;
        if (from_right)
        {
            where_to_lend = begin(lender);
            where_to_put = end(borrower);
            change_parent_child(borrower.parent, begin(borrower)->key,
                                lender.children[1].key);
        }
        else
        {
            where_to_lend = end(lender) - 1;
            where_to_put = begin(borrower);
            change_parent_child(lender.parent, begin(lender)->key,
                                where_to_lend->key);
        }
        //更新borrower结点
        std::copy_backward(where_to_put, end(borrower), end(borrower) + 1);
        *where_to_put = *where_to_lend;
        borrower.n++;

        //更新lender结点
        std::copy(where_to_lend + 1, end(lender), where_to_lend);
        lender.n--;
        write(&lender, lender_off);
        return true;
    }
    return false;
}
//更新父结点的索引项，将o改为n
void bpt::change_parent_child(off_t parent, key_t &o, key_t &n)
{
    internal_node_t node;
    read(&node, parent);

    index_t *w = find(node, o);
    assert(w != node.children + node.n);

    w->key = n;
    write(&node, parent);
    if (w == node.children + node.n - 1)
    {
        change_parent_child(node.parent, o, n);
    }
}
//修改结点的父结点
void bpt::reset_index_children_parent(index_t *begin, index_t *end, off_t parent)
{
    internal_node_t node;
    while (begin != end)
    {
        read(&node, begin->child);
        node.parent = parent;
        write(&node, begin->child, SIZE_NO_CHILDREN);
        ++begin;
    }
}
