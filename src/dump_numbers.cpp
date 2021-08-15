#include "../include/bpt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <algorithm>
int main(int argc, char *argv[])
{
    clock_t start_time, end_time;
    int start = 0;
    int end = 90000000;

    if (argc > 2)
        start = atoi(argv[2]);
    //atoi()将数字格式的字符串转换为整数类型。
    if (argc > 3)
        end = atoi(argv[3]);

    if (argc != 4 || start >= end)
    {
        fprintf(stderr, "usage: %s database [start] [end] \n", argv[0]);
        return 1;
    }
    {
        start_time = clock();
        BPT::bpt database(argv[1], true);
        for (int i = start; i <= end; i++)
        {
            if (i % 10000 == 0)
                printf("%d\n", i);
            char key[16] = {0};
            sprintf(key, "%d", i);
            database.insert(key, i);
        }
        printf("%d\n", end);
        end_time = clock();
        std::cout << "顺序插入运行时间" << (double)(end_time - start_time) / CLOCKS_PER_SEC << std::endl;
    }
    {
        int length = end - start + 1, i, j;
        int array[length];
        for (i = 0, j = start; i <= length - 1; i++, j++)
        {
            array[i] = j;
        }
        std::random_shuffle(array, array + length);
        start_time = clock();
        BPT::bpt database(argv[1], true);
        for (i = 0; i <= length - 1; i++)
        {
            if (i  % 10000 == 0)
                printf("%d\n", i);
            char key[16] = {0};
            sprintf(key, "%d", array[i]);
            database.insert(key, array[i]);
        }
        printf("%d\n", end);
        end_time = clock();
        std::cout << "乱序插入运行时间" << (double)(end_time - start_time) / CLOCKS_PER_SEC << std::endl;
    }
    return 0;
}
