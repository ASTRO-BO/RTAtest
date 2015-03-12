all: threads test_pthread pwave_opencl

SYSTEM= $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYSTEM)))
	LIBS = -lrt
endif

pwave_opencl: pwave_opencl.cpp
	$(CXX) -g -std=c++11 $(CXXFLAGS) pwave_opencl.cpp -o pwave_opencl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lOpenCL -lpthread $(LIBS)

threads: threads.c
	$(CC) $(CFLAGS) threads.c -o threads -pthread $(LIBS)

test_pthread: test_pthread.cpp
	$(CXX) $(CXXFLAGS) test_pthread.cpp -I$(CTARTA)/include -o test_pthread -L$(CTARTA)/lib -lz -lpacket -pthread -lCTAUtils $(LIBS)

clean:
	@rm -f pwave_opencl threads test_pthread
