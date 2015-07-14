all: pthreads mt pwave_serial pwave_omp cl altera

CFLAGS=-O2 -g
CXXFLAGS=-O2 -g

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
	$(CXX) $(CXXFLAGS) -std=c++11 pwave_serial.cpp -o pwave_serial -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lstdc++

pwave_omp: pwave_serial.cpp
	$(CXX) $(CXXFLAGS) -std=c++11 -fopenmp -DOMP pwave_serial.cpp -o pwave_omp -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lstdc++

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
	@rm -rf pthreads mt pwave_serial pwave_omp pwave_cl pwave_cl2 pwave_cl3 pwave_cl4 pwave_cl_altera pwave_cl2_altera pwave_cl3_altera
