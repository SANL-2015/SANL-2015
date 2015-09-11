/* ########################################################################## */
/* mcs_lock.h                                                                 */
/* (C) Jean-Pierre Lozi, 2010-2011                                            */
/* (C) Gaël Thomas, 2010-2011                                                 */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
typedef struct mcs_lock_t mcs_lock_t;
struct mcs_lock_t
{
	mcs_lock_t *next;
	int spin;
};
typedef struct mcs_lock_t *mcs_lock;

void lock_mcs(mcs_lock *m, mcs_lock_t *me);
void unlock_mcs(mcs_lock *m, mcs_lock_t *me);
int trylock_mcs(mcs_lock *m, mcs_lock_t *me);

