all: test_pthread

pwave: pwave.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave.cpp -o pwave -lRTAtelem -lpacket -lrt

pwave_minimal: pwave_minimal.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave_minimal.cpp -o pwave_minimal -lpacket -lrt

pwave_opencl: pwave_opencl.cpp
	$(CXX) -g -std=c++11 $(CXXFLAGS) pwave_opencl.cpp -o pwave_opencl -lpacket -lcfitsio -lCTAConfig -lCTAUtils -lOpenCL -lpthread

pcompress: pcompress.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pcompress.cpp -o pcompress -lpacket -lrt

pdecompress: pdecompress.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pdecompress.cpp -o pdecompress -lpacket -lrt

threads: threads.c
	$(CC) $(CFLAGS) threads.c -o threads -lrt

test_pthread: test_pthread.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) test_pthread.cpp -I$(CTARTA)/include -o test_pthread -L$(CTARTA)/lib -lz -lpacket -pthread

clean:
	@rm -f pwave pwave_minimal pwave_opencl pcompress pdecompress threads test_pthread
#	@rm -f CTAHeaderGEN.header rta_fadc_all.stream triggered_telescope1_30GEN.packet triggered_telescope1_40GEN.packet triggered_telescope1_50GEN.packet triggered_telescope_pixel1_30GEN.rblock triggered_telescope_pixel1_40GEN.rblock triggered_telescope_pixel1_50GEN.rblock triggered_telescope_pixelIDGEN.rblock
