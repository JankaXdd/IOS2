#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>

//1,000 mikrosekund = 1 milisekunda
#define MICROSEC_TO_MILISEC 1000

/*	Deklarace promennych ve sdilene pameti
*
*	f - vystupni soubor
*   hydrocount - pocet vodiku
*	oxycount - pocet kysliku
*	count - pocet atomu kysliku a vodiku
*   A - poradove cislo provadene akce
*	MOL_COUNT - pocet vytvorenych molekul
*/
FILE *f = NULL;
int *hydrocount = NULL, *oxycount = NULL, *count = NULL, *A = NULL, *MOL_COUNT = NULL;

/*  Deklarace globalnich promennych
*
*   NO - pocet kysliku
*   NH - pocet vodiku
*   TI - maximalni cas pro atom kysliku/vodiku pred zarazenim do fronty na vytvareni molekul
*   TB - maximalni cas pro vytvoreni jedne molekuly
*/ 
int NO, NH, TI, TB;

/*  Deklarace semaforu
*
*   mutex - zamek bariery pro sdilenou pamet 
*   turnstile - 1. turniket bariery 
*   turnstile2 - 2. turniket bariery
*   mutex_mol - zamek pro sdilenou pamet pri tvorbe molekuly
*   hydroque - semafor pro frontu vodiku
*   oxyque - semafor pro frontu kysliku
*   mutex_file - semafor pro zapis do souboru
*/
sem_t *mutex, *turnstile, *turnstile2, *mutex_mol, *hydroque, *oxyque, *mutex_file;

//Funkce pro inicializaci semaforu
int init_semaphores()
{
    mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(mutex, 1, 1);
    turnstile = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(turnstile, 1, 0);
    turnstile2 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(turnstile2, 1, 1);
    mutex_mol = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(mutex_mol, 1, 1);
    hydroque = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(hydroque, 1, 0);
    oxyque = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(oxyque, 1, 0);
    mutex_file = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(mutex_file, 1, 1);
    return 0;
}

//Uklid semaforu
void clean_semaphores()
{
    sem_destroy(mutex);
    sem_destroy(turnstile);
    sem_destroy(turnstile2);
    sem_destroy(mutex_mol);
    sem_destroy(hydroque);
    sem_destroy(oxyque);
    sem_destroy(mutex_file);

    munmap(mutex, sizeof(sem_t));
    munmap(turnstile, sizeof(sem_t));
    munmap(turnstile2, sizeof(sem_t));
    munmap(mutex_mol, sizeof(sem_t));
    munmap(hydroque, sizeof(sem_t));
    munmap(oxyque, sizeof(sem_t));
    munmap(mutex_file, sizeof(sem_t));

}

//Funkce pro inicializaci promennych
int init_variables()
{
    hydrocount = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    oxycount = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    count = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    A = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    MOL_COUNT = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *A = 1;
    *MOL_COUNT = 1;

	time_t t;
	srand((unsigned) time(&t));

    return 0;
}

//Uklid promennych
void clean_variables()
{
    munmap(hydrocount, sizeof(int));
    munmap(oxycount, sizeof(int));
    munmap(count, sizeof(int));
    munmap(A, sizeof(int));
    munmap(MOL_COUNT, sizeof(int));
}

//Funkce pro generaci nahodneho casu z intervalu
int RndTime(int max)
{
	return (rand() % (max + 1));
}

//Funkce ktera uvede proces do spanku
void MySleep(int time)
{
	usleep(time*MICROSEC_TO_MILISEC);
}

//Funkce pro proces "kyslík"
int oxygen(int id)
{
    sem_wait(mutex_file);
	fprintf(f, "%d: O %d: started\n", *A, id);
	*A += 1;
	sem_post(mutex_file);
    MySleep(RndTime(TI));
    sem_wait(mutex);
    *oxycount += 1;
    if (*hydrocount >= 2)
    {
        sem_post(hydroque);
        sem_post(hydroque);
        *hydrocount -= 2;
        sem_post(oxyque);
        *oxycount -= 1;
    }
    else
    {
        sem_post(mutex);
    }

    sem_wait(mutex_file);
	fprintf(f, "%d: O %d: going to queue\n", *A, id);
	*A += 1;
	sem_post(mutex_file);
    sem_wait(oxyque);
    if (NH < (*MOL_COUNT*2))
    {
        sem_wait(mutex_file);
        fprintf(f, "%d: O %d: not enough H\n", *A, id);
        *A += 1;
        sem_post(mutex_file);
        sem_post(oxyque);
        sem_post(hydroque);
    }
    else 
    {
        sem_wait(mutex_mol);
        *count += 1;
        if (*count == 3)
        {
            sem_wait(turnstile2);
            sem_post(turnstile);
        }

        sem_wait(mutex_file);
	    fprintf(f, "%d: O %d: creating molecule %d\n", *A, id, *MOL_COUNT);
	    *A += 1;
	    sem_post(mutex_file);
        
        sem_post(mutex_mol);

        sem_wait(turnstile);
        sem_post(turnstile);

        sem_wait(mutex_mol);
        *count -= 1;
        if (*count == 0)
        {
            MySleep(RndTime(TB));
            sem_wait(mutex_file);
            fprintf(f, "%d: O %d: molecule %d created\n", *A, id, *MOL_COUNT);
            *A += 1;
            sem_post(mutex_file);
            sem_wait(turnstile);
            *MOL_COUNT += 1;
            if((NO < *MOL_COUNT) || (NH < (*MOL_COUNT*2)))
            {
                sem_post(oxyque);
                sem_post(hydroque);
            }
            sem_post(turnstile2);
        }
        else
        {
            sem_wait(mutex_file);
	        fprintf(f, "%d: O %d: molecule %d created\n", *A, id, *MOL_COUNT);
	        *A += 1;
	        sem_post(mutex_file);
        }

        sem_post(mutex_mol);
        sem_wait(turnstile2);
        sem_post(turnstile2);
        sem_post(mutex);
    }
    return 0;

}

//Funkce pro proces "vodík"
int hydrogen(int id)
{
    sem_wait(mutex_file);
	fprintf(f, "%d: H %d: started\n", *A, id);
	*A += 1;
	sem_post(mutex_file);
    MySleep(RndTime(TI));
    sem_wait(mutex);
    *hydrocount += 1;
    if ((*hydrocount >= 2) && (*oxycount >= 1))
    {
        sem_post(hydroque);
        sem_post(hydroque);
        *hydrocount -= 2;
        sem_post(oxyque);
        *oxycount -= 1;
    }
    else
    {
        sem_post(mutex);
    }

    sem_wait(mutex_file);
	fprintf(f, "%d: H %d: going to queue\n", *A, id);
	*A += 1;
	sem_post(mutex_file);
    if(NH < 2)
    {
        sem_wait(mutex_file);
        fprintf(f, "%d: H %d: not enough O or H\n", *A, id);
        *A += 1;
        sem_post(mutex_file);
        sem_post(oxyque);
        return 0;
    }
    sem_wait(hydroque);
    if ((NO < *MOL_COUNT) || (NH < (*MOL_COUNT*2)))
    {
        sem_wait(mutex_file);
        fprintf(f, "%d: H %d: not enough O or H\n", *A, id);
        *A += 1;
        sem_post(mutex_file);
        sem_post(hydroque);
        sem_post(oxyque);
    } 
    else
    {
        sem_wait(mutex_mol);
        *count += 1;
        if (*count == 3)
        {
            sem_wait(turnstile2);
            sem_post(turnstile);
        }
        sem_wait(mutex_file);
	    fprintf(f, "%d: H %d: creating molecule %d\n", *A, id, *MOL_COUNT);
	    *A += 1;
	    sem_post(mutex_file);
        
        sem_post(mutex_mol);

        sem_wait(turnstile);
        sem_post(turnstile);

        sem_wait(mutex_mol);
        *count -= 1;
        if (*count == 0)
        {
            MySleep(RndTime(TB));
            sem_wait(mutex_file);
	        fprintf(f, "%d: H %d: molecule %d created\n", *A, id, *MOL_COUNT);
	        *A += 1;
	        sem_post(mutex_file);
            sem_wait(turnstile);
            *MOL_COUNT += 1;
            if((NO < *MOL_COUNT) || (NH < (*MOL_COUNT*2)))
            {
                sem_post(oxyque);
                sem_post(hydroque);
            }
            sem_post(turnstile2);
        }
        else
        {
            sem_wait(mutex_file);
	        fprintf(f, "%d: H %d: molecule %d created\n", *A, id, *MOL_COUNT);
	        *A += 1;
            sem_post(mutex_file);
        }

        sem_post(mutex_mol);
        sem_wait(turnstile2);
        sem_post(turnstile2);
    }
    return 0;
}

//Funkce pro kontrolu nevalidni hodnoty argumentu
int ArgValueCheck()
{
	if (NO <= 0)
	{
		fprintf(stderr, "Error: Incorrect value for argument NO! (expected NO > 0)\n");
		return 1;
	}

	if (NH <= 0)
	{
		fprintf(stderr, "Error: Incorrect value for argument NH! (expected NH > 0)\n");
		return 1;
	}

	if ((TI < 0) || (TI > 1000))
	{
		fprintf(stderr, "Error: Incorrect value for argument TI! (expected TI >= 0 && TI <= 1000)\n");
		return 1;
	}

	if ((TB < 0) || (TB > 1000))
	{
		fprintf(stderr, "Error: Incorrect value for argument TB! (expected TB >= 0 && TB <= 1000)\n");
		return 1;
	}

	return 0;
}

//Funkce pro kontrolu argumentu a jejich prirazeni do promennych
int ArgCheck(int argc, char const *argv[])
{
	char *errptr = NULL;
	if (argc != 5)
	{
		fprintf(stderr, "Error: Incorrect number of arguments!\n");
		return 1;
	}

	NO = strtol(argv[1], &errptr, 10);
	if (*errptr)
	{
		fprintf(stderr, "Error: Incorrect argument!\n");
		return 1;
	}

	NH = strtol(argv[2], &errptr, 10);
	if (*errptr)
	{
		fprintf(stderr, "Error: Incorrect argument!\n");
		return 1;
	}

    TI = strtol(argv[3], &errptr, 10);
	if (*errptr)
	{
		fprintf(stderr, "Error: Incorrect argument!\n");
		return 1;
	}

	TB = strtol(argv[4], &errptr, 10);
	if (*errptr)
	{
		fprintf(stderr, "Error: Incorrect argument!\n");
		return 1;
	}

	if (ArgValueCheck())
	{
		return 1;
	}

	return 0;
}

int main(int argc, char const *argv[])
{
    int error = 0;

    if ((f = fopen("proj2.out", "w")) == NULL)
    {
        fprintf(stderr, "Error: Error while opening file for output ");
        return 1;
    }

    setbuf(f, NULL);

    if (ArgCheck(argc, argv))
	{
        fclose(f);
		return 1;
	}

    init_semaphores();
    init_variables();

    for(int i = 1; i <= NO; i++)
    {
        pid_t process_id = fork();
        if (process_id == 0)
        {
            oxygen(i);
            fclose(f);
            exit(0);
        }
        else if (process_id == -1)
        {
            fprintf(stderr, "Error: Fork failed! ");
            error = 1;
        }
        
    }

    for(int i = 1; i <= NH; i++)
    {
        pid_t process_id = fork();
        if (process_id == 0)
        {
            hydrogen(i);
            fclose(f);
            exit(0);
        }
        else if (process_id == -1)
        {
            fprintf(stderr, "Error: Fork failed! ");
            error = 1;
        }
    }

    while(wait(NULL) > 0);
    
    fclose(f);
    clean_semaphores();
    clean_variables();
    if(error == 1)
    {
        return 1;
    }

    return 0;
}