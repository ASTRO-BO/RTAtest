all: pthreads mt

SYSTEM= $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYSTEM)))
	PTHREAD_LIBS = -lrt
	OCL_LIBS = -lOpenCL
endif
ifneq (, $(findstring darwin, $(SYSTEM)))
	OCL_LIBS = -framework OpenCL
endif

pthreads: pthreads.c
	$(CC) $(CFLAGS) pthreads.c -o pthreads -pthread $(PTHREAD_LIBS)

pwave_cl: pwave_cl.cpp
	$(CXX) -g -std=c++11 -I. $(CXXFLAGS) pwave_cl.cpp -o pwave_cl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -pthread $(OCL_LIBS)

mt: mt.cpp
	$(CXX) $(CXXFLAGS) mt.cpp -o mt -lz -lpacket -pthread -lCTAUtils $(PTHREAD_LIBS)

pwave_cl_altera: pwave_cl.cpp
	$(CXX) -DCL_ALTERA -g -std=c++11 -I. $(CXXFLAGS) `aocl compile-config` pwave_cl.cpp -o pwave_cl_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(OCL_LIBS) `aocl link-config`

clean:
	@rm -rf pthreads mt pwave_cl pwave_cl.dSYM
