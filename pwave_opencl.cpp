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

#define PRINTALG 1

//#include <util.hpp>

/*#define PRINTALG 1

void calcWaveformExtraction4(PacketLib::byte* buffer, int npixels, int nsamples, int ws, unsigned short* maxresext, unsigned short* timeresext) {
	PacketLib::word *b = (PacketLib::word*) buffer; //should be pedestal subtractred

	unsigned short* maxres  = new unsigned short[npixels];
	unsigned short* timeres = new unsigned short[npixels];

#ifdef PRINTALG
	std::cout << "npixels = " << npixels << std::endl;
	std::cout << "nsamples = " << nsamples << std::endl;
#endif

	for(int pixel = 0; pixel<npixels; pixel++) {
		PacketLib::word* s = b + pixel * nsamples;

#ifdef PRINTALG
		std::cout << "pixel " << pixel << " samples ";
		for(int k=0; k<nsamples; k++)
		std::cout << s[k] << " ";
		std::cout << std::endl;
#endif

		unsigned short max = 0;
		double maxt = 0;
		long sumn = 0;
		long sumd = 0;
		long maxj = 0;
		double t = 0;

		for(int j=0; j<=ws-1; j++) {
			sumn += s[j] * j;
			sumd += s[j];
		}

		max = sumd;
		if(sumd != 0)
			t = sumn / (double)sumd;
		maxt = t;
		maxj = 0;

		for(int j=1; j<nsamples-ws; j++) {
			sumn = sumn - s[j-1] * (j-1) + s[j+ws-1] * (j+ws-1);
			sumd = sumd - s[j-1] + s[j+ws-1];

			if(sumd > max) {
				max = sumd;
				if(sumd != 0)
					t = sumn / (double)sumd;
				maxt = t;
				maxj = j;
			}
		}

		maxres[pixel] = max;
		timeres[pixel] =  maxt;

#ifdef PRINTALG
		std::cout << "result " << maxt << " " << maxres[pixel] << " " << timeres[pixel] << " " << std::endl;
#endif
	}

	memcpy(maxresext, maxres, sizeof(unsigned short) * npixels);
	memcpy(timeresext, timeres, sizeof(unsigned short) * npixels);
}*/

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

const char *err_code (cl_int err_in)
{
	switch (err_in) {
		case CL_SUCCESS:
			return (char*)"CL_SUCCESS";
		case CL_DEVICE_NOT_FOUND:
			return (char*)"CL_DEVICE_NOT_FOUND";
		case CL_DEVICE_NOT_AVAILABLE:
			return (char*)"CL_DEVICE_NOT_AVAILABLE";
		case CL_COMPILER_NOT_AVAILABLE:
			return (char*)"CL_COMPILER_NOT_AVAILABLE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:
			return (char*)"CL_MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_OUT_OF_RESOURCES:
			return (char*)"CL_OUT_OF_RESOURCES";
		case CL_OUT_OF_HOST_MEMORY:
			return (char*)"CL_OUT_OF_HOST_MEMORY";
		case CL_PROFILING_INFO_NOT_AVAILABLE:
			return (char*)"CL_PROFILING_INFO_NOT_AVAILABLE";
		case CL_MEM_COPY_OVERLAP:
			return (char*)"CL_MEM_COPY_OVERLAP";
		case CL_IMAGE_FORMAT_MISMATCH:
			return (char*)"CL_IMAGE_FORMAT_MISMATCH";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:
			return (char*)"CL_IMAGE_FORMAT_NOT_SUPPORTED";
		case CL_BUILD_PROGRAM_FAILURE:
			return (char*)"CL_BUILD_PROGRAM_FAILURE";
		case CL_MAP_FAILURE:
			return (char*)"CL_MAP_FAILURE";
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:
			return (char*)"CL_MISALIGNED_SUB_BUFFER_OFFSET";
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
			return (char*)"CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
		case CL_INVALID_VALUE:
			return (char*)"CL_INVALID_VALUE";
		case CL_INVALID_DEVICE_TYPE:
			return (char*)"CL_INVALID_DEVICE_TYPE";
		case CL_INVALID_PLATFORM:
			return (char*)"CL_INVALID_PLATFORM";
		case CL_INVALID_DEVICE:
			return (char*)"CL_INVALID_DEVICE";
		case CL_INVALID_CONTEXT:
			return (char*)"CL_INVALID_CONTEXT";
		case CL_INVALID_QUEUE_PROPERTIES:
			return (char*)"CL_INVALID_QUEUE_PROPERTIES";
		case CL_INVALID_COMMAND_QUEUE:
			return (char*)"CL_INVALID_COMMAND_QUEUE";
		case CL_INVALID_HOST_PTR:
			return (char*)"CL_INVALID_HOST_PTR";
		case CL_INVALID_MEM_OBJECT:
			return (char*)"CL_INVALID_MEM_OBJECT";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
			return (char*)"CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case CL_INVALID_IMAGE_SIZE:
			return (char*)"CL_INVALID_IMAGE_SIZE";
		case CL_INVALID_SAMPLER:
			return (char*)"CL_INVALID_SAMPLER";
		case CL_INVALID_BINARY:
			return (char*)"CL_INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS:
			return (char*)"CL_INVALID_BUILD_OPTIONS";
		case CL_INVALID_PROGRAM:
			return (char*)"CL_INVALID_PROGRAM";
		case CL_INVALID_PROGRAM_EXECUTABLE:
			return (char*)"CL_INVALID_PROGRAM_EXECUTABLE";
		case CL_INVALID_KERNEL_NAME:
			return (char*)"CL_INVALID_KERNEL_NAME";
		case CL_INVALID_KERNEL_DEFINITION:
			return (char*)"CL_INVALID_KERNEL_DEFINITION";
		case CL_INVALID_KERNEL:
			return (char*)"CL_INVALID_KERNEL";
		case CL_INVALID_ARG_INDEX:
			return (char*)"CL_INVALID_ARG_INDEX";
		case CL_INVALID_ARG_VALUE:
			return (char*)"CL_INVALID_ARG_VALUE";
		case CL_INVALID_ARG_SIZE:
			return (char*)"CL_INVALID_ARG_SIZE";
		case CL_INVALID_KERNEL_ARGS:
			return (char*)"CL_INVALID_KERNEL_ARGS";
		case CL_INVALID_WORK_DIMENSION:
			return (char*)"CL_INVALID_WORK_DIMENSION";
		case CL_INVALID_WORK_GROUP_SIZE:
			return (char*)"CL_INVALID_WORK_GROUP_SIZE";
		case CL_INVALID_WORK_ITEM_SIZE:
			return (char*)"CL_INVALID_WORK_ITEM_SIZE";
		case CL_INVALID_GLOBAL_OFFSET:
			return (char*)"CL_INVALID_GLOBAL_OFFSET";
		case CL_INVALID_EVENT_WAIT_LIST:
			return (char*)"CL_INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_EVENT:
			return (char*)"CL_INVALID_EVENT";
		case CL_INVALID_OPERATION:
			return (char*)"CL_INVALID_OPERATION";
		case CL_INVALID_GL_OBJECT:
			return (char*)"CL_INVALID_GL_OBJECT";
		case CL_INVALID_BUFFER_SIZE:
			return (char*)"CL_INVALID_BUFFER_SIZE";
		case CL_INVALID_MIP_LEVEL:
			return (char*)"CL_INVALID_MIP_LEVEL";
		case CL_INVALID_GLOBAL_WORK_SIZE:
			return (char*)"CL_INVALID_GLOBAL_WORK_SIZE";
		case CL_INVALID_PROPERTY:
			return (char*)"CL_INVALID_PROPERTY";
		default:
			return (char*)"UNKNOWN ERROR";
	}
}

int main(int argc, char *argv[])
{
	if(argc <= 3)
	{
		std::cout << "Wrong arguments, please provide config file, raw input file and number of packets." << std::endl;
		std::cout << argv[0] << " <configfile> <inputfile> <npackets>" << std::endl;
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
	unsigned int message_count = 0, message_size = 0;

	cl::Context context(CL_DEVICE_TYPE_DEFAULT);
	cl::CommandQueue queue(context);
	std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
	cl::Device device = devices[0];
	std::cout << "OpenCL infos: " << std::endl;
	std::string info;
	device.getInfo(CL_DEVICE_NAME, &info);
	std::cout << info << std::endl;
	device.getInfo(CL_DEVICE_OPENCL_C_VERSION, &info);
	std::cout << info << std::endl;
	device.getInfo(CL_DEVICE_VENDOR, &info);
	std::cout << info << std::endl;
	device.getInfo(CL_DEVICE_VERSION, &info);
	std::cout << info << std::endl;
	device.getInfo(CL_DRIVER_VERSION, &info);
	std::cout << info << std::endl;

	cl::Program program(context, loadProgram("extract_wave.cl"));

	program.build(devices, "-g -s extract_wave.cl");

	cl::Kernel koWaveextract(program, "waveextract");

	::size_t workgroupSize = koWaveextract.getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device);

	auto waveextract = cl::make_kernel<cl::Buffer, int, int, int, cl::Buffer, cl::Buffer>(program, "waveextract");

	start = std::chrono::system_clock::now();
	while(message_count++ < numevents)
	{
		PacketLib::ByteStreamPtr event = events.getNext();

		message_size += event->size();

		/// decoding packetlib packet
		PacketLib::ByteStreamPtr stream = PacketLib::ByteStreamPtr(new PacketLib::ByteStream(event->getStream(), event->size(), false));
		PacketLib::Packet *packet = ps.getPacket(stream);

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
		// compute
		std::cout << workgroupSize << std::endl;
		std::cout << npixels << std::endl;
		
		cl::Buffer waveCLBuffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, buffSize*sizeof(buff), buff, NULL);

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

		queue.enqueueMapBuffer(waveCLBuffer, CL_TRUE, CL_MAP_WRITE, 0, buffSize);
		queue.enqueueMapBuffer(maxresCLBuffer, CL_TRUE, CL_MAP_READ, 0, npixels);
		queue.enqueueMapBuffer(timeresCLBuffer, CL_TRUE, CL_MAP_READ, 0, npixels);

		cl::NDRange global(npixels);
		cl::NDRange local(1);
		queue.enqueueNDRangeKernel(koWaveextract, cl::NullRange, global, local);

#ifdef PRINTALG
		queue.finish();
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
	double msgs = message_count / elapsed.count();
	double mbits = ((message_size / 1000000) * 8) / elapsed.count();
	std::cout << message_count << "messages sent in " << elapsed.count() << " s" << std::endl;
	std::cout << "mean message size: " << message_size / message_count << std::endl;
	std::cout << "throughput: " << msgs << " msg/s = " << mbits << " mbit/s" << std::endl;

	return 0;
}
