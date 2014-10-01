#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <pthread.h>
#include "mac_clock_gettime.h"
#include <zlib.h>

//#define DEBUG 1
//#define PRINTALG 1

using namespace PacketLib;

const static int NTHREADS = 4;
const static int NTIMES = 1000;
const static int MULTIPLIER = 1;

// shared mutex
pthread_mutex_t lockp;
PacketStream* ps;
unsigned long int totbytes = 0;

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
			data->compress(LZ4, 9);
	}
}

void* decompressLZ4(void* buffin)
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
			data->decompress(LZ4, 9, 400000);
	}
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
		defstream.avail_in = (uInt)data->size();
		defstream.next_in = (Bytef *)data->getStream();
		defstream.avail_out = (uInt) SIZEBUFF;
		defstream.next_out = (Bytef *)outbuff;
#ifdef DEBUG
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lockp);

		for(int i=0; i<MULTIPLIER; i++)
		{
			deflateInit(&defstream, Z_DEFAULT_COMPRESSION);
			deflate(&defstream, Z_FINISH);
			deflateEnd(&defstream);
//			printf("Deflated size is: %lu\n", (char*)defstream.next_out - b);
		}
	}
}

int main(int argc, char *argv[])
{
	struct timespec start, stop;
	string configFileName = "rta_fadc_all.xml";
	string ctarta;

	if(argc <= 2) {
		std::cerr << "ERROR: Please, provide the .raw and algorithm (waveextract, compresslz4, decompresslz4 or compressZlib)." << std::endl;
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
	else
	{
		std::cerr << "Wrong algorithm: " << argv[2] << std::endl;
		std::cerr << "Please, provide the .raw and algorithm (waveextract, compresslz4, decompresslz4 or compressZlib)." << std::endl;
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
	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << (totbytes * MULTIPLIER / 1000000) / time << " MiB/s" << std::endl;

	// destroy shared variables
	delete ps;
    pthread_mutex_destroy(&lockp);

	return 1;
}
