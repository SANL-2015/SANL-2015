#ifndef __INLINE_H

#define __INLINE_H


int32_t sysCompareAndSwap(int32_t o0, int32_t o1, int32_t *o2);

/* 32-bit CAS
 */
#define CAS(addr,oldval,newval)                                               \
  (sysCompareAndSwap((int32_t)(newval), (int32_t)(oldval), (int32_t *)(addr)) \
   == (int32_t)(oldval))


#endif /* __INLINE_H */
