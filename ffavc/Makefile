CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -g
CXXFLAGS = -std=c++11 -Wall -g
THIRDPATH = ./lib
INCLUDE = 
# LIBS = -lavformat -lavcodec -lavutil -lswscale -lSDL2 -lavfilter
LIBS = $(THIRDPATH)/libavcodec.a $(THIRDPATH)/libavdevice.a
LIBS += $(THIRDPATH)/libavfilter.a $(THIRDPATH)/libavformat.a
LIBS += $(THIRDPATH)/libavutil.a $(THIRDPATH)/libswresample.a
LIBS += $(THIRDPATH)/libswscale.a
OBJS = ffavcpp.o ffavfilter.o ffavhw.o ffavopt.o ffcmdutil.o 

.PHONY: clean

ffavapi : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) $(INCLUDE) -o ffavapi

ffavfilter.o : ffavfilter.cpp ffavfilter.h
	$(CC) -c $(CFLAGS) ffavfilter.cpp

ffavhw.o : ffavhw.cpp ffavhw.h 
	$(CC) -c $(CFLAGS) ffavhw.cpp 

ffavopt.o : ffavopt.cpp ffavopt.h 
	$(CC) -c $(CFLAGS) ffavopt.cpp

ffcmdutil.o : ffcmdutil.cpp ffcmdutil.h
	$(CC) -c $(CFLAGS) ffcmdutil.cpp

clean:
	rm -rf *.o ffavapi