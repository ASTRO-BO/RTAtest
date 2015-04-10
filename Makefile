all: pthreads mt

SYSTEM= $(shell gcc -dumpmachine)
ifneq (, $(findstring linux, $(SYSTEM)))
	LIBS = -lrt -lOpenCL
endif
ifneq (, $(findstring darwin, $(SYSTEM)))
	LIBS = -framework OpenCL
endif

pthreads: pthreads.c
	$(CC) $(CFLAGS) pthreads.c -o pthreads -pthread $(LIBS)

pwave_cl: pwave_cl.cpp
	$(CXX) -g -std=c++11 $(CXXFLAGS) pwave_cl.cpp -o pwave_cl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -pthread $(LIBS)

mt: mt.cpp
	$(CXX) $(CXXFLAGS) mt.cpp -o mt -lz -lpacket -pthread -lCTAUtils $(LIBS)

pwave_cl_altera: pwave_cl.cpp
	$(CXX) -DCL_ALTERA -g -std=c++11 $(CXXFLAGS) `aocl compile-config` pwave_cl.cpp -o pwave_cl_altera -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lpthread $(LIBS) `aocl link-config`

clean:
	@rm -r pthreads mt pwave_cl pwave_cl.dSYM
