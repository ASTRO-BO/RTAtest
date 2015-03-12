all: pthreads mt pwave_cl

SYSTEM= $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYSTEM)))
	LIBS = -lrt
endif

pthreads: pthreads.c
	$(CC) $(CFLAGS) pthreads.c -o pthreads -pthread $(LIBS)

pwave_cl: pwave_cl.cpp
	$(CXX) -g -std=c++11 $(CXXFLAGS) pwave_cl.cpp -o pwave_cl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lOpenCL -lpthread $(LIBS)

mt: mt.cpp
	$(CXX) $(CXXFLAGS) mt.cpp -I$(CTARTA)/include -o mt -L$(CTARTA)/lib -lz -lpacket -pthread -lCTAUtils $(LIBS)

clean:
	@rm -f pthreads mt pwave_cl
