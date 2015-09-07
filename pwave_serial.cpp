#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <CTAMDArray.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#ifdef OMP
#include <omp.h>
#endif

//#define DEBUG 1
#define TIMERS 1
#define TYPE unsigned short

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

void waveExtract(TYPE* inBuf, TYPE* maxBuf,
                 float* timeBuf, unsigned int nPixels,
                 unsigned int nSamples, unsigned int windowSize) {

    //#pragma omp parallel for default(shared)
    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        TYPE sumn = 0;
        TYPE sum = 1;

        for(unsigned int sliceIdx=0; sliceIdx<windowSize; sliceIdx++) {
            sum += inBuf[pixelOff + sliceIdx];
            sumn += inBuf[pixelOff + sliceIdx] * sliceIdx;
        }

        TYPE maxv = sum;
        float maxt = sumn / (float)sum;

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

void waveExtract2(TYPE* inBuf, TYPE* maxBuf,
                  float* timeBuf, unsigned int nPixels,
                  unsigned int nSamples, unsigned int windowSize) {

    TYPE sum[3000][80];

    //#pragma omp parallel for default(shared)
    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        sum[pixelIdx][0] = 0;
        for(unsigned int sliceIdx=0; sliceIdx<windowSize; sliceIdx++) {
            sum[pixelIdx][0] += inBuf[pixelOff + sliceIdx];
        }
        for(unsigned int sliceIdx=1; sliceIdx<nSamples-windowSize; sliceIdx++) {
            unsigned int prev = sliceIdx-1;
            unsigned int succ = sliceIdx+windowSize-1;
            sum[pixelIdx][sliceIdx] = sum[pixelIdx][prev] - inBuf[pixelOff+prev] + inBuf[pixelOff+succ];
        }
    }

    //#pragma omp parallel for default(shared)
    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        TYPE maxSum = 0;
        unsigned int maxSliceIdx = 0;
        for(unsigned int sliceIdx=0; sliceIdx<nSamples-windowSize; sliceIdx++) {
            TYPE s = sum[pixelIdx][sliceIdx];
            if(s > maxSum) {
                maxSum = s;
                maxSliceIdx = sliceIdx;
            }
        }
        float maxT = 0;
        for(unsigned int i=maxSliceIdx; i<maxSliceIdx+windowSize; i++) {
            maxT += inBuf[pixelOff+i] * i;
        }

        maxT /= maxSum;

        maxBuf[pixelIdx] = maxSum;
        timeBuf[pixelIdx] = maxT;

#ifndef OMP
#ifdef DEBUG
        std::cout << "pixel: " << pixelIdx << " samples: ";
        for(int sampleIdx=0; sampleIdx<nSamples; sampleIdx++) {
            std::cout << inBuf[pixelOff + sampleIdx] << " ";
        }
        std::cout << std::endl;
        std::cout << "max: " << " " << maxBuf[pixelIdx] << " time: " << timeBuf[pixelIdx] << std::endl;
#endif
#endif
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
    TYPE maxBuf[MAX_NPIXELS];
    float timeBuf[MAX_NPIXELS];

    time_point<system_clock> start = system_clock::now();
#ifdef TIMERS
    time_point<system_clock> startPacket, endPacket;
    time_point<system_clock> startExtract, endExtract;
    duration<double> elapsedPacket(0.0), elapsedExtract(0.0);
#endif

    unsigned long byteCounter = 0;
#ifdef TIMERS
    #pragma omp parallel for default(none) firstprivate(maxBuf, timeBuf, startPacket, endPacket, startExtract, endExtract) shared(elapsedPacket, elapsedExtract, byteCounter, ps, array_conf, events)
#else
    #pragma omp parallel for default(none) firstprivate(maxBuf, timeBuf) shared(byteCounter, ps, array_conf, events)
#endif
    for(unsigned long eventCounter=0; eventCounter<numEvents; eventCounter++) {

        // Get event data
        PacketLib::byte* buff;
        PacketLib::dword buffSize;
        unsigned int npixels;
        unsigned int nsamples;
        const unsigned int windowSize = 6;
        TYPE* buffT = new TYPE[300000];

        #pragma omp critical
        {
#ifdef TIMERS
            startPacket = system_clock::now();
#endif

            PacketLib::ByteStreamPtr event = events.getNext();

#ifdef ARCH_BIGENDIAN
            if(!event->isBigendian())
                event->swapWord();
#else
            if(event->isBigendian())
                event->swapWord();
#endif
            PacketLib::Packet *packet = ps.getPacket(event);
            buff = packet->getData()->getStream();
            buffSize = packet->getData()->size();
            PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
            const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");
            CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
            int telTypeSim = teltype->getID();
            npixels = teltype->getCameraType()->getNpixels();
            nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();

            // input data type conversion
            for(unsigned int i=0; i<npixels*nsamples; i++)
                buffT[i] = (TYPE) buff[i];

#ifdef TIMERS
            endPacket = system_clock::now();
            elapsedPacket += endPacket - startPacket;
            startExtract = system_clock::now();
#endif
        }

        // Compute waveform extraction
        waveExtract(buffT, maxBuf, timeBuf, npixels, nsamples, windowSize);


#ifdef TIMERS
        #pragma omp critical
        {
            endExtract = system_clock::now();
            elapsedExtract += endExtract - startExtract;
        }
#endif

        #pragma omp atomic
        byteCounter += buffSize;
    }

    time_point<system_clock> end = system_clock::now();

    duration<double> elapsedD = end-start;
    double elapsed = elapsedD.count();

    std::cout.setf(ios::fixed);
    std::cout.precision(2);
#ifdef TIMERS
    std::cout << "PARTIAL RESULTS" << std::endl;
    std::cout << "---------------" << std::endl;
    std::cout << "Elapsed packet:    " << setw(10) << elapsedPacket.count() << " s" << std::endl;
#ifndef OMP
    std::cout << "Elapsed extract:   " << setw(10) << elapsedExtract.count() << " s" << std::endl;
#else
    std::cout << "Elapsed extract:   " << setw(10) << elapsedExtract.count() / omp_get_max_threads() << " s" << std::endl;
#endif
#endif
    double throughputEvt = numEvents / elapsed;
    double throughput = byteCounter / (elapsed * 1000000.0);

    std::cout << std::endl <<  "TOTAL RESULTS" << std::endl;
    std::cout << "---------------" << std::endl;
    std::cout << "Number of events:  " << setw(10) << (float)numEvents << std::endl;
    std::cout << "Time Elapsed:      " << setw(10) << elapsed << " s" << std::endl;
    std::cout << "Throughput:        " << setw(10) << throughputEvt << " events/s" << std::endl << std::endl;
    std::cout << "(processed " << byteCounter / 1000000.0 << " MB at " << throughput << " MB/s)" << std::endl;

    return EXIT_SUCCESS;
}
