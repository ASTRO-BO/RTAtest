all: pthreads mt pwave_serial pwave_omp pwave_cl pwave_cl_reduction

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

mt: mt.cpp
	$(CXX) $(CXXFLAGS) mt.cpp -o mt -lz -lpacket -pthread -lCTAUtils $(PTHREAD_LIBS)

pwave_serial: pwave_serial.cpp
	$(CXX) -O3 -g -std=c++11 $(CXXFLAGS) pwave_serial.cpp -o pwave_serial -lpacket -lcfitsio -lCTAConfig -lCTAUtils

pwave_omp: pwave_serial.cpp
	$(CXX) -O3 -g -std=c++11 -fopenmp -DOMP $(CXXFLAGS) pwave_serial.cpp -o pwave_omp -lpacket -lcfitsio -lCTAConfig -lCTAUtils

pwave_cl: pwave_cl.cpp
	$(CXX) -O3 -g -std=c++11 -I. $(CXXFLAGS) pwave_cl.cpp -o pwave_cl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -pthread $(OCL_LIBS)

pwave_cl_reduction: pwave_cl_reduction.cpp
	$(CXX) -O3 -g -std=c++11 -I. $(CXXFLAGS) pwave_cl_reduction.cpp -o pwave_cl_reduction -lpacket -lcfitsio -lCTAConfig -lCTAUtils -pthread $(OCL_LIBS)

pwave_cl_altera: pwave_cl.cpp
	$(CXX) -DCL_ALTERA -g -std=c++11 -I. $(CXXFLAGS) `aocl compile-config` pwave_cl.cpp -o pwave_cl_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(OCL_LIBS) `aocl link-config`

clean:
	@rm -rf pthreads mt pwave_serial pwave_omp pwave_cl pwave_cl.dSYM pwave_cl_reduction pwave_cl_altera
