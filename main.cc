/*******************************************************
                          main.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include <fstream>
using namespace std;

#include "cache.h"

int main(int argc, char *argv[])
{

    ifstream fin;
    FILE *pFile;

    if (argv[1] == NULL)
    {
        printf("input format: ");
        printf("./smp_cache <cache_size> <assoc> <block_size> <num_processors> <protocol> <trace_file> \n");
        exit(0);
    }

    ulong cache_size = atoi(argv[1]);
    ulong cache_assoc = atoi(argv[2]);
    ulong blk_size = atoi(argv[3]);
    ulong num_processors = atoi(argv[4]);
    int protocol = atoi(argv[5]); /* 0:MSI 1:MSI BusUpgr 2:MESI 3:MESI Snoop FIlter */
    char *fname = (char *)malloc(20);
    fname = argv[6];

    printf("===== 506 Coherence Simulator Configuration =====\n");
    // print out simulator configuration here
    printf("L1_SIZE: %lu\n", cache_size);
    printf("L1_ASSOC: %lu\n", cache_assoc);
    printf("L1_BLOCKSIZE: %lu\n", blk_size);
    printf("NUMBER OF PROCESSORS: %lu\n", num_processors);
    if (protocol == 0)
        printf("COHERENCE PROTOCOL: MSI\n");
    else if (protocol == 1)
        printf("COHERENCE PROTOCOL: MSI BusUpgr\n");
    else if (protocol == 2)
        printf("COHERENCE PROTOCOL: MESI\n");
    else if (protocol == 3)
        printf("COHERENCE PROTOCOL: MESI Filter\n");
    printf("TRACE FILE: %s\n", fname);

    // Using pointers so that we can use inheritance */
    Cache **cacheArray = (Cache **)malloc(num_processors * sizeof(Cache));
    for (ulong i = 0; i < num_processors; i++)
        cacheArray[i] = new Cache(cache_size, cache_assoc, blk_size);

    ulong Filter_size = 16 * blk_size;
    ulong Filter_assoc = 1;
    Cache **SnoopFilterCacheArray = (Cache **)malloc(num_processors * sizeof(Cache));
    if (protocol == 3) // MESIFilter
    {
        for (ulong i = 0; i < num_processors; i++)
            SnoopFilterCacheArray[i] = new Cache(Filter_size, Filter_assoc, blk_size);
    }

    pFile = fopen(fname, "r");
    if (pFile == 0)
    {
        printf("Trace file problem\n");
        exit(0);
    }

    ulong proc;
    char op;
    ulong addr;

    int line = 1;
    while (fscanf(pFile, "%lu %c %lx", &proc, &op, &addr) != EOF)
    {
#ifdef _DEBUG
        // printf("%d\n", line);
#endif
        // propagate request down through memory hierarchy
        // by calling cachesArray[processor#]->Access(...)

        bool NotExclusiveFlag = false;
        if ((protocol == 2) || (protocol == 3)) // MESI, MESIFilter
        {
            for (ulong i = 0; i < num_processors; i++)
            {
                if (i != proc)
                    NotExclusiveFlag = NotExclusiveFlag || (!cacheArray[i]->CacheMiss(addr));
            }
        }

        cacheArray[proc]->Access(addr, op, protocol, true, NothingBusTrx, NotExclusiveFlag);
        if (protocol == 3) // MESIFilter
            SnoopFilterCacheArray[proc]->InvalidateLine(addr);

        for (ulong i = 0; i < num_processors; i++)
        {
            if (i != proc)
            {
                if ((protocol == 0) || (protocol == 1) || (protocol == 2)) // MSI,MSIUpgr,MESI
                    cacheArray[i]->Access(addr, op, protocol, false, cacheArray[proc]->getBusTrans(), false);
                else if (protocol == 3) // MESIFilter
                {
                    if (SnoopFilterCacheArray[i]->CacheMiss(addr))
                    {
                        if (cacheArray[i]->Access(addr, op, protocol, false, cacheArray[proc]->getBusTrans(), false))
                            SnoopFilterCacheArray[i]->fillLine(addr);
                    }
                    else
                        cacheArray[i]->IncrementFilterSnoops();
                }
            }
        }
        line++;
    }
    fclose(pFile);

    //********************************//
    // print out all caches' statistics //
    //********************************//
    for (ulong i = 0; i < num_processors; i++)
        cacheArray[i]->printStats(i, protocol);
}
