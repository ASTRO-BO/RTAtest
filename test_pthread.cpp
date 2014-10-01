#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <vector>
#include <pthread.h>
#include "mac_clock_gettime.h"
#include <zlib.h>

//#define DEBUG 1
#define PRINT_COMPRESS_SIZE 1
//#define PRINTALG 1

using namespace PacketLib;

const static int NTHREADS = 4;
const static int NTIMES = 1000;
const static int MULTIPLIER = 1;
const static int COMPRESSION_LEVEL = 1;

// shared variables
pthread_mutex_t lockp;
PacketStream* ps;
unsigned long int totbytes = 0;
unsigned long int totbytescomp = 0;
unsigned long int totbytesdecomp = 0;
std::vector<ByteStreamPtr> sharedBuffer;
int sharedIndex = 0;

void calcWaveformExtraction(byte* buffer, int npixels, int nsamples, int ws, unsigned short* maxresext, unsigned short* timeresext) {
	word *b = (word*) buffer; //should be pedestal subtractred

	unsigned short* maxres  = new unsigned short[npixels];
	unsigned short* timeres = new unsigned short[npixels];

#ifdef PRINTALG
	std::cout << "npixels = " << npixels << std::endl;
	std::cout << "nsamples = " << nsamples << std::endl;
#endif

	for(int pixel = 0; pixel<npixels; pixel++) {
		word* s = b + pixel * nsamples;

#ifdef PRINTALG
		cout << "pixel " << pixel << " samples ";
		for(int k=0; k<nsamples; k++)
		cout << s[k] << " ";
		cout << endl;
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
		cout << "result " << maxt << " " << maxres[pixel] << " " << timeres[pixel] << " " << endl;
#endif
	}

	memcpy(maxresext, maxres, sizeof(unsigned short) * npixels);
	memcpy(timeresext, timeres, sizeof(unsigned short) * npixels);
}

void* extractWave(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;
	unsigned short* maxres = new unsigned short[3000];
	unsigned short* timeres = new unsigned short[3000];
	int npix_idx = 0;
	int nsamp_idx = 0;

	pthread_mutex_lock(&lockp);
	try {
		Packet* p = ps->getPacketType("triggered_telescope1_30GEN");
		npix_idx = p->getPacketSourceDataField()->getFieldIndex("Number of pixels");
		nsamp_idx = p->getPacketSourceDataField()->getFieldIndex("Number of samples");
	} catch (PacketException* e)
	{
		cout << "Error during encoding: ";
		cout << e->geterror() << endl;
	}

	pthread_mutex_unlock(&lockp);

	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lockp);
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif
		ByteStreamPtr data = p->getData();
		byte* rawdata = data->getStream();
		int npix = p->getPacketSourceDataField()->getFieldValue(npix_idx);
		int nsamp = p->getPacketSourceDataField()->getFieldValue(nsamp_idx);
		totbytes += data->size();

#ifdef DEBUG
		std::cout << "npixels " << npix << std::endl;
		std::cout << "nsamples " << nsamp << std::endl;
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
			calcWaveformExtraction(rawdata, npix, nsamp, 6, maxres, timeres);
	}

	return 0;
}

void* compressLZ4(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;

	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lockp);
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif
		ByteStreamPtr data = p->getData();
		totbytes += data->size();

#ifdef DEBUG
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
		{
			ByteStreamPtr datacomp = data->compress(LZ4, COMPRESSION_LEVEL);
#ifdef PRINT_COMPRESS_SIZE
			if(i == 0)
			{
				pthread_mutex_lock(&lockp);
				totbytescomp += datacomp->size();
				pthread_mutex_unlock(&lockp);
			}
#endif
		}
	}

	return 0;
}

std::vector<ByteStreamPtr> createLZ4Buffer(PacketBufferV* buff)
{
	std::vector<ByteStreamPtr> compbuff;

	for(int i=0; i<buff->size(); i++)
	{
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif
		ByteStreamPtr data = p->getData();
		ByteStreamPtr datacomp = data->compress(LZ4, COMPRESSION_LEVEL);
		compbuff.push_back(datacomp);
	}

	return compbuff;
}

void* decompressLZ4(void* buffin)
{
	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lockp);
		ByteStreamPtr data = sharedBuffer[sharedIndex];
		sharedIndex = (sharedIndex+1) % sharedBuffer.size();
		totbytes += data->size();
#ifdef DEBUG
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
		{
			ByteStreamPtr datadecomp = data->decompress(LZ4, COMPRESSION_LEVEL, 400000);
#ifdef PRINT_COMPRESS_SIZE
			if(i == 0)
			{
				pthread_mutex_lock(&lockp);
				totbytesdecomp += datadecomp->size();
				pthread_mutex_unlock(&lockp);
			}
#endif
		}
	}

	return 0;
}

std::vector<ByteStreamPtr> createZlibBuffer(PacketBufferV* buff)
{
	std::vector<ByteStreamPtr> compbuff;
	for(int i=0; i<buff->size(); i++)
	{
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif
		ByteStreamPtr data = p->getData();

		z_stream defstream;
		defstream.zalloc = Z_NULL;
		defstream.zfree = Z_NULL;
		defstream.opaque = Z_NULL;
		defstream.avail_in = (uInt)data->size();
		defstream.next_in = (Bytef *)data->getStream();
		const size_t SIZEBUF = 400000;
		defstream.avail_out = (uInt) SIZEBUF;
		byte* outbuff = new byte[SIZEBUF];
		defstream.next_out = (Bytef *)outbuff;

		deflateInit(&defstream, COMPRESSION_LEVEL);
		deflate(&defstream, Z_FINISH);
		deflateEnd(&defstream);

		size_t compSize = ((unsigned char*) defstream.next_out - outbuff);
		ByteStreamPtr out = ByteStreamPtr(new ByteStream(outbuff, compSize, data->isBigendian()));
		compbuff.push_back(out);
	}

	return compbuff;
}

void* compressZlib(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;

	const size_t SIZEBUFF = 400000;
	unsigned char outbuff[SIZEBUFF];

	z_stream defstream;
	defstream.zalloc = Z_NULL;
	defstream.zfree = Z_NULL;
	defstream.opaque = Z_NULL;

	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lockp);
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif
		ByteStreamPtr data = p->getData();
		totbytes += data->size();
#ifdef DEBUG
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
		{
			defstream.avail_in = (uInt)data->size();
			defstream.next_in = (Bytef *)data->getStream();
			defstream.avail_out = (uInt) SIZEBUFF;
			defstream.next_out = (Bytef *)outbuff;
			deflateInit(&defstream, COMPRESSION_LEVEL);
			deflate(&defstream, Z_FINISH);
			deflateEnd(&defstream);
#ifdef PRINT_COMPRESS_SIZE
			if(i == 0)
			{
				pthread_mutex_lock(&lockp);
				totbytescomp += ((unsigned char*) defstream.next_out - outbuff);
				pthread_mutex_unlock(&lockp);
			}
#endif
		}
	}

	return 0;
}

void* decompressZlib(void* buffin)
{
	const size_t SIZEBUFF = 400000;
	unsigned char outbuff[SIZEBUFF];

	z_stream infstream;
	infstream.zalloc = Z_NULL;
	infstream.zfree = Z_NULL;
	infstream.opaque = Z_NULL;

	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lockp);
		ByteStreamPtr data = sharedBuffer[sharedIndex];
		sharedIndex = (sharedIndex+1) % sharedBuffer.size();
		totbytes += data->size();
#ifdef DEBUG
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
		{
			infstream.avail_in = (uInt)data->size();
			infstream.next_in = (Bytef *)data->getStream();
			infstream.avail_out = (uInt)SIZEBUFF;
			infstream.next_out = (Bytef *)outbuff;

			inflateInit(&infstream);
			inflate(&infstream, Z_NO_FLUSH);
			inflateEnd(&infstream);
#ifdef PRINT_COMPRESS_SIZE
			if(i == 0)
			{
				pthread_mutex_lock(&lockp);
				totbytesdecomp += ((unsigned char*) infstream.next_out - outbuff);
				pthread_mutex_unlock(&lockp);
			}
#endif
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct timespec start, stop;
	string configFileName = "rta_fadc_all.xml";
	string ctarta;

	if(argc <= 2) {
		std::cerr << "ERROR: Please, provide the .raw and algorithm (waveextract, compresslz4, decompresslz4, compressZlib or decompressZlib)." << std::endl;
		return 0;
	}

	// parse input algorithm
	void* (*alg)(void*);
	std::string algorithmStr(argv[2]);
	if(algorithmStr.compare("waveextract") == 0)
		alg = &extractWave;
	else if(algorithmStr.compare("compresslz4") == 0)
		alg = &compressLZ4;
	else if(algorithmStr.compare("decompresslz4") == 0)
		alg = &decompressLZ4;
	else if(algorithmStr.compare("compressZlib") == 0)
		alg = &compressZlib;
	else if(algorithmStr.compare("decompressZlib") == 0)
		alg = &decompressZlib;
	else
	{
		std::cerr << "Wrong algorithm: " << argv[2] << std::endl;
		std::cerr << "Please, provide the .raw and algorithm (waveextract, compresslz4, decompresslz4, compressZlib or decompressZlib)." << std::endl;
		return 0;
	}
	std::cout << "Using algorithm: " << algorithmStr << std::endl;

	PacketBufferV buff(configFileName, argv[1]);
	buff.load();
	int npackets = buff.size();
	std::cout << "Loaded " << npackets << " packets " << std::endl;

	// init shared variables
	ps = new PacketStream(configFileName.c_str());
    if(pthread_mutex_init(&lockp, NULL) != 0)
    {
        std::cout << "Mutex init failed" << std::endl;
        return 0;
    }

	std::cout << "Number of threads: " << NTHREADS << std::endl;
	std::cout << "Number of packet per threads: " << NTIMES << std::endl;
	std::cout << "Packet size multiplier: " << MULTIPLIER << std::endl;
	std::cout << "Compression level: " << COMPRESSION_LEVEL << std::endl;

	string compType = "";
	// if we are testing decompress we create the buffers for testing
	if(algorithmStr.compare("decompresslz4") == 0)
	{
		sharedBuffer = createLZ4Buffer(&buff);
		compType = "lz4";
	}
	else if(algorithmStr.compare("decompressZlib") == 0)
	{
		sharedBuffer = createZlibBuffer(&buff);
		compType = "zlib";
	}

	if(compType.compare(""))
		std::cout << "Loaded " << sharedBuffer.size() << " elements into " << compType << " buffer" << std::endl;

	// launch pthreads and timer
	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_t tid[NTHREADS];
	for(int i=0; i<NTHREADS; i++)
	{
		int err = pthread_create(&(tid[i]), NULL, alg, &buff);
        if (err != 0)
			std::cout << "Cannot create thread :[" << strerror(err) << "] " << std::endl;
	}
	for(int i=0; i<NTHREADS; i++)
		pthread_join(tid[i], NULL);
	clock_gettime(CLOCK_MONOTONIC, &stop);

	// get results
	double time = timediff(start, stop);
	float sizeMB = (totbytes * MULTIPLIER / 1000000.0);
	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << sizeMB / time << " MB/s" << std::endl;
	std::cout << "Input buffer size: " << setprecision(10) << sizeMB << std::endl;

#ifdef PRINT_COMPRESS_SIZE
	if(algorithmStr.compare("compresslz4") == 0 || algorithmStr.compare("compressZlib") == 0)
	{
		float sizecompMB = (totbytescomp * MULTIPLIER / 1000000.0);
		std::cout << "Output buffer size: " << setprecision(10) << sizecompMB << std::endl;
		std::cout << "Compression ratio: " << sizeMB / sizecompMB << std::endl;
	}
	else if(algorithmStr.compare("decompresslz4") == 0 || algorithmStr.compare("decompressZlib") == 0)
	{
		float sizedecompMB = (totbytesdecomp * MULTIPLIER / 1000000.0);
		std::cout << "Output buffer size: " << setprecision(10) << sizedecompMB << std::endl;
		std::cout << "Compression ratio: " << sizedecompMB / sizeMB << std::endl;
	}
#endif

	// destroy shared variables
	delete ps;
    pthread_mutex_destroy(&lockp);

	return 1;
}
