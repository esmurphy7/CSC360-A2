
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include "slist.h"

// ======= Prototypes ======
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

void* init_trains(void*);
void init_trainStruct(char*[], int);
void* dispatch_trains(void*);
void insert_train(train*);
void remove_train(train*);
void* train_sequence(void*);
train* get_nextTrain(char);
char* get_setAbv(NODE*);
char* get_dirString(train*);
// =========================


// ======= Constants ========
#define MAX_NTRAINS 100
// ==========================


// ====== Shared Data =======
train* trainSet[MAX_NTRAINS];
/* Ordered sets of trains ready to cross
	H: high priority
	L: low priority
	E: east
	W: west*/
NODE* HE;
NODE* LE;
NODE* HW;
NODE* LW;
/* Preferred crossing direction */
char preferred;
/* Trains left to cross */
int trainsLeft;
// ==========================


// === Condition Variables & Mutexes ===
pthread_t mainThreads[3];
/* Restrict access to the sets of trains ready to cross */
pthread_cond_t readyTrain_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t readyTrainSet_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Restrict crossing access */
pthread_cond_t cross_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cross_mutex = PTHREAD_MUTEX_INITIALIZER;
// =====================================



int main(int argc, char* argv[])
{
	if(!argv[2] || argc < 3)
	{
		printf("specify an input file and/or at least one train\n");
		exit(1);
	}
	char* filename = argv[1];
	int ntrains = atoi(argv[2]);
	trainsLeft = ntrains;
	params* parameters;
	parameters->filename = filename;
	parameters->ntrains = ntrains;
	// initialization thread: reads file, creates train structs, and creates train threads
	pthread_create(&mainThreads[0], NULL, init_trains, (void*)parameters);
	pthread_join(mainThreads[0], NULL);
	// initialize ready subsets
	HE = list_create(NULL);
	HW = list_create(NULL);
	LE = list_create(NULL);
	LW = list_create(NULL);
	// dispatcher thread: prioritizes trains and synchronizes crossing
	pthread_create(&mainThreads[1], NULL, dispatch_trains, (void*)parameters);
	// wait for dispatcher thread to exit
	void* status;
	pthread_join(mainThreads[1], &status);
	
	free(HE);
	free(HW);
	free(LE);
	free(LW);
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
	// return for compiler
	return NULL;
}

void init_trainStruct(char* trainData[4], int trainId)
{
	train* trainStruct = (train*)malloc(sizeof(train));
	trainStruct->tid = trainId;
	trainStruct->dir = *trainData[0];
	trainStruct->loadTime = atof(trainData[1]);
	trainStruct->crossTime = atof(trainData[2]);
	pthread_create(&trainStruct->thread, NULL, train_sequence, (void*)trainStruct);
	// store the train struct in the trainSet based on its tid
	trainSet[trainStruct->tid] = trainStruct;
}

void* train_sequence(void* ptr)
{
	// Begin the loading process
	train* trainStruct = (train*)ptr;
	//printf("Train %d loading for %fsec...\n",trainStruct->tid, (trainStruct->loadTime)/10);
	unsigned int microsec = (trainStruct->loadTime)*100000;
	// load
	usleep(microsec);
	pthread_mutex_lock(&readyTrainSet_mutex);
	// put self into a train ready set
	insert_train(trainStruct);
	pthread_mutex_unlock(&readyTrainSet_mutex);
	// signal ready train condvar to tell main thread to re-prioritize
	pthread_cond_signal(&readyTrain_cond);
	printf("Train %d is ready to go %s\n",trainStruct->tid, get_dirString(trainStruct));
	
	// Begin crossing process
	pthread_mutex_lock(&cross_mutex);
	// wait on the train-specific cond var to be signalled by dispatcher thread
	//printf("\nTrain %d: waiting to cross..\n",trainStruct->tid);
	pthread_cond_wait(&trainStruct->condVar, &cross_mutex);
	pthread_mutex_unlock(&cross_mutex);
	// request to cross
	pthread_mutex_lock(&cross_mutex);
	printf("Train %d is ON the main track going %s\n",trainStruct->tid, get_dirString(trainStruct));
	microsec = (trainStruct->crossTime)*100000;
	// cross
	usleep(microsec);
	pthread_mutex_unlock(&cross_mutex);
	
	pthread_mutex_lock(&readyTrainSet_mutex);
	printf("Train %d is OFF the main track going %s\n",trainStruct->tid, get_dirString(trainStruct));
	// decrement the number of trains that still need to cross
	trainsLeft--;
	// switch preferred direction
	if(trainStruct->dir == 'E' || trainStruct->dir == 'e')
		preferred = 'W';
	else
		preferred = 'E';
	// remove self from ready train subset
	remove_train(trainStruct);
	// signal done crossing
	//pthread_cond_signal(&cross_cond);
	pthread_mutex_unlock(&readyTrainSet_mutex);
	// return for compiler
	return NULL;
}

// Insert the train into the correct ready subset.
void insert_train(train* trainStruct)
{
	//printf("Train %d: inserting self...\n",trainStruct->tid);
	NODE* targetTrainSet;
	char dir = trainStruct->dir;
	if(dir == 'E')
	{
		targetTrainSet = HE;
	}
	else if(dir == 'e')
	{
		targetTrainSet = LE;
	}
	else if(dir == 'W')
	{
		targetTrainSet = HW;
	}
	else if(dir == 'w')
	{
		targetTrainSet = LW;
	}
	// Initialize the subset if need be
	if(targetTrainSet == NULL)
	{
		targetTrainSet = list_create((void*)trainStruct);
		//printf("Train %d: inited set and inserted self\n",trainStruct->tid);
	}
	else
	{
		targetTrainSet = list_insert_after(targetTrainSet, (void*)trainStruct);
		//printf("Train %d: inserted self\n",trainStruct->tid);
	}
}

// Remove the train from the correct ready subset.
void remove_train(train* trainStruct)
{
	NODE* targetTrainSet;
	char dir = trainStruct->dir;
	if(dir == 'E')
	{
		targetTrainSet = HE;
	}
	else if(dir == 'e')
	{
		targetTrainSet = LE;
	}
	else if(dir == 'W')
	{
		targetTrainSet = HW;
	}
	else if(dir == 'w')
	{
		targetTrainSet = LW;
	}
	if(targetTrainSet == NULL)
		perror("attempting to remove train from null set");
	else
		list_remove_data(targetTrainSet, (void*)trainStruct);
}

void* dispatch_trains(void* p)
{
	train* nextTrain = NULL;
	preferred = 'E';
	for(;;)
	{
		if(trainsLeft <= 0)
		{
			void* status = (void*)malloc(sizeof(void*)*100);
			printf("Dispatcher: all trains done crossing\n");
			pthread_exit((void*)status);
		}
		// if there isn't a next train
		if(nextTrain == NULL)
		{
			// wait for one to finish loading
			pthread_mutex_lock(&readyTrainSet_mutex);
			//printf("Dispatcher: waiting for first load...\n");
			pthread_cond_wait(&readyTrain_cond, &readyTrainSet_mutex);
			//printf("Dispatcher: first load done..\n");
			nextTrain = get_nextTrain(preferred);
			pthread_mutex_unlock(&readyTrainSet_mutex);
		}
		// signal the next train to cross
		pthread_cond_signal(&nextTrain->condVar);
		// re-evaluate which train should be next
		pthread_mutex_lock(&readyTrainSet_mutex);
		nextTrain = get_nextTrain(preferred);
		pthread_mutex_unlock(&readyTrainSet_mutex);
	}
	// return for compiler
	return NULL;
}

// Given the preferred direction, return the train that is next in line to cross
train* get_nextTrain(char preferred)
{
	// ready subsets of descending priority
	NODE* firstSet;
	NODE* secondSet;
	NODE* thirdSet;
	NODE* fourthSet;
	NODE* targetSet;
	// prioritize each ready subset based on the preferred direction
	if(preferred == 'E')
	{
		firstSet = HE;
		secondSet = HW;
		thirdSet = LE;
		fourthSet = LW;
	}
	else
	{
		firstSet = HW;
		secondSet = HE;
		thirdSet = LW;
		fourthSet = LE;
	}
	// point to next most prioritized set if ideal set is empty
	if(!list_isEmpty(firstSet))
		targetSet = firstSet;
	else if(!list_isEmpty(secondSet))
		targetSet = secondSet;
	else if(!list_isEmpty(thirdSet))
		targetSet = thirdSet;
	else if(!list_isEmpty(fourthSet))
		targetSet = fourthSet;
	else
	{
		//perror("cannot get next train, all ready subsets are empty");
		return NULL;
	}
	// find the train in the target set with the best load time
	float bestLoadTime = 1000;
	int bestTid = 1000;
	train* bestTrain;
	NODE* currentTrain = targetSet;
	while(currentTrain)
	{
		if(currentTrain->data)
		{
			
			train* trainStruct = (train*)currentTrain->data;
			// found train with better load time
			if(trainStruct->loadTime < bestLoadTime)
			{
				bestLoadTime = trainStruct->loadTime;
				bestTrain = trainStruct;
			}
			// found train with same load time as the best so far
			else if(trainStruct->loadTime == bestLoadTime)
			{
				// found train with tid of higher priority
				if(trainStruct->tid < bestTid)
				{
					bestTid = trainStruct->tid;
					bestTrain = trainStruct;
				}
			}
		}
		currentTrain = currentTrain->next;
	}
	return bestTrain;
}

char* get_dirString(train* trainStruct)
{
	char dir = trainStruct->dir;
	char* dirString;
	if(dir == 'E' || dir == 'e')
		dirString = "East";
	else if(dir == 'W' || dir == 'w')
		dirString = "West";
	else
		printf("cannot get direction string\n");
	return dirString;
}

char* get_setAbv(NODE* targetSet)
{
	char* setAbv = "could not compare set";
	if(targetSet == HE)
		setAbv = "HE";
	else if(targetSet == HW)
		setAbv = "HW";
	else if(targetSet == LE)
		setAbv = "LE";
	else if(targetSet == LW)
		setAbv = "LW";
	return setAbv;
}
