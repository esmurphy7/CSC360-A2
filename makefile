PROJECT = mts
OBJECTS = $(PROJECT).o
GCC = gcc
CFLAGS = -Wall
LIBS = -lpthread

$(PROJECT).exe: $(OBJECTS)
	$(GCC) $(CFLAGS) $(OBJECTS) -o $(PROJECT).exe $(LIBS)

$(PROJECT).o: $(PROJECT).c
	$(GCC) $(CFLAGS) -c $(PROJECT).c
	
clean:
	rm *.o $(PROJECT).exe