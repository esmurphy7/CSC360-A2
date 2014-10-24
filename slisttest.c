#include "slist.h"


int main()
{
	NODE *list = list_create(NULL);
	NODE *second, *inserted;
	NODE *match;
	if(list->data == NULL)
		printf("list empty\n");
	//printf("data %s\n",(char*)(list->data));
	NODE* templist = list;
	if(templist->data == NULL)
		printf("templist empty\n");
	/* Create initial elements of list */
	list=list_create((void*)"First");
	second=list_insert_after(list, (void*)"Second");
	list_insert_after(second, (void*)"Third");

	printf("Initial list:\n");
	list_foreach(list, printstring);
	putchar('\n');

	/* Insert 1 extra element in front */
	list=list_insert_beginning(list, "BeforeFirst");
	printf("After list_insert_beginning():\n");
	list_foreach(list, printstring);
	putchar('\n');

	/* Insert 1 extra element after second */
	inserted=list_insert_after(second, "AfterSecond");
	printf("After list_insert_after():\n");
	list_foreach(list, printstring);
	putchar('\n');

	/* Remove the element */
	list_remove(list, inserted);
	printf("After list_remove():\n");
	list_foreach(list, printstring);
	putchar('\n');

	/* Remove the element with data */
	list_remove_data(list, "First");
	printf("After list_remove_data():\n");
	list_foreach(list, printstring);
	putchar('\n');
	
	/* Search */
	if((match=list_find(list, findstring, "Third")))
		printf("Found \"Third\"\n");
	else printf("Did not find \"Third\"\n");

	free(list);
	
	return 0;
}
