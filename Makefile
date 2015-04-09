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

clean:
	@rm -f pthreads mt pwave_cl
