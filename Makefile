all: pwave pwave_minimale pwave_opencl pcompress pdecompress threads test_pthread

pwave: pwave.cpp
	$(CXX) $(CXXFLAGS) -fopenmp pwave.cpp -o pwave -lRTAtelem -lpacket -lrt -lCTAUtils

pwave_minimal: pwave_minimal.cpp
	$(CXX) $(CXXFLAGS) -fopenmp pwave_minimal.cpp -o pwave_minimal -lpacket -lrt -lCTAUtils

pwave_opencl: pwave_opencl.cpp
	$(CXX) -g -std=c++11 $(CXXFLAGS) pwave_opencl.cpp -o pwave_opencl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lOpenCL -lpthread

pcompress: pcompress.cpp
	$(CXX) $(CXXFLAGS) -fopenmp pcompress.cpp -o pcompress -lpacket -lrt -lCTAUtils

pdecompress: pdecompress.cpp
	$(CXX) $(CXXFLAGS) -fopenmp pdecompress.cpp -o pdecompress -lpacket -lrt -lCTAUtils

threads: threads.c
	$(CC) $(CFLAGS) threads.c -o threads -lrt

test_pthread: test_pthread.cpp
	$(CXX) $(CXXFLAGS) test_pthread.cpp -I$(CTARTA)/include -o test_pthread -L$(CTARTA)/lib -lz -lpacket -pthread -lCTAUtils

clean:
	@rm -f pwave pwave_minimal pwave_opencl pcompress pdecompress threads test_pthread
