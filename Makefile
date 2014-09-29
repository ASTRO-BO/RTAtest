all: pwave_minimal pcompress pdecompress threads

pwave: pwave.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave.cpp -o pwave -lRTAtelem -lpacket -lrt

pwave_minimal: pwave_minimal.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave_minimal.cpp -o pwave_minimal -lpacket -lrt

pcompress: pcompress.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pcompress.cpp -o pcompress -lpacket -lrt

pdecompress: pdecompress.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pdecompress.cpp -o pdecompress -lpacket -lrt

threads: threads.c
	$(CC) $(CFLAGS) threads.c -o threads -lrt

clean:
	@rm -f pwave pwave_minimal pcompress pdecompress threads
