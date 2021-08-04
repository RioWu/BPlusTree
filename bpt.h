#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define BP_ORDER 20

//offset
#define OFFSET_META 0
#define OFFSET_BLOCK OFFSET_META + sizeof(meta_t)
#define SIZE_NO_CHILDREN sizeof(leaf_node_t) - BP_ORDER * sizeof(record_t)

//key&value
typedef int value_t;
struct key_t
{
    char k[16];
    key_t(char *str = "")
    {
        bzero(k, sizeof(k));
        strcpy(k, str);
    }
    operator bool() const
    {
        return strcmp(k, "");
    }
};

//compare two keys
int keycmp(key_t a, key_t b)
{
    int x = strlen(a.k) - strlen(b.k);
    return x == 0 ? strcmp(a.k, b.k) : x;
}
template <typename T>
bool operator<(key_t l, T r)
{
    return keycmp(l, r.key) < 0;
}
template <typename T>
bool operator<(T l, key_t r)
{
    return keycmp(l.key, r) < 0;
}
template <typename T>
bool operator==(key_t l, T r)
{
    return keycmp(l, r.key) == 0;
}
template <typename T>
bool operator==(T l, key_t r)
{
    return keycmp(l.key, r) == 0;
}

//meta information for bpt
typedef struct
{
    size_t order;
    size_t value_size;
    size_t key_size;
    size_t internal_node_num;
    size_t lead_node_num;
    size_t height;
    off_t slot;
    off_t root_offset;
    off_t leaf_offset;
} meta_t;

//internal node
struct index_t
{
    key_t key;
    off_t child;
};
struct internal_node_t
{
    off_t parent;
    off_t next;
    off_t prev;
    size_t n;
    index_t children[BP_ORDER];
};

//leaf node
struct record_t
{
    key_t key;
    value_t value;
};
struct leaf_node_t
{
    off_t parent;
    off_t next;
    off_t prev;
    size_t n;
    record_t children[BP_ORDER];
};

//bpt
class bpt
{
public:
    bpt(char *path, bool force_empty = false);
    int search(key_t key, value_t value);
    int search_range(key_t left, key_t right, value_t *values, size_t max, bool *next = NULL);
    int remove(key_t key);
    int insert(key_t key, value_t value);
    int update(key_t key, value_t value);
    meta_t get_meta()
    {
        return meta;
    }

private:
    char path[512];
    meta_t meta;

    void init_from_empty();

    //search
    off_t search_index(key_t key);
    off_t search_leaf(off_t index, key_t key);
    off_t search_leaf(key_t key)
    {
        return search_leaf(search_index(key), key);
    }

    //remove
    void remove_from_index(off_t offset, internal_node_t &node, key_t key);
    bool borrow_key(bool from_right, internal_node_t &borrower, off_t offset);
    bool borrow_key(bool from_right, internal_node_t &borrower);
    void change_parent_child(off_t parent, key_t o, key_t n);
    void merge_leafs(leaf_node_t *left, leaf_node_t *right);
    void merge_keys(index_t *where, internal_node_t &left,
                    internal_node_t &right, bool change_where_key = false);

    //insert
    void insert_record_no_split(leaf_node_t *leaf,
                                key_t key, value_t value);
    void insert_key_to_index(off_t offset, key_t key,
                             off_t value, off_t after);
    void insert_key_to_index_no_split(internal_node_t &node, key_t key,
                                      off_t value);
    void reset_index_children_parent(index_t *begin, index_t *end,
                                     off_t parent);

    template <class T>
    void node_create(off_t offset, T *node, T *next);
    template <class T>
    void node_remove(T *prev, T *node);

    // open/close file
    FILE *fp;
    int fp_level;

    void open_file(const char *mode = "rb+")
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

    // alloc/unalloc
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

    // read/write
    template <class T>
    int get_data(void *block, off_t offset, size_t size)
    {
        open_file();
        fseek(fp, offset, SEEK_SET);
        size_t rd = fread(block, size, 1, fp);
        close_file();
        return rd - 1;
    }
    template <class T>
    int get_data(T *block, off_t offset)
    {
        return get_data(block, offset, sizeof(T));
    }

    int put_data(void *block, off_t offset, size_t size) const
    {
        open_file();
        fseek(fp, offset, SEEK_SET);
        size_t wd = fwrite(block, size, 1, fp);
        close_file();
        return wd - 1;
    }

    template <class T>
    int put_data(T *block, off_t offset) const
    {
        return put_data(block, offset, sizeof(T));
    }
};