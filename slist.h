/* The authors of this work have released all rights to it and placed it
in the public domain under the Creative Commons CC0 1.0 waiver
(http://creativecommons.org/publicdomain/zero/1.0/).

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Retrieved from: http://en.literateprograms.org/Singly_linked_list_(C)?oldid=19215
*/

#include<stdlib.h>
#include<stdio.h>
#include<string.h>

typedef struct node_s {
	void *data;
	struct node_s *next;	
} NODE;

NODE *list_create(void *data)
{
	NODE *node;
	if(!(node=malloc(sizeof(NODE)))) return NULL;
	node->data=data;
	node->next=NULL;
	return node;
}
NODE *list_insert_after(NODE *node, void *data)
{
	NODE *newnode;
        newnode=list_create(data);
        newnode->next = node->next;
        node->next = newnode;
	return newnode;
}
NODE *list_insert_beginning(NODE *list, void *data)
{
	NODE *newnode;
        newnode=list_create(data);
        newnode->next = list;
	return newnode;
}
int list_remove(NODE *list, NODE *node)
{
	while(list->next && list->next!=node) list=list->next;
	if(list->next) {
		list->next=node->next;
		free(node);
		return 0;		
	} else return -1;
}
int list_foreach(NODE *node, int(*func)(void*))
{
	while(node) {
		if(func(node->data)!=0) return -1;
		node=node->next;
	}
	return 0;
}
NODE *list_find(NODE *node, int(*func)(void*,void*), void *data)
{
	while(node) {
		if(func(node->data, data)>0) return node;
		node=node->next;
	}
	return NULL;
}
int printstring(void *s)
{
	printf("%s\n", (char *)s);
	return 0;
}
int findstring(void *listdata, void *searchdata)
{
	return strcmp((char*)listdata, (char*)searchdata)?0:1;
}

