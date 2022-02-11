#ifndef CACHE_SET_SDBP_H
#define CACHE_SET_SDBP_H

#include "cache_set.h"

typedef struct
{
   UInt8 LRUstackposition;
   bool prediction;
} LINE_REPLACEMENT_STATE;


class CacheSetInfoSDBP : public CacheSetInfo
{
   public:
      CacheSetInfoSDBP(String name, String cfgname, core_id_t core_id, UInt32 associativity, UInt8 num_attempts);
      virtual ~CacheSetInfoSDBP();
      void increment(UInt32 index)
      {
         LOG_ASSERT_ERROR(index < m_associativity, "Index(%d) >= Associativity(%d)", index, m_associativity);
         ++m_access[index];
      }
      void incrementAttempt(UInt8 attempt)
      {
         if (m_attempts)
            ++m_attempts[attempt];
         else
            LOG_ASSERT_ERROR(attempt == 0, "No place to store attempt# histogram but attempt != 0");
      }
   private:
      const UInt32 m_associativity;
      UInt64* m_access;
      UInt64* m_attempts;
};

class CacheSetSDBP : public CacheSet
{
   public:
      CacheSetSDBP(CacheBase::cache_t cache_type,
            UInt32 associativity, UInt32 blocksize, CacheSetInfoSDBP* set_info, UInt8 num_attempts);
      virtual ~CacheSetSDBP();

      virtual UInt32 getReplacementIndex(CacheCntlr *cntlr);
      void updateReplacementIndex(UInt32 accessed_index);
      // updated by prajwal
      virtual void update_prediction(UInt32 way, bool pred_val);

   protected:
      const UInt8 m_num_attempts;
      LINE_REPLACEMENT_STATE *sdbp_cache_set;
      // UInt8* m_lru_bits;
      CacheSetInfoSDBP* m_set_info;
      void moveToMRU(UInt32 accessed_index);
};

#endif /* CACHE_SET_SDBP_H */
