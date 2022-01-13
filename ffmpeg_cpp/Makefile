CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -g
CXXFLAGS = -std=c++11 -Wall -g
THIRDPATH = ./lib
INCLUDE = -I Logger.h
# LIBS = -lavformat -lavcodec -lavutil -lswscale -lSDL2 -lavfilter
LIBS = $(THIRDPATH)/libavcodec.a $(THIRDPATH)/libavdevice.a
LIBS += $(THIRDPATH)/libavfilter.a $(THIRDPATH)/libavformat.a
LIBS += $(THIRDPATH)/libavutil.a $(THIRDPATH)/libswresample.a
LIBS += $(THIRDPATH)/libswscale.a
OBJS = FFmpegApi.o FFmpegFilter.o ffavhw.o FFmpegOpt.o FFmpegUtil.o 

.PHONY: clean

FFmpegApi : $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBS) $(INCLUDE) -o FFmpegApi

FFmpegFilter.o : FFmpegFilter.cpp FFmpegFilter.h
	$(CXX) -c $(CXXFLAGS) FFmpegFilter.cpp

ffavhw.o : ffavhw.cpp ffavhw.h 
	$(CXX) -c $(CXXFLAGS) ffavhw.cpp 

FFmpegOpt.o : FFmpegOpt.cpp FFmpegOpt.h 
	$(CXX) -c $(CXXFLAGS) FFmpegOpt.cpp

FFmpegUtil.o : FFmpegUtil.cpp FFmpegUtil.h
	$(CXX) -c $(CXXFLAGS) FFmpegUtil.cpp

clean:
	rm -rf *.o *.log FFmpegApi