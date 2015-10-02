#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <ctautils/mac_clock_gettime.h>
#include <CTAMDArray.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <cstdlib>
#include <iomanip>
#include <math.h>
#ifdef OMP
#include <omp.h>
#endif

//#define DEBUG 1
#ifndef TYPE
#define TYPE unsigned short
#endif


const unsigned int NPIXELS = 1855;
const unsigned int NSAMPLES = 30;
const unsigned int IN_SIZE = NPIXELS*NSAMPLES;
const unsigned int OUT_SIZE = NPIXELS;
const unsigned int WINDOW_SIZE = 8;

int main(int argc, char *argv[]) {
    srand(0);

    if(argc < 5) {
        std::cout << "Wrong arguments, please provide config file, raw input file, the number of events in the buffer and number of loops." << std::endl;
        std::cout << argv[0] << " config.xml input.raw <nevents> <loops>" << std::endl;
        return 1;
    }
    char* configFilename = realpath(argv[1], NULL);
    char* inputFilename = realpath(argv[2], NULL);
    unsigned long nevents = std::atol(argv[3]);
    unsigned long loops = std::atol(argv[4]);

    int nthreads = 1;
#ifdef OMP
#pragma omp parallel
{
    #pragma omp master
    nthreads = omp_get_num_threads();
}
#endif
    std::cout << "Using " << nthreads << " threads" << std::endl;

    // timers variables
    struct timespec start, stop;
    double elapsed = 0;

    // input buffers
    TYPE* iBuff = new TYPE[nevents*IN_SIZE];

    // output buffers
    TYPE* oBuffMax = new TYPE[nevents*OUT_SIZE];
    float* oBuffTime = new float[nevents*OUT_SIZE];

    PacketLib::PacketBufferV events(configFilename, inputFilename);
    events.load();
    PacketLib::PacketStream ps(configFilename);
    CTAConfig::CTAMDArray array_conf;
    array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
    unsigned int nevent=0;

    std::cout << "Filling input buffer .." << std::endl;
    while(nevent < nevents) {
        PacketLib::ByteStreamPtr bs = events.getNext();
#ifdef ARCH_BIGENDIAN
        if(!bs->isBigendian())
            bs->swapWord();
#else
        if(bs->isBigendian())
            bs->swapWord();
#endif
        PacketLib::Packet *packet = ps.getPacket(bs);
        PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
        const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");
        CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
        unsigned int npixels = teltype->getCameraType()->getNpixels();
        unsigned int nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
        PacketLib::byte* rBuff = packet->getData()->getStream();
        unsigned int size = packet->getData()->size();
        if(npixels == NPIXELS && nsamples == NSAMPLES) {
            for(unsigned long i=0; i<IN_SIZE; i++)
                iBuff[nevent*IN_SIZE+i] = ((unsigned short*)rBuff)[i];
//            memcpy(iBuff+nevent*IN_SIZE, rBuff, size);
            // adds a -1 or +1 random value to a random position in the vector
            int noise = (int) round((rand() / (float)RAND_MAX) * sizeof(TYPE) - 1);
            int pos = (int) round((rand() / (float) RAND_MAX) * (size/sizeof(TYPE)-2));
            iBuff[nevent*IN_SIZE+pos] += noise;
            nevent++;
        }
    }

    std::cout << "Started .." << std::endl;
    clock_gettime(CLOCK_MONOTONIC, &start);

    #pragma omp parallel
    for(unsigned int loop=0; loop<loops; loop++) {

/*        #pragma omp for
        for(unsigned int pixelIdx = 0; pixelIdx < nevents * NPIXELS; pixelIdx++) {
            unsigned int pixelOff = pixelIdx * NSAMPLES;

            TYPE sumn = 0;
            TYPE sum = 0;

            for(unsigned int sliceIdx = 0; sliceIdx < WINDOW_SIZE; sliceIdx++) {
                sum += iBuff[pixelOff + sliceIdx];
                sumn += iBuff[pixelOff + sliceIdx] * sliceIdx;
            }

            TYPE maxv = sum;
            float maxt = sumn / (float)sum;

            for(unsigned int sampleIdx = 1; sampleIdx < NSAMPLES - WINDOW_SIZE; sampleIdx++) {
                unsigned int prev = sampleIdx-1;
                unsigned int succ = sampleIdx + WINDOW_SIZE-1;
                sum = sum - iBuff[pixelOff+prev] + iBuff[pixelOff+succ];
                sumn = sumn - iBuff[pixelOff+prev] * prev + iBuff[pixelOff+succ] * succ;
                if(sum > maxv) {
                    maxv = sum;
                    maxt = (sumn / (float)sum);
                }
            }
            oBuffMax[pixelIdx] = maxv;
            oBuffTime[pixelIdx] = maxt;*/

        #pragma omp for
        for(unsigned int pixelIdx = 0; pixelIdx < nevents * NPIXELS; pixelIdx++) {
            TYPE sumv[NSAMPLES-WINDOW_SIZE];
            TYPE sumvn[NSAMPLES-WINDOW_SIZE];
            for(unsigned int sliceIdx=0; sliceIdx<NSAMPLES-WINDOW_SIZE; sliceIdx++) {
                sumv[sliceIdx] = 0;
                sumvn[sliceIdx] = 0;
            }
            for(unsigned int sliceIdx=0; sliceIdx<NSAMPLES-WINDOW_SIZE; sliceIdx++) {
                TYPE* iBuffPtr = iBuff + pixelIdx * NSAMPLES + sliceIdx;
                for(unsigned int sampleIdx=0; sampleIdx<WINDOW_SIZE; sampleIdx++) {
                    sumv[sliceIdx] += iBuffPtr[sampleIdx];
                    sumvn[sliceIdx] += iBuffPtr[sampleIdx] * sampleIdx;
                }
            }

            TYPE sum = 0;
            TYPE sumn = 0;
            for(unsigned int sliceIdx=0; sliceIdx<NSAMPLES-WINDOW_SIZE; sliceIdx++) {
                if(sumv[sliceIdx] > sum) {
                    sum = sumv[sliceIdx];
                    sumn = sumvn[sliceIdx];
                }
            }

            oBuffMax[pixelIdx] = sum;
            oBuffTime[pixelIdx] = sumn / (float)sum;

#ifdef DEBUG

#ifdef OMP
#pragma omp critical
{
#endif
            std::cout << "pixel: " << pixelIdx << " samples: ";
            for(unsigned int sampleIdx=0; sampleIdx<NSAMPLES; sampleIdx++)
                std::cout << iBuff[pixelIdx * NSAMPLES + sampleIdx] << " ";
            std::cout << std::endl;
            std::cout << "max: " << " " << oBuffMax[pixelIdx] << " time: " << oBuffTime[pixelIdx] << std::endl;
#ifdef OMP
}
#endif

#endif
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &stop);
    elapsed = timediff(start, stop);

    free(configFilename);
    free(inputFilename);

    delete[] iBuff;
    delete[] oBuffMax;
    delete[] oBuffTime;

    unsigned int eventSize = (NPIXELS * NSAMPLES * sizeof(unsigned short)) / 1024.0; // KiB
    unsigned int bufferSize = (nevents * eventSize) / 1024.0; // MiB
    unsigned int totalEvents = nevents * loops; // events
    unsigned int totalSize = totalEvents * eventSize; // KiB
    float throughputE = totalEvents / (elapsed * 1000.0); // kevents/s
    float throughput = totalSize / (elapsed * 1024.0);    // MiB/s

    const unsigned int width = 9;
    std::cout.setf(ios::fixed);
    std::cout.precision(3);
    std::cout << "Number of events: " << setw(width) << nevents     << " (events)"    << std::endl;
    std::cout << "Number of loops:  " << setw(width) << loops       << " (loops)"     << std::endl;
    std::cout << "Event size:       " << setw(width) << eventSize   << " (MiB)"       << std::endl;
    std::cout << "Buffer size:      " << setw(width) << bufferSize  << " (MiB)"       << std::endl;
    std::cout << "Elapsed total:    " << setw(width) << elapsed     << " (seconds)"   << std::endl;
    std::cout << "Throughput:       " << setw(width) << throughputE << " (kevents/s)" << std::endl;
    std::cout << "Throughput:       " << setw(width) << throughput  << " (MiB/s)"     << std::endl;

    return EXIT_SUCCESS;
}
