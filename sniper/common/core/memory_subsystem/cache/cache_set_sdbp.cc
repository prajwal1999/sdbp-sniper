#include "cache_set_sdbp.h"
#include "log.h"
#include "stats.h"

// Implements SDBP replacement, optionally augmented with Query-Based Selection [Jaleel et al., MICRO'10]

CacheSetSDBP::CacheSetSDBP(
      CacheBase::cache_t cache_type,
      UInt32 associativity, UInt32 blocksize, CacheSetInfoSDBP* set_info, UInt8 num_attempts)
   : CacheSet(cache_type, associativity, blocksize)
   , m_num_attempts(num_attempts)
   , m_set_info(set_info)
{
   // m_lru_bits = new UInt8[m_associativity];
   // updated by prajwal
   sdbp_cache_set = new LINE_REPLACEMENT_STATE [m_associativity];
   for (UInt32 i = 0; i < m_associativity; i++)
      // m_lru_bits[i] = i;
      sdbp_cache_set[i].LRUstackposition = i;
}

CacheSetSDBP::~CacheSetSDBP()
{
   // delete [] m_lru_bits;
   delete [] sdbp_cache_set;
}

UInt32
CacheSetSDBP::getReplacementIndex(CacheCntlr *cntlr)
{
   // First try to find an invalid block
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (!m_cache_block_info_array[i]->isValid())
      {
         // Mark our newly-inserted line as most-recently used
         moveToMRU(i);
         return i;
      }
   }

   // Make m_num_attemps attempts at evicting the block at SDBP position
   for(UInt8 attempt = 0; attempt < m_num_attempts; ++attempt)
   {
      UInt32 index = 0;
      UInt8 max_bits = 0;
      for (UInt32 i = 0; i < m_associativity; i++)
      {
         if (sdbp_cache_set[i].LRUstackposition > max_bits && isValidReplacement(i))
         {
            index = i;
            max_bits = sdbp_cache_set[i].LRUstackposition;
         }
      }
      LOG_ASSERT_ERROR(index < m_associativity, "Error Finding SDBP bits");

      bool qbs_reject = false;
      if (attempt < m_num_attempts - 1)
      {
         LOG_ASSERT_ERROR(cntlr != NULL, "CacheCntlr == NULL, QBS can only be used when cntlr is passed in");
         qbs_reject = cntlr->isInLowerLevelCache(m_cache_block_info_array[index]);
      }

      if (qbs_reject)
      {
         // Block is contained in lower-level cache, and we have more tries remaining.
         // Move this block to MRU and try again
         moveToMRU(index);
         cntlr->incrementQBSLookupCost();
         continue;
      }
      else
      {
         // Mark our newly-inserted line as most-recently used
         moveToMRU(index);
         m_set_info->incrementAttempt(attempt);
         return index;
      }
   }

   LOG_PRINT_ERROR("Should not reach here");
}

void
CacheSetSDBP::updateReplacementIndex(UInt32 accessed_index)
{
   m_set_info->increment(sdbp_cache_set[accessed_index].LRUstackposition);
   moveToMRU(accessed_index);
}

void
CacheSetSDBP::moveToMRU(UInt32 accessed_index)
{
   for (UInt32 i = 0; i < m_associativity; i++)
   {
      if (sdbp_cache_set[i].LRUstackposition < sdbp_cache_set[accessed_index].LRUstackposition)
         sdbp_cache_set[i].LRUstackposition ++;
   }
   sdbp_cache_set[accessed_index].LRUstackposition = 0;
}

// updated by prajwal
void CacheSetSDBP::update_prediction(UInt32 way, bool pred_val)
{
   sdbp_cache_set[way].prediction = pred_val;
}

CacheSetInfoSDBP::CacheSetInfoSDBP(String name, String cfgname, core_id_t core_id, UInt32 associativity, UInt8 num_attempts)
   : m_associativity(associativity)
   , m_attempts(NULL)
{
   m_access = new UInt64[m_associativity];
   for(UInt32 i = 0; i < m_associativity; ++i)
   {
      m_access[i] = 0;
      registerStatsMetric(name, core_id, String("access-mru-")+itostr(i), &m_access[i]);
   }

   if (num_attempts > 1)
   {
      m_attempts = new UInt64[num_attempts];
      for(UInt32 i = 0; i < num_attempts; ++i)
      {
         m_attempts[i] = 0;
         registerStatsMetric(name, core_id, String("qbs-attempt-")+itostr(i), &m_attempts[i]);
      }
   }
};

CacheSetInfoSDBP::~CacheSetInfoSDBP()
{
   delete [] m_access;
   if (m_attempts)
      delete [] m_attempts;
}
