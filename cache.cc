/*******************************************************
                          cache.cc
********************************************************/

#include <stdlib.h>
#include <assert.h>
#include "cache.h"
using namespace std;

Cache::Cache(int s, int a, int b)
{
   ulong i, j;
   reads = readMisses = writes = 0;
   writeMisses = writeBacks = currentCycle = 0;

   size = (ulong)(s);
   lineSize = (ulong)(b);
   assoc = (ulong)(a);
   sets = (ulong)((s / b) / a);
   numLines = (ulong)(s / b);
   log2Sets = (ulong)(log2(sets));
   log2Blk = (ulong)(log2(b));

   //*******************//
   // initialize your counters here//
   //*******************//
   BusTrans = NothingBusTrx;
   CachetoCacheTransfers = MemoryTransactions = Interventions = Invalidations = FlushestoMainMemory = BusRdxCount = BusUpgrCount = UsefulSnoops = WastedSnops = FilteredSnoops = 0;

   tagMask = 0;
   for (i = 0; i < log2Sets; i++)
   {
      tagMask <<= 1;
      tagMask |= 1;
   }

   /**create a two dimentional cache, sized as cache[sets][assoc]**/
   cache = new cacheLine *[sets];
   for (i = 0; i < sets; i++)
   {
      cache[i] = new cacheLine[assoc];
      for (j = 0; j < assoc; j++)
      {
         cache[i][j].invalidate();
      }
   }
}

/**you might add other parameters to Access()
since this function is an entry point
to the memory hierarchy (i.e. caches)**/
bool Cache::CacheMiss(ulong addr)
{
   cacheLine *line = findLine(addr);
   if (line == NULL) /*miss*/
      return true;
   else /*hit*/
      return false;
}

bool Cache::Access(ulong addr, uchar op, int protocol, bool RequestorFlag, ulong BusCmd, bool NotExclusiveFlag)
{
   bool SnooperInvalidationFlag = false;
   if (!RequestorFlag) // Snooper Side
   {
      cacheLine *line = findLine(addr);
      if (line != NULL) /*hit*/
         UsefulSnoops++;
      else /*Miss*/
      {
         WastedSnops++;
         SnooperInvalidationFlag = true;
      }

      if ((BusCmd == BusRdx) || (BusCmd == BusUpgr))
      {
         if (line != NULL) /*hit*/
         {
            /**since it's a hit, Invalidate Tag**/
            if ((line->getTagState() == ModifiedState))
            {
               writeBacks++;
               FlushestoMainMemory++;
               MemoryTransactions++;
            }
            SnooperInvalidationFlag = true;
            line->invalidate();
            Invalidations++;
         }
      }
      else if (BusCmd == BusRd)
      {
         if (line != NULL) /*hit*/
         {
            /**since it's a hit, if it is in Modified/Exclusive State, change to Shared State**/
            if (line->getTagState() == ExclusiveState)
            {
               line->setTagState(SharedState);
               Interventions++;
            }
            else if (line->getTagState() == ModifiedState)
            {
               line->setTagState(SharedState);
               line->setFlags(VALID);
               Interventions++;
               writeBacks++;
               FlushestoMainMemory++;
               MemoryTransactions++;
            }
         }
      }
   }

   else // Requestor Side
   {
      currentCycle++; /*per cache global counter to maintain LRU order
                        among cache ways, updated on every cache access*/
      if (op == 'w')
         writes++;
      else
         reads++;
      cacheLine *line = findLine(addr);
      if (line == NULL) /*miss*/
      {
         if ((protocol == 0) || (protocol == 1))
            MemoryTransactions++;
         cacheLine *newline = fillLine(addr);

         if (op == 'w')
         {
            if ((protocol == 0) || (protocol == 1) || (protocol == 2) || (protocol == 3)) // MSI, MSIUpgr, MESI, MESIFilter
            {
               BusTrans = BusRdx;
               BusRdxCount++;
               newline->setTagState(ModifiedState);
               newline->setFlags(DIRTY);
               if ((protocol == 2) || (protocol == 3)) // MESI, MESIFilter
               {
                  if (NotExclusiveFlag)
                     CachetoCacheTransfers++;
                  else
                     MemoryTransactions++;
               }
            }
            writeMisses++;
         }
         else
         {
            BusTrans = BusRd;
            if ((protocol == 0) || (protocol == 1)) // MSI, MSIUpgr
               newline->setTagState(SharedState);
            else if ((protocol == 2) || (protocol == 3)) // MESI, MESIFilter
            {
               if (NotExclusiveFlag)
               {
                  CachetoCacheTransfers++;
                  newline->setTagState(SharedState);
               }
               else
               {
                  newline->setTagState(ExclusiveState);
                  MemoryTransactions++;
               }
            }
            readMisses++;
         }
      }
      else
      {
         /**since it's a hit, update LRU and update dirty flag**/
         updateLRU(line);
         if (op == 'w')
         {
            if (line->getTagState() == SharedState)
            {
               if (protocol == 0) // MSI
               {
                  MemoryTransactions++;
                  BusTrans = BusRdx;
                  BusRdxCount++;
               }
               else if ((protocol == 1) || (protocol == 2) || (protocol == 3)) // MSIUpgr, MESI, MESIFilter
               {
                  BusTrans = BusUpgr;
                  BusUpgrCount++;
               }
            }
            else if ((line->getTagState() == ModifiedState) || (line->getTagState() == ExclusiveState))
               BusTrans = NothingBusTrx;
            line->setTagState(ModifiedState);
            line->setFlags(DIRTY);
         }
         else
         {
            BusTrans = NothingBusTrx;
            // TagState is not modified
         }
      }
   }
   return SnooperInvalidationFlag;
}

/*delete line*/
void Cache::InvalidateLine(ulong addr)
{
   ulong i, j, tag, pos;

   pos = assoc;
   tag = calcTag(addr);
   i = calcIndex(addr);

   for (j = 0; j < assoc; j++)
      if (cache[i][j].isValid())
      {
         if (cache[i][j].getTag() == tag)
         {
            pos = j;
            break;
         }
      }
   if (pos != assoc)
      cache[i][pos].invalidate();
}

/*look up line*/
cacheLine *Cache::findLine(ulong addr)
{
   ulong i, j, tag, pos;

   pos = assoc;
   tag = calcTag(addr);
   i = calcIndex(addr);

   for (j = 0; j < assoc; j++)
      if (cache[i][j].isValid())
      {
         if (cache[i][j].getTag() == tag)
         {
            pos = j;
            break;
         }
      }
   if (pos == assoc)
   {
      return NULL;
   }
   else
   {
      return &(cache[i][pos]);
   }
}

/*upgrade LRU line to be MRU line*/
void Cache::updateLRU(cacheLine *line)
{
   line->setSeq(currentCycle);
}

/*return an invalid line as LRU, if any, otherwise return LRU line*/
cacheLine *Cache::getLRU(ulong addr)
{
   ulong i, j, victim, min;

   victim = assoc;
   min = currentCycle;
   i = calcIndex(addr);

   for (j = 0; j < assoc; j++)
   {
      if (cache[i][j].isValid() == 0)
      {
         return &(cache[i][j]);
      }
   }

   for (j = 0; j < assoc; j++)
   {
      if (cache[i][j].getSeq() <= min)
      {
         victim = j;
         min = cache[i][j].getSeq();
      }
   }

   assert(victim != assoc);

   return &(cache[i][victim]);
}

/*find a victim, move it to MRU position*/
cacheLine *Cache::findLineToReplace(ulong addr)
{
   cacheLine *victim = getLRU(addr);
   updateLRU(victim);

   return (victim);
}

/*allocate a new line*/
cacheLine *Cache::fillLine(ulong addr)
{
   ulong tag;

   cacheLine *victim = findLineToReplace(addr);
   assert(victim != 0);

   if (victim->getFlags() == DIRTY)
   {
      writeBack(addr);
      MemoryTransactions++;
   }

   tag = calcTag(addr);
   victim->setTag(tag);
   victim->setFlags(VALID);
   /**note that this cache line has been already
      upgraded to MRU in the previous function (findLineToReplace)**/

   return victim;
}

void Cache::printStats(ulong CacheNumber, ulong Protocol)
{
   printf("============ Simulation results (Cache %lu) ============\n", CacheNumber);
   // printf("===== Simulation results (Cache %lu) =====\n", CacheNumber);
   printf("01. number of reads: %lu\n", reads);
   printf("02. number of read misses: %lu\n", readMisses);
   printf("03. number of writes: %lu\n", writes);
   printf("04. number of write misses: %lu\n", writeMisses);
   printf("05. total miss rate: %.2f%%\n", (((double(readMisses + writeMisses) / double(reads + writes))) * 100));
   printf("06. number of writebacks: %lu\n", writeBacks);
   printf("07. number of cache-to-cache transfers: %lu\n", CachetoCacheTransfers);
   printf("08. number of memory transactions: %lu\n", MemoryTransactions);
   printf("09. number of interventions: %lu\n", Interventions);
   printf("10. number of invalidations: %lu\n", Invalidations);
   printf("11. number of flushes: %lu\n", FlushestoMainMemory);
   printf("12. number of BusRdX: %lu\n", BusRdxCount);
   printf("13. number of BusUpgr: %lu\n", BusUpgrCount);
   if (Protocol == 3) // MESIFilter
   {
      printf("14. number of useful snoops: %lu\n", UsefulSnoops);
      printf("15. number of wasted snoops: %lu\n", WastedSnops);
      printf("16. number of filtered snoops: %lu\n", FilteredSnoops);
   }
}

/*void Cache::printStats(ulong CacheNumber, ulong Protocol)
{
   printf("============ Simulation results (Cache %lu) ============\n", CacheNumber);
   printf("%lu\n", reads);
   printf("%lu\n", readMisses);
   printf("%lu\n", writes);
   printf("%lu\n", writeMisses);
   printf("%.2f%%\n", (((double(readMisses + writeMisses) / double(reads + writes))) * 100));
   printf("%lu\n", writeBacks);
   printf("%lu\n", CachetoCacheTransfers);
   printf("%lu\n", MemoryTransactions);
   printf("%lu\n", Interventions);
   printf("%lu\n", Invalidations);
   printf("%lu\n", FlushestoMainMemory);
   printf("%lu\n", BusRdxCount);
   printf("%lu\n", BusUpgrCount);
   printf("%lu\n", UsefulSnoops);
   printf("%lu\n", WastedSnops);
   printf("%lu\n", FilteredSnoops);
}*/
