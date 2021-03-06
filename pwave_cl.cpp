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
	if(argc <= 3)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and number of packets." << std::endl;
		std::cout << argv[0] << " config.xml input.raw <npackets>" << std::endl;
		return 1;
	}
	const std::string configFilename(realpath(argv[1], NULL));
	const std::string inputFilename(realpath(argv[2], NULL));
	const unsigned long numevents = std::atol(argv[3]);

	// load events buffer
	PacketLib::PacketBufferV events(configFilename, inputFilename);
	events.load();
	PacketLib::PacketStream ps(configFilename.c_str());

	CTAConfig::CTAMDArray array_conf;
	std::cout << "Preloading.." << std::endl;
	array_conf.loadConfig("AARPROD2", "PROD2_telconfig.fits.gz", "Aar.conf", "conf/");
	std::cout << "Load complete!" << std::endl;

	std::chrono::time_point<std::chrono::system_clock> start, end;
	unsigned long event_count = 0, event_size = 0;

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

	std::vector<cl::Device> devices;
	platforms[0].getDevices(CL_DEVICE_TYPE_ALL, &devices);
    std::cout << "Using platform 0." << std::endl;
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

    std::cout << "Using device 0." << std::endl;
	cl::Context context(devices);

	cl::CommandQueue queue(context, devices[0], 0, NULL);

    std::string source = loadProgram("extract_wave.cl");

	cl::Program::Sources sources(1, std::make_pair(source.c_str(), source.length()));
	cl::Program program(context, sources);
	program.build(devices);

	cl::Kernel koWaveextract(program, "waveextract");

	::size_t workgroupSize = koWaveextract.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(devices[0]);

	start = std::chrono::system_clock::now();
	while(event_count++ < numevents)
	{
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
#ifdef DEBUG
		std::cout << workgroupSize << std::endl;
		std::cout << npixels << std::endl;
#endif

		// compute waveform extraction
		cl::Buffer waveCLBuffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, buffSize, buff, NULL);

		std::vector<unsigned short> maxres(npixels);
		std::vector<unsigned short> timeres(npixels);
		cl::Buffer maxresCLBuffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, npixels*sizeof(unsigned short), maxres.data(), NULL);
		cl::Buffer timeresCLBuffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, npixels*sizeof(unsigned short), timeres.data(), NULL);

		koWaveextract.setArg(0, waveCLBuffer);
		koWaveextract.setArg(1, npixels);
		koWaveextract.setArg(2, nsamples);
		koWaveextract.setArg(3, 6);
		koWaveextract.setArg(4, maxresCLBuffer);
		koWaveextract.setArg(5, timeresCLBuffer);

		queue.enqueueMapBuffer(waveCLBuffer, CL_FALSE, CL_MAP_WRITE, 0, buffSize);
		queue.enqueueMapBuffer(maxresCLBuffer, CL_FALSE, CL_MAP_READ, 0, npixels);
		queue.enqueueMapBuffer(timeresCLBuffer, CL_FALSE, CL_MAP_READ, 0, npixels);

		cl::NDRange global(npixels);
		cl::NDRange local(1);
		queue.enqueueNDRangeKernel(koWaveextract, cl::NullRange, global, local);
		queue.finish();

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
	}

	end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed = end-start;
	double throughput = event_count / elapsed.count();
	double mbytes = (event_size / 1000000) / elapsed.count();
	std::cout << event_count << " events sent in " << elapsed.count() << " s" << std::endl;
	std::cout << "mean event size: " << event_size / event_count << " B" << std::endl;
	std::cout << "throughput: " << throughput << " event/s = " << mbytes << " MB/s" << std::endl;

	return 0;
}
