#ifndef BPT_H
#define BPT_H

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

namespace BPT
{
#define BP_ORDER 4

//B+树元信息
#define OFFSET_META 0
//用于存储B+树内容的位置
#define OFFSET_BLOCK OFFSET_META + sizeof(meta_t)
//结点除过所存储数据占用的大小，用于仅修改结点结构的情况使用
#define SIZE_NO_CHILDREN sizeof(leaf_node_t) - BP_ORDER * sizeof(record_t)

    //定义索引结构和数据结构
    typedef int value_t;
    struct key_t
    {
        char k[16];
        key_t(const char *str = "")
        {
            bzero(k, sizeof(k));
            strcpy(k, str);
        }
        operator bool() const
        {
            return strcmp(k, "");
        }
    };

    //重载操作符，最终目的是使用stl里的算法去处理自定义的数据结构。
    //必须声明成inline，否则链接时会出现duplicate symbol
    inline int keycmp(const key_t &a, const key_t &b)
    {
        int x = strlen(a.k) - strlen(b.k);
        return x == 0 ? strcmp(a.k, b.k) : x;
    }
    //用于单元测试
        inline int keycmp_ut(const key_t &l, const key_t &r)
    {
        return strcmp(l.k, r.k);
    }
    //一棵b+树所需要的元数据
    typedef struct
    {
        size_t order; //B+树的阶数
        size_t value_size;
        size_t key_size;
        size_t internal_node_num; //内部结点个数
        size_t leaf_node_num;     //叶子结点个数
        size_t height;            //B+树高度（不包括叶子结点）
        off_t slot;               //储存新数据块的位置
        off_t root_offset;        //根结点位置
        off_t leaf_offset;        //第一个叶子结点位置
    } meta_t;

    //索引项结构
    struct index_t
    {
        key_t key;
        off_t child; //孩子结点的偏移量
    };
    //内部节点结构
    struct internal_node_t
    {
        typedef index_t *child_t;
        off_t parent; //父亲结点的偏移量
        off_t next;
        off_t prev;
        size_t n; //孩子个数
        index_t children[BP_ORDER];
    };
    //数据项结构
    struct record_t
    {
        key_t key;
        value_t value;
    };
    //叶子节点结构
    struct leaf_node_t
    {
        typedef record_t *child_t;
        off_t parent; //父结点偏移量
        off_t next;
        off_t prev;
        size_t n;
        record_t children[BP_ORDER];
    };

    //b+树
    class bpt
    {
    public:
        bpt(const char *path, bool force_empty = false);
        int search(const key_t &key, value_t *value) const;
        int search_range(key_t *left, const key_t &right,
                         value_t *values, size_t max, bool *next = NULL) const;
        int remove(const key_t &key);
        int insert(const key_t &key, value_t value);
        int update(const key_t &key, value_t value);
        meta_t get_meta()
        {
            return meta;
        }

        char path[512];
        meta_t meta;

        //初始化一颗空的B+树
        void init_from_empty();

        /*
            *******
            查找相关
            *******
        */
        //寻找索引key对应位置
        off_t search_index(const key_t &key) const;
        //寻找叶子结点
        off_t search_leaf(off_t index, const key_t &key) const;
        off_t search_leaf(const key_t &key) const
        {
            return search_leaf(search_index(key), key);
        }

        /*
            *******
            删除相关
            *******
        */
        //删除内部结点里对应的key值
        void remove_from_index(off_t offset, internal_node_t &node,
                               const key_t &key);
        //从内部结点借一个索引项
        bool borrow_key(bool from_right, internal_node_t &borrower,
                        off_t offset);
        //从叶子结点借一个数据项
        bool borrow_key(bool from_right, leaf_node_t &borrower);
        //修改一个结点的父结点对应的key
        void change_parent_child(off_t parent, const key_t &o,
                                 const key_t &n);
        //将右边的叶子合并至左边的叶子结点
        void merge_leafs(leaf_node_t *left, leaf_node_t *right);
        //合并内部节点
        void merge_keys(index_t *where, internal_node_t &left,
                        internal_node_t &right, bool change_where_key = false);
        //修改孩子结点的父结点
        void reset_index_children_parent(index_t *begin, index_t *end,
                                         off_t parent);

        /*
            *******
            增加相关
            *******
        */
        //将一个数据项插入至叶子结点（不包含分裂的情况）
        void insert_record_no_split(leaf_node_t *leaf,
                                    const key_t &key, const value_t &value);
        //将一个索引项插入至内部节点
        void insert_key_to_index(off_t offset, const key_t &key,
                                 off_t value, off_t after);
        //将一个索引项插入至内部节点（不包含分裂的情况）
        void insert_key_to_index_no_split(internal_node_t &node, const key_t &key,
                                          off_t value);


        template <class T>
        void node_create(off_t offset, T *node, T *next);
        template <class T>
        void node_remove(T *prev, T *node);

        //用于打开关闭文件
        mutable FILE *fp;
        mutable int fp_level;

        void open_file(const char *mode = "rb+") const
        {
            if (fp_level == 0)
                fp = fopen(path, mode);
            ++fp_level;
        }

        void close_file() const
        {
            if (fp_level == 1)
                fclose(fp);
            --fp_level;
        }

        //为节点分配磁盘空间
        off_t alloc(size_t size)
        {
            off_t slot = meta.slot;
            meta.slot += size;
            return slot;
        }
        off_t alloc(leaf_node_t *leaf)
        {
            leaf->n = 0;
            meta.leaf_node_num++;
            return alloc(sizeof(leaf_node_t));
        }
        off_t alloc(internal_node_t *node)
        {
            node->n = 1;
            meta.internal_node_num++;
            return alloc(sizeof(internal_node_t));
        }
        void unalloc(leaf_node_t *leaf, off_t offset)
        {
            --meta.leaf_node_num;
        }

        void unalloc(internal_node_t *node, off_t offset)
        {
            --meta.internal_node_num;
        }

        //读磁盘、写磁盘
        int read(void *block, off_t offset, size_t size) const
        {
            open_file();
            fseek(fp, offset, SEEK_SET);
            size_t rd = fread(block, size, 1, fp);
            close_file();
            return rd - 1;
        }
        template <class T>
        int read(T *block, off_t offset) const
        {
            return read(block, offset, sizeof(T));
        }

        int write(void *block, off_t offset, size_t size) const
        {
            open_file();
            fseek(fp, offset, SEEK_SET);
            size_t wd = fwrite(block, size, 1, fp);
            close_file();
            return wd - 1;
        }

        template <class T>
        int write(T *block, off_t offset) const
        {
            return write(block, offset, sizeof(T));
        }
    };
}
#endif