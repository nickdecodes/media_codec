CXX = g++
CXXFLAGS = -std=c++11 -Wall -g
INCLUDE = 
LIBS = -lavformat -lavcodec -lavutil -lswscale -lSDL2 -lavfilter
OBJS = ffavcpp.o ffavfilter.o ffavhw.o ffavopt.o ffcmdutil.o 

.PHONY: clean

ffavcpp : $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBS) $(INCLUDE) -o ffavcpp

ffavfilter.o : ffavfilter.cpp ffavfilter.h
	$(CXX) -c $(CXXFLAGS) ffavfilter.cpp

ffavhw.o : ffavhw.cpp ffavhw.h 
	$(CXX) -c $(CXXFLAGS) ffavhw.cpp 

ffavopt.o : ffavopt.cpp ffavopt.h 
	$(CXX) -c $(CXXFLAGS) ffavopt.cpp

ffcmdutil.o : ffcmdutil.cpp ffcmdutil.h
	$(CXX) -c $(CXXFLAGS) ffcmdutil.cpp

clean:
	rm -rf *.o ffavcpp