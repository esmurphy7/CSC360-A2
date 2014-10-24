
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
void insert_train(train*);
void remove_train(train*);
void* train_sequence(void*);
train* get_nextTrain(char);
char* get_setAbv(NODE*);
// =========================

// ======= Constants ========
#define MAX_NTRAINS 100
// ==========================

// ====== Shared Data =======
train* trainSet[MAX_NTRAINS];
train* readyTrainSet[MAX_NTRAINS];
/* Ready train subsets 
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
// ==========================

// === Condition Variables & Mutexes ===
pthread_t mainThreads[3];
/* Restrict access to ready train sets */
pthread_cond_t readyTrain_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t readyTrainSet_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Restrict crossing access */
pthread_cond_t cross_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cross_mutex = PTHREAD_MUTEX_INITIALIZER;
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
	// initialization thread
	pthread_create(&mainThreads[0], NULL, init_trains, (void*)parameters);
	pthread_join(mainThreads[0], NULL);
	// dispatcher thread: prioritizes trains and handles crossing
	HE = list_create(NULL);
	HW = list_create(NULL);
	LE = list_create(NULL);
	LW = list_create(NULL);
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
	pthread_create(&trainStruct->thread, NULL, train_sequence, (void*)trainStruct);
	// store the train struct in the trainSet based on its tid
	trainSet[trainStruct->tid] = trainStruct;
}

void* train_sequence(void* ptr)
{
	// Begin the loading process
	train* trainStruct = (train*)ptr;
	printf("Train %d loading for %fsec...\n",trainStruct->tid, (trainStruct->loadTime)/10);
	unsigned int microsec = (trainStruct->loadTime)*100000;
	// load
	usleep(microsec);
	pthread_mutex_lock(&readyTrainSet_mutex);
	// put self into a train ready set
	insert_train(trainStruct);
	pthread_mutex_unlock(&readyTrainSet_mutex);
	// signal ready train condvar to tell main thread to re-prioritize
	pthread_cond_signal(&readyTrain_cond);
	printf("Train %d done loading\n",trainStruct->tid);
	
	// Begin crossing process
	pthread_mutex_lock(&cross_mutex);
	// wait on the train-specific cond var to be signalled by dispatcher thread
	printf("\nTrain %d: waiting to cross..\n",trainStruct->tid);
	pthread_cond_wait(&trainStruct->condVar, &cross_mutex);
	pthread_mutex_unlock(&cross_mutex);
	// request to cross
	pthread_mutex_lock(&cross_mutex);
	printf("Train %d crossing for %fsec...\n",trainStruct->tid, (trainStruct->crossTime)/10);
	microsec = (trainStruct->crossTime)*100000;
	// cross
	usleep(microsec);
	pthread_mutex_unlock(&cross_mutex);
	
	pthread_mutex_lock(&readyTrainSet_mutex);
	printf("Train %d: DONE CROSSING\n\n",trainStruct->tid);
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
	char* setAbv = get_setAbv(targetTrainSet);
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
	// print each element of target set
	NODE* current = targetTrainSet;
	//printf("Train %d: set %s after being inserted:\n",trainStruct->tid,setAbv);
	while(current)
	{
		if(current->data)
		{		
			train* trStr = (train*)current->data;
			//printf("%d\n",trStr->tid);	
		}
		current = current->next;
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
	char* setAbv = get_setAbv(targetTrainSet);
	if(targetTrainSet == NULL)
		perror("attempting to remove train from null set");
	else
		list_remove_data(targetTrainSet, (void*)trainStruct);
	// print each element of target set
	NODE* current = targetTrainSet;
	//printf("Train %d: set %s after being removed:\n",trainStruct->tid,setAbv);
	while(current)
	{
		if(current->data)
		{		
			train* trStr = (train*)current->data;
			//printf("%d\n",trStr->tid);	
		}
		current = current->next;
	}
}

void* prioritize_trains(void* p)
{
	params* parameters = (params*)p;
	int ntrains = (int)parameters->ntrains;
	train* nextTrain = NULL;
	preferred = 'E';
	for(;;)
	{
		// if there isn't a next train
		if(nextTrain == NULL)
		{
			// wait for one to finish loading
			pthread_mutex_lock(&readyTrainSet_mutex);
			printf("Dispatcher: waiting for first load...\n");
			pthread_cond_wait(&readyTrain_cond, &readyTrainSet_mutex);
			printf("Dispatcher: first load done..\n");
			nextTrain = get_nextTrain(preferred);
			pthread_mutex_unlock(&readyTrainSet_mutex);
		}
		// signal the train waiting on condVar to cross
		//printf("Dispatcher: signal train %d to cross\n",nextTrain->tid);
		pthread_cond_signal(&nextTrain->condVar);
		// wait for crossing threads to signal they're done
		//printf("Dispatcher: waiting for train %d to cross..\n",nextTrain->tid);
		//pthread_cond_wait(&cross_cond, &cross_mutex);
		// re-evaluate which train should be next
		pthread_mutex_lock(&readyTrainSet_mutex);
		nextTrain = get_nextTrain(preferred);
		pthread_mutex_unlock(&readyTrainSet_mutex);
	}
}

// Given the preferred direction, return the train that is next in line to cross
train* get_nextTrain(char preferred)
{
	//printf("Dispatcher: preferred direction: %c\n",preferred);
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
		perror("cannot get next train, all ready subsets are empty");
		exit(1);
	}
	char* setAbv = get_setAbv(targetSet);
	//printf("Dispatcher: finding next train in set %s...\n",setAbv);
	// find the set of trains within the target set that have the lowest load time
	//NODE* loadTrains = list_create(NULL);
	float bestLoadTime = 1000;
	int bestTid = 1000;
	train* bestTrain;
	NODE* currentTrain = targetSet;
	while(currentTrain)
	{
		if(currentTrain->data)
		{
			
			train* trainStruct = (train*)currentTrain->data;
			//printf("Dispatcher: current train: %d\n",trainStruct->tid);
			if(trainStruct->loadTime < bestLoadTime)
			{
				bestLoadTime = trainStruct->loadTime;
				//printf("Dispatcher: new best load time detected\n");
				bestTrain = trainStruct;
			}
			else if(trainStruct->loadTime == bestLoadTime)
			{
				//printf("Dispatcher: best load time tied %f\n",bestLoadTime);
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
	/*
	train* bestTrain;
	// if more than one train tied for best load time
	if(list_get_length(loadTrains) > 2)
	{
		printf("Dispatcher: more than one best load time\n");
		// find the best train in the most prioritized set
		int bestTid = 1000;
		currentTrain = loadTrains;
		while(currentTrain)
		{
			if(currentTrain->data)
			{
				train* trainStruct = (train*)currentTrain->data;
				//printf("Dispatcher: current tid %d\n",bestTid);
				if(trainStruct->tid < bestTid && trainStruct->tid != NULL)
				{
					//printf("Dispatcher: current tid %d\n",bestTid); 
					bestTid = trainStruct->tid;
				}
				//printf("Dispatcher: current tid %d\n",bestTid);
			}
			currentTrain = currentTrain->next;	
		}
		
		bestTrain = trainSet[bestTid];
	}
	else
	{
		bestTrain = (train*)loadTrains->data;
		//printf("Dispatcher: single best load time by train %d: %f\n",bestTrain->tid,bestTrain->loadTime);
		
	}
	//printf("Dispatcher: train %d crosses next\n", bestTrain->tid);
	return bestTrain;
	*/
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
