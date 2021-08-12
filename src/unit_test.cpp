#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>

#define PRINT(a) fprintf(stderr, "\033[33m%s\033[0m \033[32m%s\033[0m\n", a, "Passed")

#include "../include/bpt.h"
using BPT::bpt;

int main(int argc, char *argv[])
{
    const int size = 128;

    {
        bpt tree("test.db", true);
        assert(tree.meta.order == 4);
        assert(tree.meta.value_size == sizeof(BPT::value_t));
        assert(tree.meta.key_size == sizeof(BPT::key_t));
        assert(tree.meta.internal_node_num == 1);
        assert(tree.meta.leaf_node_num == 1);
        assert(tree.meta.height == 1);
        PRINT("EmptyTree");
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.meta.value_size == sizeof(BPT::value_t));
        assert(tree.meta.key_size == sizeof(BPT::key_t));
        assert(tree.meta.internal_node_num == 1);
        assert(tree.meta.leaf_node_num == 1);
        assert(tree.meta.height == 1);
        PRINT("ReReadEmptyTree");

        assert(tree.insert("t2", 2) == 0);
        assert(tree.insert("t4", 4) == 0);
        assert(tree.insert("t1", 1) == 0);
        assert(tree.insert("t3", 3) == 0);
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.meta.value_size == sizeof(BPT::value_t));
        assert(tree.meta.key_size == sizeof(BPT::key_t));
        assert(tree.meta.internal_node_num == 1);
        assert(tree.meta.leaf_node_num == 1);
        assert(tree.meta.height == 1);

        BPT::leaf_node_t leaf;
        tree.read(&leaf, tree.search_leaf("t1"));
        assert(leaf.n == 4);
        assert(BPT::keycmp_ut(leaf.children[0].key, "t1") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "t2") == 0);
        assert(BPT::keycmp_ut(leaf.children[2].key, "t3") == 0);
        assert(BPT::keycmp_ut(leaf.children[3].key, "t4") == 0);
        BPT::value_t value;
        assert(tree.search("t1", &value) == 0);
        assert(value == 1);
        assert(tree.search("t2", &value) == 0);
        assert(value == 2);
        assert(tree.search("t3", &value) == 0);
        assert(value == 3);
        assert(tree.search("t4", &value) == 0);
        assert(value == 4);
        assert(tree.insert("t1", 4) == 1);
        assert(tree.insert("t2", 4) == 1);
        assert(tree.insert("t3", 4) == 1);
        assert(tree.insert("t4", 4) == 1);
        PRINT("Insert4Elements");

        assert(tree.insert("t5", 5) == 0);
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.meta.value_size == sizeof(BPT::value_t));
        assert(tree.meta.key_size == sizeof(BPT::key_t));
        assert(tree.meta.internal_node_num == 1);
        assert(tree.meta.leaf_node_num == 2);
        assert(tree.meta.height == 1);

        BPT::internal_node_t index;
        off_t index_off = tree.search_index("t1");
        tree.read(&index, index_off);
        assert(index.n == 2);
        assert(index.parent == 0);
        assert(BPT::keycmp_ut(index.children[0].key, "t4") == 0);

        BPT::leaf_node_t leaf1, leaf2;
        off_t leaf1_off = tree.search_leaf("t1");
        assert(leaf1_off == index.children[0].child);
        tree.read(&leaf1, leaf1_off);
        assert(leaf1.n == 3);
        assert(BPT::keycmp_ut(leaf1.children[0].key, "t1") == 0);
        assert(BPT::keycmp_ut(leaf1.children[1].key, "t2") == 0);
        assert(BPT::keycmp_ut(leaf1.children[2].key, "t3") == 0);

        off_t leaf2_off = tree.search_leaf("t4");
        assert(leaf1.next == leaf2_off);
        assert(leaf2_off == index.children[1].child);
        tree.read(&leaf2, leaf2_off);
        assert(leaf2.n == 2);
        assert(BPT::keycmp_ut(leaf2.children[0].key, "t4") == 0);
        assert(BPT::keycmp_ut(leaf2.children[1].key, "t5") == 0);

        PRINT("SplitLeafBy2");
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.insert("t1", 4) == 1);
        assert(tree.insert("t2", 4) == 1);
        assert(tree.insert("t3", 4) == 1);
        assert(tree.insert("t4", 4) == 1);
        assert(tree.insert("t5", 4) == 1);
        BPT::value_t value;
        assert(tree.search("t1", &value) == 0);
        assert(value == 1);
        assert(tree.search("t2", &value) == 0);
        assert(value == 2);
        assert(tree.search("t3", &value) == 0);
        assert(value == 3);
        assert(tree.search("t4", &value) == 0);
        assert(value == 4);
        assert(tree.search("t5", &value) == 0);
        assert(value == 5);
        PRINT("Search2Leaf");

        assert(tree.insert("t6", 6) == 0);
        assert(tree.insert("t7", 7) == 0);
        assert(tree.insert("t8", 8) == 0);
        assert(tree.insert("t9", 9) == 0);
        assert(tree.insert("ta", 10) == 0);
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.meta.internal_node_num == 1);
        assert(tree.meta.leaf_node_num == 3);
        assert(tree.meta.height == 1);

        BPT::internal_node_t index;
        off_t index_off = tree.search_index("t8");
        tree.read(&index, index_off);
        assert(index.n == 3);
        assert(index.parent == 0);
        assert(BPT::keycmp_ut(index.children[0].key, "t4") == 0);
        assert(BPT::keycmp_ut(index.children[1].key, "t7") == 0);

        BPT::leaf_node_t leaf1, leaf2, leaf3;
        off_t leaf1_off = tree.search_leaf("t3");
        off_t leaf2_off = tree.search_leaf("t5");
        off_t leaf3_off = tree.search_leaf("ta");
        tree.read(&leaf1, leaf1_off);
        tree.read(&leaf2, leaf2_off);
        tree.read(&leaf3, leaf3_off);
        assert(index.children[0].child == leaf1_off);
        assert(index.children[1].child == leaf2_off);
        assert(index.children[2].child == leaf3_off);
        assert(leaf1.next == leaf2_off);
        assert(leaf2.next == leaf3_off);
        assert(leaf3.next == 0);
        PRINT("SplitLeafBy3");
    }

    {
        bpt tree("test.db", true);
        assert(tree.meta.order == 4);
        assert(tree.insert("t00", 0) == 0);
        assert(tree.insert("t01", 1) == 0);
        assert(tree.insert("t02", 2) == 0);
        assert(tree.insert("t03", 3) == 0);
        assert(tree.insert("t04", 4) == 0);
        assert(tree.insert("t05", 5) == 0);
        assert(tree.insert("t06", 6) == 0);
        assert(tree.insert("t07", 7) == 0);
        assert(tree.insert("t08", 8) == 0);
        assert(tree.insert("t09", 9) == 0);
        assert(tree.insert("t10", 10) == 0);
        assert(tree.insert("t11", 11) == 0);
        assert(tree.insert("t12", 12) == 0);
        assert(tree.insert("t13", 13) == 0);
        assert(tree.insert("t14", 14) == 0);
    }

    {
        bpt tree("test.db");
        assert(tree.meta.order == 4);
        assert(tree.meta.internal_node_num == 3);
        assert(tree.meta.leaf_node_num == 5);
        assert(tree.meta.height == 2);

        BPT::internal_node_t node1, node2, root;
        tree.read(&root, tree.meta.root_offset);
        off_t node1_off = tree.search_index("t03");
        off_t node2_off = tree.search_index("t14");
        tree.read(&node1, node1_off);
        tree.read(&node2, node2_off);
        assert(root.n == 2);
        assert(root.children[0].child == node1_off);
        assert(root.children[1].child == node2_off);
        assert(BPT::keycmp_ut(root.children[0].key, "t09") == 0);
        assert(node1.n == 3);
        assert(BPT::keycmp_ut(node1.children[0].key, "t03") == 0);
        assert(BPT::keycmp_ut(node1.children[1].key, "t06") == 0);
        assert(node2.n == 2);
        assert(BPT::keycmp_ut(node2.children[0].key, "t12") == 0);

        BPT::value_t value;
        for (int i = 0; i < 10; i++)
        {
            char key[8] = {0};
            sprintf(key, "t0%d", i);
            assert(tree.search(key, &value) == 0);
            assert(value == i);
        }
        for (int i = 10; i < 14; i++)
        {
            char key[8] = {0};
            sprintf(key, "t%d", i);
            assert(tree.search(key, &value) == 0);
            assert(value == i);
        }

        PRINT("CreateNewRoot");
    }

    int numbers[size];
    for (int i = 0; i < size; i++)
        numbers[i] = i;

    {
        bpt tree("test.db", true);
        for (int i = 0; i < size; i++)
        {
            char key[8] = {0};
            sprintf(key, "%d", numbers[i]);
            assert(tree.insert(key, numbers[i]) == 0);
        }
    }

    {
        bpt tree("test.db");
        for (int i = 0; i < size; i++)
        {
            char key[8] = {0};
            sprintf(key, "%d", numbers[i]);
            BPT::value_t value;
            assert(tree.search(key, &value) == 0);
            assert(value == numbers[i]);
        }
        PRINT("InsertManyKeys");
    }

    std::reverse(numbers, numbers + size);
    {
        bpt tree("test.db", true);
        for (int i = 0; i < size; i++)
        {
            char key[8] = {0};
            sprintf(key, "%d", numbers[i]);
            assert(tree.insert(key, numbers[i]) == 0);
        }
    }

    {
        bpt tree("test.db");
        for (int i = 0; i < size; i++)
        {
            char key[8] = {0};
            sprintf(key, "%d", numbers[i]);
            BPT::value_t value;
            assert(tree.search(key, &value) == 0);
            assert(value == numbers[i]);
        }
        PRINT("InsertManyKeysReverse");
    }

    for (int i = 0; i < 10; i++)
    {
        std::random_shuffle(numbers, numbers + size);
        {
            bpt tree("test.db", true);
            for (int i = 0; i < size; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", numbers[i]);
                assert(tree.insert(key, numbers[i]) == 0);
            }
        }

        {
            bpt tree("test.db");
            for (int i = 0; i < size; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", numbers[i]);
                BPT::value_t value;
                assert(tree.search(key, &value) == 0);
                assert(value == numbers[i]);
            }
        }
    }

    PRINT("InsertManyKeysRandom");

    {
        for (int i = 0; i < size; i++)
            numbers[i] = i;

        bpt tree("test.db", true);
        for (int i = 0; i < size; i++)
        {
            char key[8] = {0};
            sprintf(key, "%04d", numbers[i]);
            assert(tree.insert(key, numbers[i]) == 0);
        }
    }

    {
        bpt tree("test.db");
        int start = rand() % (size - 20);
        int end = rand() % (size - start) + start;
        char bufkey1[8] = {0};
        char bufkey2[8] = {0};
        sprintf(bufkey1, "%04d", start);
        sprintf(bufkey2, "%04d", end);
        BPT::key_t key1(bufkey1), key2(bufkey2);
        BPT::value_t values[end - start + 1];
        assert(tree.search_range(&key1, key2, values, end - start + 1) == end - start + 1);

        for (int i = start; i <= end; i++)
        {
            char key[8] = {0};
            sprintf(key, "%04d", i);
            assert(i == values[i - start]);
        }

        bool next;
        assert(tree.search_range(&key1, key2, values, end - start + 100) == end - start + 1);
        assert(tree.search_range(&key1, key2, values, end - start + 100, &next) == end - start + 1);
        assert(next == false);

        PRINT("SearchRangeSuccess");

        assert(tree.search_range(&key1, key1, values, end - start + 1) == 1);
        assert(tree.search_range(&key1, key1, values, end - start + 1, &next) == 1);
        assert(next == false);
        assert(tree.search_range(&key2, key2, values, end - start + 1) == 1);
        assert(tree.search_range(&key2, key2, values, end - start + 1, &next) == 1);
        assert(next == false);
        PRINT("SearchRangeSameKey");

        assert(tree.search_range(&key2, key1, values, end - start + 1) == -1);
        assert(tree.search_range(&key1, key2, values, end - start) == end - start);
        assert(tree.search_range(&key1, key2, values, end - start, &next) == end - start);
        assert(next == true);

        PRINT("SearchRangeFailed");
    }

    for (int i = 0; i < 2; i++)
    {
        std::random_shuffle(numbers, numbers + size);
        {
            bpt tree("test.db", true);
            for (int i = 0; i < size; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", numbers[i]);
                assert(tree.insert(key, numbers[i]) == 0);
            }
        }

        {
            bpt tree("test.db");
            std::random_shuffle(numbers, numbers + size);
            for (int i = 0; i < size; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", numbers[i]);
                assert(tree.update(key, numbers[i] + 1) == 0);
            }
        }

        {
            bpt tree("test.db");
            for (int i = 0; i < size; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", numbers[i]);
                BPT::value_t value;
                assert(tree.search(key, &value) == 0);
                assert(value == numbers[i] + 1);
            }

            for (int i = size; i < size * 2; i++)
            {
                char key[8] = {0};
                sprintf(key, "%d", i);
                BPT::value_t value;
                assert(tree.search(key, &value) != 0);
                assert(tree.update(key, i) != 0);
            }
        }
    }

    PRINT("UpdateManyKeysRandom");

    {
        bpt tree("test.db");
        BPT::leaf_node_t leaf;
        off_t offset = tree.meta.leaf_offset;
        off_t last = 0;
        size_t counter = 0;
        while (offset != 0)
        {
            tree.read(&leaf, offset);
            ++counter;
            assert(last == leaf.prev);
            last = offset;
            offset = leaf.next;
        }
        assert(counter == tree.meta.leaf_node_num);

        PRINT("LeafsList");
    }

    {
        bpt tree("test.db", true);
        assert(tree.insert("t2", 2) == 0);
        assert(tree.insert("t4", 4) == 0);
        assert(tree.insert("t1", 1) == 0);
        assert(tree.insert("t3", 3) == 0);
    }

    {
        bpt tree("test.db");
        assert(tree.remove("t9") != 0);
        assert(tree.remove("t3") == 0);
        assert(tree.remove("t3") != 0);

        BPT::leaf_node_t leaf;
        tree.read(&leaf, tree.meta.leaf_offset);
        assert(leaf.n == 3);
        assert(BPT::keycmp_ut(leaf.children[0].key, "t1") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "t2") == 0);
        assert(BPT::keycmp_ut(leaf.children[2].key, "t4") == 0);
        assert(tree.remove("t1") == 0);
        tree.read(&leaf, tree.meta.leaf_offset);
        assert(leaf.n == 2);
        assert(BPT::keycmp_ut(leaf.children[0].key, "t2") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "t4") == 0);
        assert(tree.remove("t2") == 0);
        tree.read(&leaf, tree.meta.leaf_offset);
        assert(leaf.n == 1);
        assert(BPT::keycmp_ut(leaf.children[0].key, "t4") == 0);
        assert(tree.remove("t4") == 0);
        tree.read(&leaf, tree.meta.leaf_offset);
        assert(leaf.n == 0);
        assert(tree.remove("t4") != 0);

        PRINT("RemoveInRootLeaf");
    }

    {
        bpt tree("test.db", true);
        for (int i = 0; i < 10; i++)
        {
            char key[8] = {0};
            sprintf(key, "%02d", i);
            assert(tree.insert(key, i) == 0);
        }
    }

    {
        bpt tree("test.db");
        BPT::leaf_node_t leaf;
        BPT::internal_node_t node;
        assert(tree.meta.leaf_node_num == 3);
        assert(tree.meta.internal_node_num == 1);

        // | 3 6  |
        // | 0 1 2 | 3 4 5 | 6 7 8 9 |
        tree.read(&node, tree.meta.root_offset);
        assert(BPT::keycmp_ut(node.children[0].key, "03") == 0);
        assert(BPT::keycmp_ut(node.children[1].key, "06") == 0);
        assert(tree.remove("03") == 0);
        assert(tree.remove("04") == 0);
        // | 2 6  |
        // | 0 1 | 2 5 | 6 7 8 9 |
        tree.read(&node, tree.meta.root_offset);
        assert(BPT::keycmp_ut(node.children[0].key, "02") == 0);
        assert(BPT::keycmp_ut(node.children[1].key, "06") == 0);
        tree.read(&leaf, tree.search_leaf("00"));
        assert(leaf.parent == tree.meta.root_offset);
        assert(leaf.n == 2);
        assert(BPT::keycmp_ut(leaf.children[0].key, "00") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "01") == 0);
        tree.read(&leaf, tree.search_leaf("05"));
        assert(leaf.parent == tree.meta.root_offset);
        assert(leaf.n == 2);
        assert(BPT::keycmp_ut(leaf.children[0].key, "02") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "05") == 0);
        assert(tree.remove("05") == 0);
        // | 2 7  |
        // | 0 1 | 2 6 | 7 8 9 |
        tree.read(&node, tree.meta.root_offset);
        assert(node.n == 3);
        assert(BPT::keycmp_ut(node.children[0].key, "02") == 0);
        assert(BPT::keycmp_ut(node.children[1].key, "07") == 0);
        tree.read(&leaf, tree.search_leaf("04"));
        assert(leaf.parent == tree.meta.root_offset);
        assert(leaf.n == 2);
        assert(BPT::keycmp_ut(leaf.children[0].key, "02") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "06") == 0);
        tree.read(&leaf, tree.search_leaf("07"));
        assert(leaf.parent == tree.meta.root_offset);
        assert(leaf.n == 3);
        assert(BPT::keycmp_ut(leaf.children[0].key, "07") == 0);
        assert(BPT::keycmp_ut(leaf.children[1].key, "08") == 0);
        assert(BPT::keycmp_ut(leaf.children[2].key, "09") == 0);

        BPT::value_t value;
        assert(tree.search("00", &value) == 0);
        assert(value == 0);
        assert(tree.search("01", &value) == 0);
        assert(value == 1);
        assert(tree.search("02", &value) == 0);
        assert(value == 2);
        assert(tree.search("03", &value) != 0);
        assert(tree.search("04", &value) != 0);
        assert(tree.search("05", &value) != 0);
        assert(tree.search("06", &value) == 0);
        assert(value == 6);
        assert(tree.search("07", &value) == 0);
        assert(value == 7);
        assert(tree.search("08", &value) == 0);
        assert(value == 8);
        assert(tree.search("09", &value) == 0);
        assert(value == 9);

        PRINT("RemoveWithBorrow");
    }
    unlink("test.db");

    return 0;
}
