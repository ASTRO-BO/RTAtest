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
#define MULTIPLE_TIMERS 1

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

int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and number of packets." << std::endl;
		std::cout << argv[0] << " config.xml input.raw <npackets> [platform] [device]" << std::endl;
		return 1;
	}
	const std::string configFilename(realpath(argv[1], NULL));
	const std::string inputFilename(realpath(argv[2], NULL));
	const unsigned long numevents = std::atol(argv[3]);
	unsigned int platformnum = 0;
    unsigned int devicenum = 0;

	if(argc >= 5)
	    platformnum = atoi(argv[4]);
    if(argc >= 6)
	    devicenum = atoi(argv[5]);

	// load events buffer
	PacketLib::PacketBufferV events(configFilename, inputFilename);
	events.load();
	PacketLib::PacketStream ps(configFilename.c_str());

	CTAConfig::CTAMDArray array_conf;
	std::cout << "Preloading.." << std::endl;
	array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
	std::cout << "Load complete!" << std::endl;

	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	for(unsigned int i=0; i<platforms.size(); i++)
	{
		cl::Platform platform = platforms[i];
        std::cout << "----------------------------" << std::endl;
		std::cout << "Platform " << i << " info:" << std::endl;
		std::string info;
		platform.getInfo(CL_PLATFORM_NAME, &info);
		std::cout << info << std::endl;
		platform.getInfo(CL_PLATFORM_VERSION, &info);
		std::cout << info << std::endl;
		platform.getInfo(CL_PLATFORM_VENDOR, &info);
		std::cout << info << std::endl;
	}
	if(platformnum > platforms.size()) {
	    std::cout << "Cannot use the platform with number " << platformnum << "." << std::endl;
	    return EXIT_FAILURE;
    }

	std::vector<cl::Device> devices;
	platforms[platformnum].getDevices(CL_DEVICE_TYPE_ALL, &devices);
	for(unsigned int i=0; i<devices.size(); i++)
	{
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
	}
	if(devicenum > devices.size()) {
	    std::cout << "Cannot use the device with number " << devicenum << "." << std::endl;
	    return EXIT_FAILURE;
    }

    std::cout << "Using platform " << platformnum << "." << std::endl;
    std::cout << "Using device " << devicenum  << "."<< std::endl;

	cl::Context context(devices);
	cl::CommandQueue queue(context, devices[devicenum], CL_QUEUE_PROFILING_ENABLE, NULL);

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
        program.build(devices);
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

	cl::Kernel koWaveextract(program, "waveextract");

	::size_t workgroupSize = koWaveextract.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(devices[devicenum]);

    const unsigned int MAX_NUM_SAMPLES = 100;
    const unsigned int MAX_NUM_PIXELS = 3000;

    unsigned short maxres[MAX_NUM_PIXELS];
    unsigned short timeres[MAX_NUM_PIXELS];

    cl::Buffer inputBuffer(context, CL_MEM_READ_ONLY, MAX_NUM_PIXELS*MAX_NUM_SAMPLES, NULL, NULL);
    cl::Buffer maxresBuffer(context, CL_MEM_WRITE_ONLY, MAX_NUM_PIXELS*sizeof(unsigned short), NULL, NULL);
    cl::Buffer timeresBuffer(context, CL_MEM_WRITE_ONLY, MAX_NUM_PIXELS*sizeof(unsigned short), NULL, NULL);
    PacketLib::byte* inputMBuffer = (PacketLib::byte*) queue.enqueueMapBuffer(inputBuffer, CL_FALSE, CL_MAP_WRITE, 0, MAX_NUM_PIXELS*MAX_NUM_SAMPLES);
    unsigned short* maxresMBuffer = (unsigned short*) queue.enqueueMapBuffer(maxresBuffer, CL_FALSE, CL_MAP_READ, 0, MAX_NUM_PIXELS);
    unsigned short* timeresMBuffer = (unsigned short*) queue.enqueueMapBuffer(timeresBuffer, CL_FALSE, CL_MAP_READ, 0, MAX_NUM_PIXELS);

	std::chrono::time_point<std::chrono::system_clock> start, end;
#ifdef MULTIPLE_TIMERS
	std::chrono::time_point<std::chrono::system_clock> startcopyto, endcopyto;
	std::chrono::time_point<std::chrono::system_clock> startcopyfrom, endcopyfrom;
	std::chrono::time_point<std::chrono::system_clock> startpacket, endpacket;
	std::chrono::time_point<std::chrono::system_clock> startkernel, endkernel;
	std::chrono::duration<double> elapsedCopyTo(0.);
	std::chrono::duration<double> elapsedCopyFrom(0.);
	std::chrono::duration<double> elapsedPacket(0.);
	std::chrono::duration<double> elapsedKernel(0.);
#endif
	unsigned long event_count = 0, event_size(0.);

	start = std::chrono::system_clock::now();
	while(event_count < numevents)
	{
#ifdef MULTIPLE_TIMERS
        startpacket = std::chrono::system_clock::now();
#endif
		PacketLib::ByteStreamPtr event = events.getNext();
		event_size += event->size();

		/// swap if the stream has a different endianity
#ifdef ARCH_BIGENDIAN
		if(!event->isBigendian())
			event->swapWord();
#else
		if(event->isBigendian())
			event->swapWord();
#endif

		/// decoding packetlib packet
		PacketLib::Packet *packet = ps.getPacket(event);

		/// get telescope id
		PacketLib::DataFieldHeader* dfh = packet->getPacketDataFieldHeader();
		const unsigned int telescopeID = dfh->getFieldValue_16ui("TelescopeID");

		/// get the waveforms
		PacketLib::byte* buff = packet->getData()->getStream();
		PacketLib::dword buffSize = packet->getData()->size();

		/// get npixels and nsamples from ctaconfig using the telescopeID
		CTAConfig::CTAMDTelescopeType* teltype = array_conf.getTelescope(telescopeID)->getTelescopeType();
		int telTypeSim = teltype->getID();
		const unsigned int npixels = teltype->getCameraType()->getNpixels();
		const unsigned int nsamples = teltype->getCameraType()->getPixel(0)->getPixelType()->getNSamples();
/*		std::cout << workgroupSize << std::endl;
		std::cout << npixels << std::endl;*/
#ifdef MULTIPLE_TIMERS
        endpacket = std::chrono::system_clock::now();
#endif

        // send the data to the device
#ifdef MULTIPLE_TIMERS
        startcopyto = std::chrono::system_clock::now();
#endif
        memcpy(inputMBuffer, buff, buffSize);
#ifdef MULTIPLE_TIMERS
        endcopyto = std::chrono::system_clock::now();
#endif

		// compute kernel
#ifdef MULTIPLE_TIMERS
        startkernel = std::chrono::system_clock::now();
#endif
        koWaveextract.setArg(0, inputBuffer);
		koWaveextract.setArg(1, npixels);
		koWaveextract.setArg(2, nsamples);
		koWaveextract.setArg(3, 6);
		koWaveextract.setArg(4, maxresBuffer);
		koWaveextract.setArg(5, timeresBuffer);

		cl::NDRange global(npixels);
		cl::NDRange local(1);
		queue.enqueueNDRangeKernel(koWaveextract, cl::NullRange, global, cl::NullRange);

		// get the data back from the device
		queue.finish();
        endkernel = std::chrono::system_clock::now();

        startcopyfrom = std::chrono::system_clock::now();
		memcpy(maxres, maxresMBuffer, npixels*sizeof(unsigned short));
		memcpy(timeres, timeresMBuffer, npixels*sizeof(unsigned short));
#ifdef MULTIPLE_TIMERS
        endcopyfrom = std::chrono::system_clock::now();
#endif

#ifdef DEBUG
		std::cout << "npixels = " << npixels << std::endl;
		std::cout << "nsamples = " << nsamples << std::endl;
		for(int pixel = 0; pixel<npixels; pixel++) {
			PacketLib::word* s = (PacketLib::word*) buff + pixel * nsamples;
			std::cout << "pixel " << pixel << " samples ";
			for(int k=0; k<nsamples; k++) {
				std::cout << s[k] << " ";
			}
			std::cout << std::endl;

			std::cout << "result " << " " << maxres[pixel] << " " << timeres[pixel] << " " << std::endl;
		}
#endif
        event_count++;
#ifdef MULTIPLE_TIMERS
        elapsedCopyTo += endcopyto-startcopyto;
        elapsedCopyFrom += endcopyfrom-startcopyfrom;
        elapsedPacket += endpacket-startpacket;
        elapsedKernel += endkernel-startkernel;
#endif
	}

#ifdef MULTIPLE_TIMERS
    std::cout << "Elapsed packet: " << elapsedPacket.count() << std::endl;
    std::cout << "Elapsed copyto: " << elapsedCopyTo.count() << std::endl;
    std::cout << "Elapsed kernel: " << elapsedKernel.count() << std::endl;
    std::cout << "Elapsed copyfrom: " << elapsedCopyFrom.count() << std::endl;
#endif

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end-start;
	double throughput = event_count / elapsed.count();
	double throughput2 = event_size / elapsed.count() / 1000000;
	std::cout << event_count << " events sent in " << elapsed.count() << " s" << std::endl;
	std::cout << "mean event size: " << event_size / event_count << std::endl;
	std::cout << "throughput: " << throughput << " msg/s = " << throughput2 << " MB/s" << std::endl;

	return EXIT_SUCCESS;
}
