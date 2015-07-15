#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstdlib>

//#define DEBUG 1

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

void waveExtract(unsigned short* inBuf, unsigned short* maxBuf,
                 float* timeBuf, unsigned int nPixels,
                 unsigned int nSamples, unsigned int windowSize) {

    for(unsigned int pixelIdx=0; pixelIdx<nPixels; pixelIdx++) {
        unsigned int pixelOff = pixelIdx * nSamples;

        unsigned short sumn = 0;
        unsigned short sum = 0;

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

#ifdef DEBUG
        std::cout << "pixel: " << pixelIdx << " samples: ";
        for(int sampleIdx=0; sampleIdx<nSamples; sampleIdx++) {
            std::cout << inBuf[pixelOff + sampleIdx] << " ";
        }
        std::cout << std::endl;
        std::cout << "max: " << " " << maxBuf[pixelIdx] << " time: " << timeBuf[pixelIdx] << std::endl;
#endif
    }
}

int main(int argc, char *argv[]) {
    const unsigned int NPIXELS = 1800;
    const unsigned int NSAMPLES = 30;
    unsigned short* buff = (unsigned short*) aligned_alloc(64, NPIXELS*NSAMPLES*sizeof(unsigned short));
    std::cout << "Buff size: " << NPIXELS*NSAMPLES*sizeof(unsigned short) << std::endl;
    unsigned short* maxBuf = (unsigned short*) aligned_alloc(64, NPIXELS*sizeof(unsigned short));
    std::cout << "Max buff size: " << NPIXELS*sizeof(unsigned short) << std::endl;
    float* timeBuf = (float*) aligned_alloc(64, NPIXELS*sizeof(float));
    std::cout << "Time buff size: " << NPIXELS*sizeof(float) << std::endl;
    std::cout << "Total buffs size: " <<  NPIXELS*NSAMPLES*sizeof(unsigned short)+ NPIXELS*sizeof(unsigned short) + NPIXELS*sizeof(float) << std::endl;
    
    duration<double> elapsedExtract(0.0);
    unsigned long byteCounter = 0;
    unsigned long numEvents = 1;

    if(argc > 1)
        numEvents = atol(argv[1]);

    for(unsigned int i=0; i<NPIXELS*NSAMPLES; i++)
        buff[i] = (std::rand() / (float)RAND_MAX) * 255;

    time_point<system_clock> start = system_clock::now();
    for(unsigned long eventCounter=0; eventCounter<numEvents; eventCounter++) {
        time_point<system_clock> startExtract, endExtract;
        startExtract = system_clock::now();
        waveExtract(buff, maxBuf, timeBuf, NPIXELS, NSAMPLES, 6);
        endExtract = system_clock::now();
        elapsedExtract += endExtract - startExtract;
        byteCounter += NPIXELS*NSAMPLES;
    }

    time_point<system_clock> end = system_clock::now();

    duration<double> elapsed = end-start;
    double throughputE = byteCounter / elapsedExtract.count() / 1000000;
    double throughputB = byteCounter / elapsed.count() / 1000000;
    std::cout << numEvents << " events" << std::endl;
    std::cout << "Elapsed extract " << std::setprecision(2) << std::fixed << elapsedExtract.count() << std::endl;
    std::cout << "Elapsed total " << std::setprecision(2) << std::fixed << elapsed.count() << std::endl;
    std::cout << "throughput extract: " << throughputE << " MB/s - total: " << throughputB << " MB/s" << std::endl;

    return EXIT_SUCCESS;
}
