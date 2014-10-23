
// TODO:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include "slist.h"

typedef struct
{
	int tid;
	char dir;
	float loadTime;
	float crossTime;
	pthread_t thread;
	pthread_cond_t condVar;
} train;

typedef struct
{
	char* filename;
	int ntrains;
} params;

// ======= Prototypes ======
void* init_trains(void*);
void init_trainStruct(char*[], int);
void* prioritize_trains(void*);
void* load(void*);
// =========================

// ======= Constants ========
#define MAX_NTRAINS 100
// ==========================

// ======== Globals =========
train* trainSet[MAX_NTRAINS];
train* readyTrainSet[MAX_NTRAINS];
// ==========================

// === Condition Variables & Mutexes ===
pthread_t mainThreads[3];
pthread_cond_t readyTrain_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t readyTrainSet_mutex = PTHREAD_MUTEX_INITIALIZER;
// =====================================

int main(int argc, char* argv[])
{
	char* filename = argv[1];
	int ntrains = atoi(argv[2]);
	if(ntrains <= 0)
	{
		perror("specify at least one train");
		exit(1);
	}
	params* parameters;
	parameters->filename = filename;
	parameters->ntrains = ntrains;
	pthread_create(&mainThreads[0], NULL, init_trains, (void*)parameters);
	pthread_join(mainThreads[0], NULL);
	pthread_create(&mainThreads[1], NULL, prioritize_trains, (void*)parameters);
	pthread_join(mainThreads[1], NULL);
	return 0;
}

void* init_trains(void* p)
{
	params* parameters = (params*)p;
	char* filename = (char*)parameters->filename;
	int ntrains = (int)parameters->ntrains;
	FILE* file = fopen(filename, "r");
	ssize_t l_length;
	size_t len = 0;
	char* line = NULL;
	if(!file)
	{
		perror("fopen");
		exit(1);
	}
	else
	{
		char* tok;
		char* delims = ":,\n";
		int currentLine = 0;
		
		while((l_length = getline(&line, &len, file)) != -1)
		{
			char* trainData[4];
			int i = 0;
			if(currentLine == ntrains)
				break;
			// tokenize line with delims
			tok = strtok(line, delims);
			while(tok != NULL)
			{
				trainData[i] = tok;
				tok = strtok(NULL, delims);
				i++;
			}
			// store each element of trainData in a new train struct
			init_trainStruct(trainData, currentLine);
			currentLine++;
		}
	}
	fclose(file);
	if(line)
		free(line);
}

void init_trainStruct(char* trainData[4], int trainId)
{
	train* trainStruct = (train*)malloc(sizeof(train));
	trainStruct->tid = trainId;
	trainStruct->dir = *trainData[0];
	trainStruct->loadTime = atof(trainData[1]);
	trainStruct->crossTime = atof(trainData[2]);
	pthread_create(&trainStruct->thread, NULL, load, (void*)trainStruct);
	// store the train struct in the trainSet based on its tid
	trainSet[trainStruct->tid] = trainStruct;
}

void* prioritize_trains(void* p)
{
	printf("prioritizing...\n");
	params* parameters = (params*)p;
	int ntrains = (int)parameters->ntrains;
	for(;;)
	{
		pthread_mutex_lock(&readyTrainSet_mutex);
		// wait for a train to signal that it's ready
		pthread_cond_wait(&readyTrain_cond, &readyTrainSet_mutex);
		// print all ready trains
		int i;
		for(i=0; i<ntrains; i++)
		{
			if(readyTrainSet[i])
				printf("Train %d is ready\n",i);
		}
		printf("\n");
		//train* nextTrain = get_nextTrain();
		pthread_mutex_unlock(&readyTrainSet_mutex);
	}
}

void* load(void* ptr)
{
	train* trainStruct = (train*)ptr;
	printf("Train %d loading for %fsec...\n",trainStruct->tid, (trainStruct->loadTime)/10);
	unsigned int microsec = (trainStruct->loadTime)*100000;
	usleep(microsec);
	printf("Train %d done loading\n",trainStruct->tid);
	// lock the ready train set mutex
	pthread_mutex_lock(&readyTrainSet_mutex);
	// put self into train ready set
	readyTrainSet[trainStruct->tid] = trainStruct;
	// signal ready train condvar? - to tell main thread to re-prioritize?
	pthread_cond_signal(&readyTrain_cond);
	// unlock mutex
	pthread_mutex_unlock(&readyTrainSet_mutex);
}
