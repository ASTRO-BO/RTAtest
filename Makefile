all: pwave

pwave: pwave.cpp mac_clock_gettime.h
	$(CXX) $(CXXFLAGS) -fopenmp pwave.cpp -o pwave -lRTAtelem -lpacket -lrt

clean:
	@rm -f pwave
