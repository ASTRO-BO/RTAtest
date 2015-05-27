#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <CTAMDArray.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdlib>

//#define DEBUG 1

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

void waveExtract(unsigned short* inBuf, unsigned short* maxBuf,
                 unsigned short* timeBuf, unsigned int nPixels,
                 unsigned int nSamples, unsigned int windowSize) {

    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        unsigned short sumn = 0;
        unsigned short sum = 1;

        for(unsigned int winIdx=0; winIdx<windowSize; winIdx++) {
            sum += inBuf[pixelOff + winIdx];
            sumn += inBuf[pixelOff + winIdx] * winIdx;
        }

        unsigned short maxv = sum;
        unsigned short maxt = sumn / (float)sum;

        for(unsigned int sampleIdx=1; sampleIdx<nSamples-windowSize; sampleIdx++) {
            unsigned int prev = sampleIdx-1;
            unsigned int succ = sampleIdx+windowSize-1;
            sum = sum - inBuf[pixelOff+prev] + inBuf[pixelOff+succ];
            sumn = sumn - inBuf[pixelOff+prev] * prev + inBuf[pixelOff+succ] * succ;
            if(sum > maxv) {
                maxv = sum;
                maxt = (sumn / (float)sum);
            }
        }

        maxBuf[pixelIdx] = maxv;
        timeBuf[pixelIdx] = maxt;
    }
}

int main(int argc, char *argv[]) {
	if(argc < 4) {
		std::cout << "Wrong arguments, please provide config file, raw input file and number of events." << std::endl;
		std::cout << argv[0] << " config.xml input.raw <nevents>" << std::endl;
		return 1;
	}
	const std::string configFilename(realpath(argv[1], NULL));
	const std::string inputFilename(realpath(argv[2], NULL));
	const unsigned long numEvents = std::atol(argv[3]);

	PacketLib::PacketBufferV events(configFilename, inputFilename);
	events.load();
	PacketLib::PacketStream ps(configFilename.c_str());

	CTAConfig::CTAMDArray array_conf;
	std::cout << "Preloading.." << std::endl;
	array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
	std::cout << "Load complete!" << std::endl;

    const unsigned int MAX_NPIXELS = 3000;
    unsigned short maxBuf[MAX_NPIXELS];
    unsigned short timeBuf[MAX_NPIXELS];

	time_point<system_clock> start = system_clock::now();

    unsigned long eventCounter = 0;
    unsigned long byteCounter = 0;
    while(eventCounter < numEvents) {

        // Get event data
        PacketLib::ByteStreamPtr event = events.getNext();
#ifdef ARCH_BIGENDIAN
        if(!event->isBigendian())
            event->swapWord();
#else
        if(event->isBigendian())
            event->swapWord();
#endif
        PacketLib::Packet *packet = ps.getPacket(event);
        PacketLib::byte* buff = packet->getData()->getStream();
        PacketLib::dword buffSize = packet->getData()->size();
        PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
        const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");
        CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
        int telTypeSim = teltype->getID();
        const unsigned int npixels = teltype->getCameraType()->getNpixels();
        const unsigned int nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
        const unsigned int windowSize = 6;

        // Compute waveform extraction
        waveExtract((unsigned short*)buff, maxBuf, timeBuf, npixels, nsamples, windowSize);

#ifdef DEBUG
		std::cout << "npixels: " << npixels << std::endl;
		std::cout << "nsamples: " << nsamples << std::endl;
		for(int pixelIdx = 0; pixelIdx<npixels; pixelIdx++) {
			PacketLib::word* s = (PacketLib::word*) buff + pixelIdx * nsamples;
			std::cout << "pixel: " << pixelIdx << " samples: ";
			for(int sampleIdx=0; sampleIdx<nsamples; sampleIdx++) {
				std::cout << s[sampleIdx] << " ";
			}
			std::cout << std::endl;
			std::cout << "result: " << " " << maxBuf[pixelIdx] << " " << timeBuf[pixelIdx] << " " << std::endl;
		}
#endif
        eventCounter++;
        byteCounter += buffSize;
    }

	time_point<system_clock> end = system_clock::now();

	duration<double> elapsed = end-start;
	double throughputE = eventCounter / elapsed.count();
	double throughputB = byteCounter / elapsed.count() / 1000000;
	std::cout << eventCounter << " events sent in " << elapsed.count() << " s" << std::endl;
	std::cout << "throughput: " << throughputE << " events/s - " << throughputB << " MB/s" << std::endl;

	return EXIT_SUCCESS;
}
