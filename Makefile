all: pwave_serial pwave_omp

CFLAGS=-O2 -g -Wall -Wno-unknown-pragmas
CXXFLAGS=-O2 -g -Wall -Wno-unknown-pragmas

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

test_cache: test_cache.cpp
	$(CXX) -O3 -g -std=c++11 $(CXXFLAGS) test_cache.cpp -o test_cache

pwave_serial: pwave_serial.cpp
	$(CXX) $(CXXFLAGS) -std=c++11 pwave_serial.cpp -o pwave_serial -lpacket -lCTAConfig -lCTAUtils -lcfitsio -lrt

pwave_omp: pwave_serial.cpp
	$(CXX) $(CXXFLAGS) -std=c++11 -fopenmp -DOMP pwave_serial.cpp -o pwave_omp -lpacket -lCTAConfig -lCTAUtils -lcfitsio -lrt

cl: pwave_cl pwave_cl2 pwave_cl3 pwave_cl4

pwave_cl: pwave_cl.cpp CLUtility.h
	$(CXX) $(CXXFLAGS) -std=c++11 -I. pwave_cl.cpp -o pwave_cl -lpacket -lcfitsio -lCTAConfig -lCTAUtils $(OCL_LIBS)

pwave_cl2: pwave_cl2.cpp CLUtility.h
	$(CXX) $(CXXFLAGS) -std=c++11 -I. pwave_cl2.cpp -o pwave_cl2 -lpacket -lcfitsio -lCTAConfig -lCTAUtils $(OCL_LIBS)

pwave_cl3: pwave_cl3.cpp CLUtility.h
	$(CXX) $(CXXFLAGS) -std=c++11 -I. pwave_cl3.cpp -o pwave_cl3 -lpacket -lcfitsio -lCTAConfig -lCTAUtils $(OCL_LIBS)

pwave_cl4: pwave_cl4.cpp CLUtility.h
	$(CXX) $(CXXFLAGS) -std=c++11 -I. pwave_cl4.cpp -o pwave_cl4 -lpacket -lcfitsio -lCTAConfig -lCTAUtils $(OCL_LIBS)

altera: pwave_cl_altera pwave_cl2_altera pwave_cl3_altera

pwave_cl_altera: pwave_cl.cpp
	$(CXX) $(CXXFLAGS) -DCL_ALTERA -std=c++11 -I. `aocl compile-config` pwave_cl.cpp -o pwave_cl_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(OCL_LIBS) `aocl link-config`

pwave_cl2_altera: pwave_cl2.cpp
	$(CXX) $(CXXFLAGS) -DCL_ALTERA -std=c++11 -I. `aocl compile-config` pwave_cl2.cpp -o pwave_cl2_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(OCL_LIBS) `aocl link-config`

pwave_cl3_altera: pwave_cl3.cpp
	$(CXX) $(CXXFLAGS) -DCL_ALTERA -std=c++11 -I. `aocl compile-config` pwave_cl3.cpp -o pwave_cl3_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(OCL_LIBS) `aocl link-config`

clean:
	@rm -rf pthreads mt pwave_serial pwave_omp pwave_cl pwave_cl2 pwave_cl3 pwave_cl4 pwave_cl_altera pwave_cl2_altera pwave_cl3_altera test_cache
