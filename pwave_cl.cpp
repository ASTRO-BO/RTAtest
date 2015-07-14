#include "CLUtility.h"
#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <CTAMDArray.h>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <iomanip>

//#define DEBUG 1
//#define TIMERS 1

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

int main(int argc, char *argv[]) {
    if(argc < 4) {
        std::cout << "Wrong arguments, please provide config file, raw input file and number of events." << std::endl;
        std::cout << argv[0] << " config.xml input.raw <nevents> [platformnum] [devicenum] [buffer size]" << std::endl;
        return 1;
    }

    // parse command line options
    const std::string configFilename(realpath(argv[1], NULL));
    const std::string inputFilename(realpath(argv[2], NULL));
    const unsigned long numEvents = std::atol(argv[3]);
    unsigned int platformnum = 0;
    unsigned int devicenum = 0;
    unsigned int N = 1;
    if(argc >= 5)
        platformnum = atoi(argv[4]);
    if(argc >= 6)
        devicenum = atoi(argv[5]);
    if(argc >= 7)
        N = atoi(argv[6]);

    PacketLib::PacketBufferV events(configFilename, inputFilename);
    events.load();
    PacketLib::PacketStream ps(configFilename.c_str());

    CTAConfig::CTAMDArray array_conf;
    std::cout << "Preloading.." << std::endl;
    array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
    std::cout << "Load complete!" << std::endl;

    // select a platform and device
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    printPlatforms(platforms);
    if(platformnum > platforms.size()) {
        std::cout << "Cannot use the platform with number " << platformnum << "." << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Using platform " << platformnum << "." << std::endl;
    std::vector<cl::Device> devices;
    platforms[platformnum].getDevices(CL_DEVICE_TYPE_ALL, &devices);
    printDevices(devices);
    if(devicenum > devices.size()) {
        std::cout << "Cannot use the device with number " << devicenum << "." << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Using device " << devicenum  << "."<< std::endl;
    std::cout << "----------------------------" << std::endl;

    cl::Context context(devices);

    // compile OpenCL programs
#ifdef CL_ALTERA
    std::ifstream file("extract.aocx", std::ios::binary);
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> b(size);
    file.read(b.data(), size);
    cl::Program::Binaries binaries(1, std::make_pair(&b[0], size));
    cl::Program program(context, devices, binaries);
#else
    std::string source = loadProgram("extract.cl");
    cl::Program::Sources sources(1, std::make_pair(source.c_str(), source.length()));
    cl::Program program(context, sources);
#endif

    try {
        program.build(devices, "-cl-finite-math-only");
    }
    catch (cl::Error err) {
        if (err.err() == CL_BUILD_PROGRAM_FAILURE) {
            cl_int err;
            cl::STRING_CLASS buildlog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[devicenum], &err);
            std::cout << "Building error! Log: " << std::endl;
            std::cout << buildlog << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // create a kernel and a queue
    cl::Kernel kernelExtract(program, "waveExtract");
    cl::CommandQueue queue(context, devices[devicenum], CL_QUEUE_PROFILING_ENABLE, NULL);

    const unsigned int MAX_NPIXELS = 3000;
    const unsigned int MAX_NSAMPLES = 100;
    const unsigned int MAX_EVENT_SIZE = MAX_NPIXELS * MAX_NSAMPLES * sizeof(unsigned short) * N;
    cl::Buffer inDevBuf(context, CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR, MAX_EVENT_SIZE, NULL, NULL);
    cl::Buffer maxDevBuf(context, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, MAX_NPIXELS*sizeof(unsigned int)*N, NULL, NULL);
    cl::Buffer timeDevBuf(context, CL_MEM_WRITE_ONLY | CL_MEM_ALLOC_HOST_PTR, MAX_NPIXELS*sizeof(float)*N, NULL, NULL);
    PacketLib::byte* inData = (PacketLib::byte*) queue.enqueueMapBuffer(inDevBuf, CL_FALSE, CL_MAP_WRITE, 0, MAX_EVENT_SIZE);
    unsigned short* maxData = (unsigned short*) queue.enqueueMapBuffer(maxDevBuf, CL_FALSE, CL_MAP_READ, 0, MAX_NPIXELS*sizeof(unsigned int)*N);
    float* timeData = (float*) queue.enqueueMapBuffer(timeDevBuf, CL_FALSE, CL_MAP_READ, 0, MAX_NPIXELS*sizeof(float)*N);

    time_point<system_clock> start = system_clock::now();
#ifdef TIMERS
    time_point<system_clock> startPacket, endPacket;
    time_point<system_clock> startCopyTo, endCopyTo;
    time_point<system_clock> startBuffering, endBuffering;
    time_point<system_clock> startExtract, endExtract;
    time_point<system_clock> startCopyFrom, endCopyFrom;
    duration<double> elapsedPacket(0.0), elapsedExtract(0.0);
    duration<double> elapsedBuffering(0.0), elapsedCopyTo(0.0), elapsedCopyFrom(0.0);
#endif

    unsigned long byteCounter = 0;
    for(unsigned long eventCounter=0; eventCounter<numEvents; eventCounter++) {

#ifdef TIMERS
        startPacket = system_clock::now();
#endif
        // get next packet
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
        unsigned int nPixels = teltype->getCameraType()->getNpixels();
        unsigned int nSamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
        const unsigned int windowSize = 6;
#ifdef DEBUG
        std::cout << "window size: " << windowSize << std::endl;
        std::cout << "nPixels:     " << nPixels << std::endl;
        std::cout << "nSamples:    " << nSamples << std::endl;
        std::cout << "buffSize:    " << buffSize << std::endl;
#endif

#ifdef TIMERS
        endPacket = system_clock::now();
        elapsedPacket += endPacket - startPacket;
        startBuffering = std::chrono::system_clock::now();
#endif
        // copy raw data multiple times (to simulate packet buffering)
        for(unsigned int i=0; i<N; i++)
            memcpy(inData+i*nPixels*nSamples, buff, buffSize);

        // send input data to the device
#ifndef TIMERS
        queue.enqueueWriteBuffer(inDevBuf, CL_FALSE, 0, buffSize*N, inData);
#else
        endBuffering = std::chrono::system_clock::now();
        elapsedBuffering += endCopyTo - startCopyTo;
        startCopyTo = std::chrono::system_clock::now();
        queue.enqueueWriteBuffer(inDevBuf, CL_TRUE, 0, buffSize*N, inData);
        endCopyTo = std::chrono::system_clock::now();
        elapsedCopyTo += endCopyTo - startCopyTo;
        startExtract = std::chrono::system_clock::now();
#endif

        // compute waveform extraction kernels
        kernelExtract.setArg(0, inDevBuf);
        kernelExtract.setArg(1, maxDevBuf);
        kernelExtract.setArg(2, timeDevBuf);
        kernelExtract.setArg(3, nSamples);
        cl::NDRange global(nPixels*N);
        cl::NDRange local(cl::NullRange);
        queue.enqueueNDRangeKernel(kernelExtract, cl::NullRange, global, local);
#ifdef TIMERS
        queue.finish();
        endExtract = system_clock::now();
        elapsedExtract += endExtract - startExtract;
        startCopyFrom = std::chrono::system_clock::now();
#endif
        // copy output data back from device
        queue.enqueueReadBuffer(maxDevBuf, CL_FALSE, 0, nPixels*sizeof(unsigned short)*N, maxData);
        queue.enqueueReadBuffer(timeDevBuf, CL_TRUE, 0, nPixels*sizeof(float)*N, timeData);


#ifdef TIMERS
        endCopyFrom = std::chrono::system_clock::now();
        elapsedCopyFrom += endCopyFrom - startCopyFrom;
#endif

#ifdef DEBUG
        for(int pixelIdx = 0; pixelIdx<nPixels*N; pixelIdx++) {
            PacketLib::word* s = (PacketLib::word*) buff + (pixelIdx%nPixels) * nSamples;
            std::cout << "pixel: " << pixelIdx << " samples: ";
            for(int sampleIdx=0; sampleIdx<nSamples; sampleIdx++) {
                std::cout << s[sampleIdx] << " ";
            }
            std::cout << std::endl;
            std::cout << "result: " << " " << maxData[pixelIdx] << " " << timeData[pixelIdx] << std::endl;
        }
#endif
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
    std::cout << "Elapsed packet:         " << setw(10) << elapsedPacket.count() << " s" << std::endl;
    std::cout << "Elapsed buffering:      " << setw(10) << elapsedBuffering.count() << " s" << std::endl;
    std::cout << "Elapsed copy to:        " << setw(10) << elapsedCopyTo.count() << " s" << std::endl;
    std::cout << "Elapsed extract kernel: " << setw(10) << elapsedExtract.count() << " s" << std::endl;
    std::cout << "Elapsed copy from:      " << setw(10) << elapsedCopyFrom.count() << " s" << std::endl;
    double memPerc = (elapsedBuffering.count()+elapsedCopyTo.count()+elapsedCopyFrom.count()) / elapsed.count() * 100;
    std::cout << "Elapsed on memory:      " << setw(8) << memPerc << " %" << std::endl;
    double kernelPerc = elapsedExtract.count() / elapsed.count() * 100;
    std::cout << "Elapsed on kernels:     " << setw(8) << kernelPerc << " %" << std::endl;
#endif
    double throughputEvt = (numEvents*N) / elapsed;
    double throughput = (byteCounter*N) / (elapsed * 1000000.0);

    std::cout << std::endl <<  "TOTAL RESULTS" << std::endl;
    std::cout << "---------------" << std::endl;
    std::cout << "Number of events:  " << setw(10) << (float)numEvents << std::endl;
    std::cout << "Time Elapsed:      " << setw(10) << elapsed << " s" << std::endl;
    std::cout << "Throughput:        " << setw(10) << throughputEvt << " events/s" << std::endl << std::endl;
    std::cout << "(processed " << (byteCounter*N) / 1000000.0 << " MB at " << throughput << " MB/s)" << std::endl;

#ifdef TIMERS
    double throughputEvtNoBuff = (numEvents*N) / (elapsed.count()-elapsedBuffering.count());
    double throughputNoBuff = (byteCounter*N) / (elapsed.count()-elapsedBuffering.count());
    std::cout << "Throughput no buffering:  " << setw(10) << throughputEvtNoBuff << " events/s" << std::endl;
    std::cout << "                          " << setw(10) << throughputNoBuff << " MB/s" << std::endl;
#endif

    return EXIT_SUCCESS;
}
