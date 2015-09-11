
#include <machine/param.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int
main(void)
{
    int ret;
    int id;
    int i;

    for(i = 12; i< 20; i++)
    {
	id = shmget(i, 0, 0);
	if(id < 0)
	{
	    perror("shmget");
	    continue;
	}

	ret = shmctl(id, IPC_RMID, 0);
    
	if(ret)
	{
	    perror("shmctl");
	}
    }
}
