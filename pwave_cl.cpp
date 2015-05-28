#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <CTAMDArray.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdlib>

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

//#define DEBUG 1

using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::system_clock;

inline std::string loadProgram(std::string input)
{
    std::ifstream stream(input.c_str());
    if (!stream.is_open()) {
        std::cout << "Cannot open file: " << input << std::endl;
        exit(1);
    }
    return std::string(std::istreambuf_iterator<char>(stream),
            (std::istreambuf_iterator<char>()));
}

void printPlatforms(std::vector<cl::Platform>& platforms) {
    std::string info;
    for(unsigned int i=0; i<platforms.size(); i++) {
        cl::Platform platform = platforms[i];
        std::cout << "----------------------------" << std::endl;
        std::cout << "Platform " << i << std::endl;
        platform.getInfo(CL_PLATFORM_NAME, &info);
        std::cout << info << std::endl;
        platform.getInfo(CL_PLATFORM_VERSION, &info);
        std::cout << info << std::endl;
        platform.getInfo(CL_PLATFORM_VENDOR, &info);
        std::cout << info << std::endl;
    }
    std::cout << "----------------------------" << std::endl;
}

void printDevices(std::vector<cl::Device>& devices) {
    std::vector<::size_t> wgsizes;
    for(unsigned int i=0; i<devices.size(); i++) {
        cl::Device device = devices[i];
        std::cout << "----------------------------" << std::endl;
        std::cout << "Device " << i << " info:" << std::endl;
        std::string info;
        device.getInfo(CL_DEVICE_NAME, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DEVICE_VENDOR, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DEVICE_VERSION, &info);
        std::cout << info << std::endl;
        device.getInfo(CL_DRIVER_VERSION, &info);
        std::cout << info << std::endl;
        std::cout << "Work item sizes: ";
        device.getInfo(CL_DEVICE_MAX_WORK_ITEM_SIZES, &wgsizes);
        for(unsigned int j=0; j<wgsizes.size(); j++)
            std::cout << wgsizes[j] << " ";
        std::cout << std::endl;
        ::size_t wgsize;
        device.getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, &wgsize);
        std::cout << "Max work group size: " << wgsize << std::endl;
        cl_ulong maxAlloc;
        device.getInfo(CL_DEVICE_MAX_MEM_ALLOC_SIZE, &maxAlloc);
        std::cout << "Max alloc: " << maxAlloc << std::endl;
    }
    std::cout << "----------------------------" << std::endl;
}



int main(int argc, char *argv[]) {
    if(argc < 4) {
        std::cout << "Wrong arguments, please provide config file, raw input file and number of events." << std::endl;
        std::cout << argv[0] << " config.xml input.raw <nevents> [platformnum] [devicenum]" << std::endl;
        return 1;
    }

    // parse command line options
    const std::string configFilename(realpath(argv[1], NULL));
    const std::string inputFilename(realpath(argv[2], NULL));
    const unsigned long numEvents = std::atol(argv[3]);
    unsigned int platformnum = 0;
    unsigned int devicenum = 0;
    if(argc >= 5)
        platformnum = atoi(argv[4]);
    if(argc >= 6)
        devicenum = atoi(argv[5]);

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
    std::ifstream file("extract_wave.aocx", std::ios::binary);
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> b(size);
    file.read(b.data(), size);
    cl::Program::Binaries binaries(1, std::make_pair(&b[0], size));
    cl::Program program(context, devices, binaries);
#else
    std::string source = loadProgram("extract_wave.cl");
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

    // allocate buffers
    const int MAX_NPIXELS = 3000;
    const int MAX_NSAMPLES = 100;
    unsigned short maxBuf[MAX_NPIXELS];
    unsigned short timeBuf[MAX_NPIXELS];
    cl::Buffer inDevBuf(context, CL_MEM_READ_ONLY, MAX_NPIXELS*MAX_NSAMPLES*sizeof(unsigned short), NULL, NULL);
    cl::Buffer maxDevBuf(context, CL_MEM_WRITE_ONLY, MAX_NPIXELS*sizeof(unsigned short), NULL, NULL);
    cl::Buffer timeDevBuf(context, CL_MEM_WRITE_ONLY, MAX_NPIXELS*sizeof(unsigned short), NULL, NULL);
    PacketLib::byte* inMapBuf = (PacketLib::byte*) queue.enqueueMapBuffer(inDevBuf, CL_TRUE, CL_MAP_WRITE, 0, MAX_NPIXELS*MAX_NSAMPLES);
    unsigned short* maxMapBuf = (unsigned short*) queue.enqueueMapBuffer(maxDevBuf, CL_TRUE, CL_MAP_READ, 0, MAX_NPIXELS);
    unsigned short* timeMapBuf = (unsigned short*) queue.enqueueMapBuffer(timeDevBuf, CL_TRUE, CL_MAP_READ, 0, MAX_NPIXELS);

    time_point<system_clock> start = system_clock::now();
    time_point<system_clock> startPacket, endPacket;
    time_point<system_clock> startCopyTo, endCopyTo;
    time_point<system_clock> startExtract, endExtract;
    time_point<system_clock> startCopyFrom, endCopyFrom;
    duration<double> elapsedPacket(0.0), elapsedExtract(0.0);
    duration<double> elapsedCopyTo(0.0), elapsedCopyFrom(0.0);

    unsigned long byteCounter = 0;
    for(unsigned long eventCounter=0; eventCounter<numEvents; eventCounter++) {

        // get event data
        startPacket = system_clock::now();
        PacketLib::ByteStreamPtr event = events.getNext();
#ifdef ARCH_BIGENDIAN
        if(!event->isBigendian())
            event->swapWord();
#else
        if(event->isBigendian())
            event->swapWord();
#endif
        PacketLib::Packet *packet = ps.getPacket(event);
        PacketLib::byte* inBuff = packet->getData()->getStream();
        PacketLib::dword inBuffSize = packet->getData()->size();
        PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
        const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");
        CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
        int telTypeSim = teltype->getID();
        unsigned int nPixels = teltype->getCameraType()->getNpixels();
        unsigned int nSamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
        const unsigned int windowSize = 6;
        endPacket = system_clock::now();
        elapsedPacket += endPacket - startPacket;

        // copy input buffer into pinned dev memory
        startCopyTo = std::chrono::system_clock::now();
        memcpy(inMapBuf, inBuff, inBuffSize);
        endCopyTo = std::chrono::system_clock::now();
        elapsedCopyTo += endCopyTo - startCopyTo;

        // compute waveform extraction
        startExtract = std::chrono::system_clock::now();
        kernelExtract.setArg(0, inBuff);
        kernelExtract.setArg(1, maxBuf);
        kernelExtract.setArg(2, timeBuf);
        kernelExtract.setArg(3, nPixels);
        kernelExtract.setArg(4, nSamples);
        kernelExtract.setArg(5, windowSize);
        cl::NDRange global(nPixels);
        cl::NDRange local(cl::NullRange);
        queue.enqueueNDRangeKernel(kernelExtract, cl::NullRange, global, local);
        queue.finish();
        endExtract = system_clock::now();
        elapsedExtract += endExtract - startExtract;

        // copy pinned dev memory buffers to output
        startCopyFrom = std::chrono::system_clock::now();
        memcpy(maxBuf, maxMapBuf, nPixels*sizeof(unsigned short));
        memcpy(timeBuf, timeMapBuf, nPixels*sizeof(unsigned short));
        endCopyFrom = std::chrono::system_clock::now();

#ifdef DEBUG
        std::cout << "npixels: " << nPixels << std::endl;
        std::cout << "nsamples: " << nSamples << std::endl;
        for(int pixelIdx = 0; pixelIdx<nPixels; pixelIdx++) {
            PacketLib::word* s = (PacketLib::word*) buff + pixelIdx * nSamples;
            std::cout << "pixel: " << pixelIdx << " samples: ";
            for(int sampleIdx=0; sampleIdx<nSamples; sampleIdx++) {
                std::cout << s[sampleIdx] << " ";
            }
            std::cout << std::endl;
            std::cout << "result: " << " " << maxBuf[pixelIdx] << " " << timeBuf[pixelIdx] << " " << std::endl;
        }
#endif
        byteCounter += inBuffSize;
    }

    time_point<system_clock> end = system_clock::now();

    duration<double> elapsed = end-start;
    double throughputE = numEvents / elapsed.count();
    double throughputB = byteCounter / elapsed.count() / 1000000;
    std::cout << numEvents << " events" << std::endl;
    std::cout << "Elapsed packet " << elapsedPacket.count() << std::endl;
    std::cout << "Elapsed copy to " << elapsedCopyTo.count() << std::endl;
    std::cout << "Elapsed extract " << elapsedExtract.count() << std::endl;
    std::cout << "Elapsed copy from " << elapsedCopyFrom.count() << std::endl;
    std::cout << "Elapsed total " << elapsed.count() << std::endl;
    std::cout << "throughput: " << throughputE << " events/s - " << throughputB << " MB/s" << std::endl;

    return EXIT_SUCCESS;
}
