#define MLOCK(a,b) ({DB_MUTEX *mutexp = MUTEXP_SET(a,b); &mutexp->u.m.lmutex;})
#define __RETURN__(a) return a
#define liblock_execute_operation(a1, a2,  b, c) ({int64_t r; if ((a2) != MUTEX_INVALID) \
    r = (int64_t)liblock_exec(MLOCK((a1), (a2)), (c), (b)); else r = (int64_t)(c)(b); r;})

