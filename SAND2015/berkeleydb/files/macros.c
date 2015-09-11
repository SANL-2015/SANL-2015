#if 0
#define MUTEX_LOCK_MOD(__ENV__, __MUT__) {	if ((__MUT__) != MUTEX_INVALID) {	\
  DB_ENV *dbenv;																\
  DB_MUTEX *mutexp;																\
																				\
  dbenv = (__ENV__)->dbenv;														\
  																				\
  /*if (MUTEX_ON(__ENV__) && !F_ISSET(dbenv, DB_ENV_NOLOCKING))*/					\
  {																				\
    mutexp = MUTEXP_SET((__ENV__), (__MUT__));									\
																				\
    /*CHECK_MTX_THREAD((__ENV__), mutexp);*/										\
																				\
    pthread_mutex_lock(&mutexp->u.m.mutex);										\
																				\
    /*F_SET(mutexp, DB_MUTEX_LOCKED);*/												\
    /*dbenv->thread_id(dbenv, &mutexp->pid, &mutexp->tid);*/						\
  }																				\
}}

#define MUTEX_UNLOCK_MOD(__ENV__, __MUT__) {  if ((__MUT__) != MUTEX_INVALID) { 	\
  DB_ENV *dbenv;																	\
  DB_MUTEX *mutexp;																	\
																					\
  dbenv = (__ENV__)->dbenv;															\
																					\
  /*if (MUTEX_ON(__ENV__) && !F_ISSET(dbenv, DB_ENV_NOLOCKING))*/						\
  {																					\
    mutexp = MUTEXP_SET((__ENV__), (__MUT__));										\
    /*F_CLR(mutexp, DB_MUTEX_LOCKED);*/													\
    pthread_mutex_unlock(&mutexp->u.m.mutex);										\
  }																					\
}}
#endif

#define MUTEX_LOCK_MOD(__ENV__, __MUT__) {                                  \
    if ((__MUT__) != MUTEX_INVALID) {                                       \
        pthread_mutex_lock(&MUTEXP_SET((__ENV__), (__MUT__))->u.m.mutex);   \
    }                                                                       \
}

#define MUTEX_UNLOCK_MOD(__ENV__, __MUT__) {                                \
    if ((__MUT__) != MUTEX_INVALID) {                                       \
        pthread_mutex_unlock(&MUTEXP_SET((__ENV__), (__MUT__))->u.m.mutex); \
    }                                                                       \
}

