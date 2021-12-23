CC = gcc
CFLAGS = -std=c99 -Wall -g
INCLUDE = 
LIBS = -lavformat -lavcodec
OBJS = main.o

.PHONY: clean

a.out : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) $(INCLUDE) -o test
    
clean:
	rm -rf *.o test
