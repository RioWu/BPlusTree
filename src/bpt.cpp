#include "../include/bpt.h"
#include <stdlib.h>
#include <list>
#include <algorithm>

using std::begin;
using std::binary_search;
using std::end;
using std::lower_bound;
using std::swap;
using std::upper_bound;

namespace BPT
{

    //构造函数
    bpt::bpt(const char *p, bool force_empty) : fp(NULL), fp_level(0)
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

    /*
    *******
    辅助函数
    *******
*/
    //两个辅助函数，用于获取结点内元素数组的首地址，以及尾地址向后一位的地址。
    template <class T>
    typename T::child_t begin(T &node)
    {
        return node.children;
    }

    template <class T>
    typename T::child_t end(T &node)
    {
        return node.children + node.n;
    }
    //在内部结点查找第一个大于key值的对应元素下标
    index_t *find(internal_node_t &node, const key_t &key)
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
        return begin(node);
    }
    //在叶子结点中查找第一个小于等于key值的对应元素下标
    record_t *find(leaf_node_t &node, const key_t &key)
    {
        return lower_bound(begin(node), end(node), key);
    }

    /*
        *******
        查找相关
        *******
    */

    //根据内部结点偏移量以及key值找到对应的叶子结点
    off_t bpt::search_leaf(off_t index, const key_t &key) const
    {
        internal_node_t node;
        read(&node, index);

        index_t *i = upper_bound(begin(node), end(node), key);
        return i->child;
    }
    //找到该key值对应的叶子结点的父结点
    off_t bpt::search_index(const key_t &key) const
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
    int bpt::search(const key_t &key, value_t *value) const
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
    int bpt::search_range(key_t *left, const key_t &right,
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
                b = begin(leaf);

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
    /*
    *******
    删除相关
    *******
*/
    int bpt::remove(const key_t &key)
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
                assert(leaf.next != 0 || leaf.prev != 0);
                key_t index_key;
                if (where == end(parent) - 1)
                {
                    //若该结点为父结点最右边的子结点，则合并prev和leaf
                    assert(leaf.prev != 0);
                    leaf_node_t prev;
                    read(&prev, leaf.prev);
                    index_key = begin(prev)->key;

                    merge_leafs(&prev, &leaf);
                    node_remove(&prev, &leaf);
                    write(&prev, leaf.prev);
                }
                else
                {
                    //否则合并prev和next
                    assert(leaf.next != 0);
                    leaf_node_t next;
                    read(&next, leaf.next);
                    index_key = begin(next)->key;

                    merge_leafs(&next, &leaf);
                    node_remove(&next, &leaf);
                    write(&next, leaf.next);
                }
                //删除父结点对应的key
                remove_from_index(parent_off, parent, index_key);
            }
            else
            {
                write(&leaf, offset);
            }
        }
        else
        {
            write(&leaf, offset);
        }
        return 0;
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
            leaf_node_t::child_t where_to_lend, where_to_put;
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
    void bpt::change_parent_child(off_t parent, const key_t &o, const key_t &n)
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
    //合并两个叶子结点
    void bpt::merge_leafs(leaf_node_t *left, leaf_node_t *right)
    {
        std::copy(begin(*right), end(*right), end(*left));
        left->n += right->n;
    }
    //删除一个结点
    template <class T>
    void bpt::node_remove(T *prev, T *node)
    {
        unalloc(node, prev->next);
        prev->next = node->next;
        if (node->next != 0)
        {
            T next;
            read(&next, node->next, SIZE_NO_CHILDREN);
            next.prev = node->prev;
            write(&next, node->next, SIZE_NO_CHILDREN);
        }
        write(&meta, OFFSET_META);
    }
    //删除一个内部节点
    void bpt::remove_from_index(off_t offset, internal_node_t &node,
                                const key_t &key)
    {
        size_t min_n = meta.root_offset == offset ? 1 : meta.order / 2;
        assert(node.n >= min_n && node.n <= meta.order);

        //删除key
        key_t index_key = begin(node)->key;
        index_t *to_delete = find(node, key);
        if (to_delete != end(node))
        {
            (to_delete + 1)->child = to_delete->child;
            std::copy(to_delete + 1, end(node), to_delete);
        }
        node.n--;
        //当被删除的是父结点最后一个key时
        if (node.n == 1 && meta.root_offset == offset && meta.internal_node_num != 1)
        {
            unalloc(&node, meta.root_offset);
            meta.height--;
            meta.root_offset = node.children[0].child;
            write(&meta, offset);
            return;
        }
        //合并或者借兄弟结点的key
        if (node.n < min_n)
        {
            internal_node_t parent;
            read(&parent, node.parent);

            //先从左边借
            bool borrowed = false;
            if (offset != begin(parent)->child)
                borrowed = borrow_key(false, node, offset);

            //再从右边借
            if (!borrowed && offset != (end(parent) - 1)->child)
            {
                borrowed = borrow_key(true, node, offset);
            }
            //都不成功，则合并
            if (!borrowed)
            {
                assert(node.next != 0 || node.prev != 0);
                if (offset == (end(parent) - 1)->child)
                {
                    //若该结点为父结点的最右边结点，则合并prev和node
                    assert(node.prev != 0);
                    internal_node_t prev;
                    read(&prev, node.prev);

                    //合并
                    index_t *where = find(parent, begin(prev)->key);
                    reset_index_children_parent(begin(node), end(node), node.prev);
                    merge_keys(where, prev, node, true);
                    write(&prev, node.prev);
                }
                else
                {
                    //否则合并next和node
                    assert(node.next != 0);
                    internal_node_t next;
                    read(&next, node.next);

                    index_t *where = find(parent, index_key);
                    reset_index_children_parent(begin(next), end(next), offset);
                    merge_keys(where, node, next);
                    write(&node, offset);
                }
                //删除父结点的key
                remove_from_index(node.parent, parent, index_key);
            }
            else
            {
                write(&node, offset);
            }
        }
        else
        {
            write(&node, offset);
        }
    }
    //内部结点的借操作
    bool bpt::borrow_key(bool from_right, internal_node_t &borrower,
                         off_t offset)
    {
        typedef internal_node_t::child_t child_t;

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
    //修改内部结点的父结点
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
    void bpt::merge_keys(index_t *where,
                         internal_node_t &node, internal_node_t &next, bool change_where_key)
    {
        if (change_where_key)
        {
            where->key = (end(next) - 1)->key;
        }
        std::copy(begin(next), end(next), end(node));
        node.n += next.n;
        node_remove(&node, &next);
    }
    /*
    *******
    增加相关
    *******
*/
    int bpt::insert(const key_t &key, value_t value)
    {
        off_t parent = search_index(key);
        off_t offset = search_leaf(parent, key);
        leaf_node_t leaf;
        read(&leaf, offset);

        //检查是否已有相同key值
        if (binary_search(begin(leaf), end(leaf), key))
            return 1;

        if (leaf.n == meta.order)
        {
            //当数据项数满时，进行分裂
            leaf_node_t new_leaf;
            node_create(offset, &leaf, &new_leaf);

            //找到合适的分裂点
            size_t point = leaf.n / 2;
            bool place_right = keycmp(key, leaf.children[point].key) > 0;
            if (place_right)
                ++point;

            //分裂
            std::copy(leaf.children + point, leaf.children + leaf.n,
                      new_leaf.children);
            new_leaf.n = leaf.n - point;
            leaf.n = point;

            //将key分配至分裂出的结点
            if (place_right)
                insert_record_no_split(&new_leaf, key, value);
            else
                insert_record_no_split(&leaf, key, value);

            //保存叶子结点
            write(&leaf, offset);
            write(&new_leaf, leaf.next);

            //在父结点中添加索引项
            insert_key_to_index(parent, new_leaf.children[0].key,
                                offset, leaf.next);
        }
        else
        {
            insert_record_no_split(&leaf, key, value);
            write(&leaf, offset);
        }
        return 0;
    }
    //创建一个新结点
    template <class T>
    void bpt::node_create(off_t offset, T *node, T *next)
    {
        next->parent = node->parent;
        next->next = node->next;
        next->prev = offset;
        node->next = alloc(next);
        //更新next结点的prev
        if (next->next != 0)
        {
            T old_next;
            read(&old_next, next->next, SIZE_NO_CHILDREN);
            old_next.prev = node->next;
            write(&old_next, next->next, SIZE_NO_CHILDREN);
        }
        write(&meta, OFFSET_META);
    }
    //在叶子结点中添加新的数据项(无分裂)
    void bpt::insert_record_no_split(leaf_node_t *leaf,
                                     const key_t &key, const value_t &value)
    {
        record_t *where = upper_bound(begin(*leaf), end(*leaf), key);
        std::copy_backward(where, end(*leaf), end(*leaf) + 1);

        where->key = key;
        where->value = value;
        leaf->n++;
    }
    //在内部结点中添加新的索引项
    void bpt::insert_key_to_index(off_t offset, const key_t &key,
                                  off_t old, off_t after)
    {
        if (offset == 0)
        {
            //创建新的根结点
            internal_node_t root;
            root.next = root.prev = root.parent = 0;
            meta.root_offset = alloc(&root);
            meta.height++;

            //添加"old"和"after"
            root.n = 2;
            root.children[0].key = key;
            root.children[0].child = old;
            root.children[1].child = after;

            write(&meta, OFFSET_META);
            write(&root, meta.root_offset);

            //更新孩子结点的父结点
            reset_index_children_parent(begin(root), end(root),
                                        meta.root_offset);
            return;
        }
        internal_node_t node;
        read(&node, offset);
        assert(node.n <= meta.order);

        if (node.n == meta.order)
        {
            //当数据项满时进行分裂

            internal_node_t new_node;
            node_create(offset, &node, &new_node);

            //找到合适的分裂点
            size_t point = (node.n - 1) / 2;
            bool place_right = keycmp(key, node.children[point].key) > 0;
            if (place_right)
                ++point;

            if (place_right && keycmp(key, node.children[point].key) < 0)
                point--;

            key_t middle_key = node.children[point].key;

            //分裂
            std::copy(begin(node) + point + 1, end(node), begin(new_node));
            new_node.n = node.n - point - 1;
            node.n = point + 1;

            //放置新key
            if (place_right)
                insert_key_to_index_no_split(new_node, key, after);
            else
                insert_key_to_index_no_split(node, key, after);
            write(&node, offset);
            write(&new_node, node.next);

            //更新孩子结点的父结点
            reset_index_children_parent(begin(new_node), end(new_node), node.next);
        }
        else
        {
            insert_key_to_index_no_split(node, key, after);
            write(&node, offset);
        }
    }
    void bpt::insert_key_to_index_no_split(internal_node_t &node,
                                           const key_t &key, off_t value)
    {
        index_t *where = upper_bound(begin(node), end(node) - 1, key);

        //将后面的索引项前置
        std::copy_backward(where, end(node), end(node) + 1);

        //插入该key
        where->key = key;
        where->child = (where + 1)->child;
        (where + 1)->child = value;
    }
    /*
    *******
    更新相关
    *******
    */
    int bpt::update(const key_t &key, value_t value)
    {
        off_t offset = search_leaf(key);
        leaf_node_t leaf;
        read(&leaf, offset);

        record_t *record = find(leaf, key);
        if (record != leaf.children + leaf.n)
            if (keycmp(key, record->key) == 0)
            {
                record->value = value;
                write(&leaf, offset);

                return 0;
            }
            else
            {
                return 1;
            }
        else
            return -1;
    }
}