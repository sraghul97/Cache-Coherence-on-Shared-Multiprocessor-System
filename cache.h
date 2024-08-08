/*******************************************************
                          cache.h
********************************************************/

#ifndef CACHE_H
#define CACHE_H

#include <cmath>
#include <iostream>
#include <string>
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bitset>
using namespace std;

typedef unsigned long ulong;
typedef unsigned char uchar;
typedef unsigned int uint;

/****add new states, based on the protocol****/

enum
{
   INVALID = 0,
   VALID,
   DIRTY
};
enum
{
   NothingBusTrx = 0,
   BusRd,
   BusRdx,
   BusUpgr
};
enum
{
   NothingState = 0,
   ModifiedState,
   ExclusiveState,
   SharedState,
   InvalidState
};

class cacheLine
{
protected:
   ulong tag;
   ulong Flags; // 0:invalid, 1:valid, 2:dirty
   ulong seq;
   ulong TagState;

public:
   cacheLine()
   {
      tag = 0;
      Flags = 0;
      TagState = 0;
   }
   ulong getTag() { return tag; }
   ulong getFlags() { return Flags; }
   ulong getSeq() { return seq; }
   ulong getTagState() { return TagState; }
   void setSeq(ulong Seq) { seq = Seq; }
   void setFlags(ulong flags) { Flags = flags; }
   void setTag(ulong a) { tag = a; }
   void setTagState(ulong NewTagState) { TagState = NewTagState; }
   void invalidate()
   {
      tag = 0;
      Flags = INVALID;
      TagState = InvalidState;
   } // useful function
   bool isValid() { return ((Flags) != INVALID); }
};

class Cache
{
protected:
   ulong size, lineSize, assoc, sets, log2Sets, log2Blk, tagMask, numLines;
   ulong reads, readMisses, writes, writeMisses, writeBacks;

   //******///
   // add coherence counters here///
   //******///
   ulong CachetoCacheTransfers, MemoryTransactions, Interventions, Invalidations, FlushestoMainMemory, BusRdxCount, BusUpgrCount, UsefulSnoops, WastedSnops, FilteredSnoops;
   ulong BusTrans;

   cacheLine **cache;
   ulong calcTag(ulong addr) { return (addr >> (log2Blk)); }
   ulong calcIndex(ulong addr) { return ((addr >> log2Blk) & tagMask); }
   ulong calcAddr4Tag(ulong tag) { return (tag << (log2Blk)); }

public:
   ulong currentCycle;

   Cache(int, int, int);
   ~Cache() { delete cache; }

   cacheLine *findLineToReplace(ulong addr);
   cacheLine *fillLine(ulong addr);
   cacheLine *findLine(ulong addr);
   cacheLine *getLRU(ulong);

   ulong getRM() { return readMisses; }
   ulong getWM() { return writeMisses; }
   ulong getReads() { return reads; }
   ulong getWrites() { return writes; }
   ulong getWB() { return writeBacks; }

   void writeBack(ulong) { writeBacks++; }
   bool Access(ulong, uchar, int, bool, ulong = NothingBusTrx, bool = false);
   void printStats(ulong, ulong);
   void updateLRU(cacheLine *);

   //******///
   // add other functions to handle bus transactions///
   //******///
   bool CacheMiss(ulong);
   ulong getBusTrans() { return BusTrans; }
   void IncrementFilterSnoops() { FilteredSnoops++; }
   void setBusTrans(char BusCmd) { BusTrans = BusCmd; }
   void InvalidateLine(ulong addr);
   ulong GetCacheTag(ulong Index)
   {
      return cache[Index][0].getTag();
   }
};

#endif
