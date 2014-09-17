all: pwave pwave_minimal

pwave: pwave.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave.cpp -o pwave -lRTAtelem -lpacket -lrt

pwave_minimal: pwave_minimal.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave_minimal.cpp -o pwave_minimal -lpacket -lrt

clean:
	@rm -f pwave pwave_minimal
