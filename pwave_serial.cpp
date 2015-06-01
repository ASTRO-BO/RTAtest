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
//#define TIMERS 1

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

void waveExtract(unsigned short* inBuf, unsigned short* maxBuf,
                 float* timeBuf, unsigned int nPixels,
                 unsigned int nSamples, unsigned int windowSize) {

    //#pragma omp parallel for default(shared)
    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        unsigned short sumn = 0;
        unsigned short sum = 1;

        for(unsigned int sliceIdx=0; sliceIdx<windowSize; sliceIdx++) {
            sum += inBuf[pixelOff + sliceIdx];
            sumn += inBuf[pixelOff + sliceIdx] * sliceIdx;
        }

        unsigned short maxv = sum;
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

void waveExtract2(unsigned short* inBuf, unsigned short* maxBuf,
                  float* timeBuf, unsigned int nPixels,
                  unsigned int nSamples, unsigned int windowSize) {

    unsigned short sum[3000][80];

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

        unsigned short maxSum = 0;
        unsigned int maxSliceIdx = 0;
        for(unsigned int sliceIdx=0; sliceIdx<nSamples-windowSize; sliceIdx++) {
            unsigned short s = sum[pixelIdx][sliceIdx];
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
    unsigned short maxBuf[MAX_NPIXELS];
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

#ifdef TIMERS
            endPacket = system_clock::now();
            elapsedPacket += endPacket - startPacket;
            startExtract = system_clock::now();
#endif
        }

        // Compute waveform extraction
        waveExtract2((unsigned short*)buff, maxBuf, timeBuf, npixels, nsamples, windowSize);


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

    duration<double> elapsed = end-start;
    double throughputE = numEvents / elapsed.count();
    double throughputB = byteCounter / elapsed.count() / 1000000;
    std::cout << numEvents << " events" << std::endl;
#ifdef TIMERS
    std::cout << "Elapsed packet  " << std::setprecision(2) << std::fixed << elapsedPacket.count() << std::endl;
#ifndef OMP
    std::cout << "Elapsed extract " << std::setprecision(2) << std::fixed << elapsedExtract.count() << std::endl;
#else
    std::cout << "Elapsed extract " << std::setprecision(2) << std::fixed << elapsedExtract.count() / omp_get_max_threads() << std::endl;
#endif
#endif
    std::cout << "Elapsed total " << std::setprecision(2) << std::fixed << elapsed.count() << std::endl;
    std::cout << "throughput: " << throughputE << " events/s - " << throughputB << " MB/s" << std::endl;

    return EXIT_SUCCESS;
}
