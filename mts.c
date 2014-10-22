
// TODO: Create and attach pthread to each train struct

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void init_trains(char*,int);
void create_trainStruct(char*[], int);

typedef struct
{
	int tid;
	char dir;
	int loadTime;
	int crossTime;
	pthread_t thread;
} train;
// ======== Globals ========
#define MAX_NTRAINS 20
train* trainSet[MAX_NTRAINS];
// =========================
void main(int argc, char* argv[])
{
	char* filename = argv[1];
	int ntrains = atoi(argv[2]);
	if(ntrains <= 0)
	{
		perror("specify at least one train");
		exit(1);
	}
	init_trains(filename, ntrains);
}


void init_trains(char* filename, int ntrains)
{
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
			// tokenize line with delims : and ,
			tok = strtok(line, delims);
			while(tok != NULL)
			{
				trainData[i] = tok;
				tok = strtok(NULL, delims);
				i++;
			}
			// store each element of trainData in a new train struct
			create_trainStruct(trainData, currentLine);
			currentLine++;
		}
	}
	fclose(file);
	if(line)
		free(line);
}

void create_trainStruct(char* trainData[4], int trainId)
{
	train* trainStruct = (train*)malloc(sizeof(train));
	trainStruct->tid = trainId;
	trainStruct->dir = *trainData[0];
	trainStruct->loadTime = atoi(trainData[1]);
	trainStruct->crossTime = atoi(trainData[2]);
	// pthread_create(&trainStruct->thread
	trainSet[trainStruct->tid] = trainStruct;
}

