/***************************************************************************
						CTA/RTA performance tests
 -------------------
 begin                : September 2014
 copyright            : (C) 2014 by Andrea Zoli, Andrea Bulgarelli
 email                : zoli@iasfbo.inaf.it, bulgarelli@iasfbo.inaf.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <vector>
#include <pthread.h>
#include <ctautils/mac_clock_gettime.h>
#include <zlib.h>

//#define DEBUG 1
//#define PRINTALG 1

using namespace PacketLib;

static int NTHREADS = 8;
static long NTIMES = 10000;
const static long PACKET_NUM = 1;
static int COMPRESSION_LEVEL = 1;

// shared variables
string configFileName;
pthread_mutex_t lockp;
PacketStream* ps;
//unsigned long int totbytes = 0;
unsigned long long totbytescomp = 0;
unsigned long long totbytesdecomp = 0;
double sizeMB = 0.0;

typedef struct dataBufferElementStr {
	ByteStreamPtr data;
	int npix;
	int nsamp;
} DataBufferElement;

//a buffer that contains the data and some infos about the data
std::vector<DataBufferElement> sharedDataAndInfoBuffer;
int sharedIndex = 0;

//a buffer that contains the data
std::vector<ByteStreamPtr> sharedDataBuffer;

std::vector<DataBufferElement> createBuffer(PacketBufferV* buff)
{
	int npix_idx = 0;
	int nsamp_idx = 0;
	std::vector<DataBufferElement> outBuff;
	try {
		Packet *p = ps->getPacketType("triggered_telescope1_30GEN");
		npix_idx = p->getPacketSourceDataField()->getFieldIndex("Number of pixels");
		nsamp_idx = p->getPacketSourceDataField()->getFieldIndex("Number of samples");
	} catch (PacketException* e)
	{
		cout << "Error during encoding: ";
		cout << e->geterror() << endl;
	}
	for(int i=0; i<buff->size(); i++)
	{
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
		// get npixel and nsamples

		int npix = p->getPacketSourceDataField()->getFieldValue(npix_idx);
		int nsamp = p->getPacketSourceDataField()->getFieldValue(nsamp_idx);

#ifdef DEBUG
		if(p->getPacketID() == 0) {
			std::cerr << "No packet type recognized" << std::endl;
			continue;
		}
#endif

		DataBufferElement elem;
		elem.data = p->getData();

#ifdef ARCH_BIGENDIAN
		if(!elem.data->isBigendian())
			elem.data->swapWord();
#else
		if(elem.data->isBigendian())
			elem.data->swapWord();
#endif
		elem.npix = npix;
		elem.nsamp = nsamp;
		outBuff.push_back(elem);
	}
	
	return outBuff;
}

//---------------------------------------------------------
// WAVEFORM EXTRACTION
//---------------------------------------------------------

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
	delete[] maxres;
	delete[] timeres;
}

void* extractWaveData(void* buffin)
{
	unsigned short* maxres = new unsigned short[3000];
	unsigned short* timeres = new unsigned short[3000];

	// load packet type (once per thread)

	ByteStreamPtr localBuffer[PACKET_NUM];
	int npix[PACKET_NUM];
	int nsamp[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = sharedDataAndInfoBuffer[sharedIndex].data;
			localBuffer[m] = data;
			npix[m] = sharedDataAndInfoBuffer[sharedIndex].npix;
			nsamp[m] = sharedDataAndInfoBuffer[sharedIndex].nsamp;
			
			sharedIndex = (sharedIndex+1) % sharedDataAndInfoBuffer.size();
			sizeMB += (data->size() / 1000000.0);
#ifdef DEBUG
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
			
		}
		pthread_mutex_unlock(&lockp);

		for(int m=0; m<PACKET_NUM; m++)
		{
			byte* rawdata = localBuffer[m]->getStream();
			rawdata[0] = rand() % 100 + 50;

#ifdef DEBUG
			std::cout << "npixels " << npix[m] << std::endl;
			std::cout << "nsamples " << nsamp[m] << std::endl;
			std::cout << "data size " << localBuffer[m]->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
			// extract waveform
			calcWaveformExtraction(rawdata, npix[m], nsamp[m], 6, maxres, timeres);
			maxres[0]++;
			timeres[0]++;
		}
	}

	return 0;
}

void* extractWavePacket(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;
	unsigned short* maxres = new unsigned short[3000];
	unsigned short* timeres = new unsigned short[3000];
	int npix_idx = 0;
	int nsamp_idx = 0;
	// load packet type (once per thread)
	PacketStream* ps;
	pthread_mutex_lock(&lockp);
	try {
		ps = new PacketStream(configFileName.c_str());
		Packet *p = ps->getPacketType("triggered_telescope1_30GEN");
		npix_idx = p->getPacketSourceDataField()->getFieldIndex("Number of pixels");
		nsamp_idx = p->getPacketSourceDataField()->getFieldIndex("Number of samples");
	} catch (PacketException* e)
	{
		cout << "Error during extractWavePacket: ";
		cout << e->geterror() << endl;
	}
	pthread_mutex_unlock(&lockp);
	
	ByteStreamPtr localBuffer[PACKET_NUM];
	int npix[PACKET_NUM];
	int nsamp[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = buff->getNext();
			sizeMB += (rawPacket->size() / 1000000.0);
			localBuffer[m] = rawPacket;
			
		}
		pthread_mutex_unlock(&lockp);
		
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = localBuffer[m];
			Packet *p = ps->getPacket(rawPacket);
			// get npixel and nsamples
			
			int npix = p->getPacketSourceDataField()->getFieldValue(npix_idx);
			int nsamp = p->getPacketSourceDataField()->getFieldValue(nsamp_idx);
			ByteStreamPtr data = p->getData();
#ifdef ARCH_BIGENDIAN
			if(!data->isBigendian())
				data->swapWord();
#else
			if(data->isBigendian())
				data->swapWord();
#endif
			byte* rawdata = data->getStream();
			rawdata[0] = rand() % 100 + 50;
#ifdef DEBUG
			std::cout << "npixels " << npix << std::endl;
			std::cout << "nsamples " << nsamp << std::endl;
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
			// extract waveform
			calcWaveformExtraction(rawdata, npix, nsamp, 6, maxres, timeres);
			maxres[0]++;
			timeres[0]++;
		}
	}
	delete ps;
	return 0;
}

void* routingPacket(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;
	int npix_idx = 0;
	int nsamp_idx = 0;
	// load packet type (once per thread)
	PacketStream* ps;
	pthread_mutex_lock(&lockp);
	try {
		ps = new PacketStream(configFileName.c_str());
	} catch (PacketException* e)
	{
		cout << "Error during routingPacket: ";
		cout << e->geterror() << endl;
	}
	pthread_mutex_unlock(&lockp);
	
	ByteStreamPtr localBuffer[PACKET_NUM];
	int npix[PACKET_NUM];
	int nsamp[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = buff->getNext();
			localBuffer[m] = rawPacket;
			
		}
		pthread_mutex_unlock(&lockp);
		
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = localBuffer[m];
			Packet *p = ps->getPacket(rawPacket);
			// get npixel and nsamples
			if(p->getPacketID() > 0) {
				sizeMB += (p->size() / 1000000.0);
				//do something with the packet
				;
			}
#ifdef DEBUG
			else {
				cout << "Warning: no packet recognized" << endl;
			}
#endif
			
#ifdef DEBUG
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
		}
	}
	delete ps;
	return 0;
}

void* decodePacket(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;
	int npix_idx = 0;
	int nsamp_idx = 0;
	// load packet type (once per thread)
	PacketStream* ps;
	pthread_mutex_lock(&lockp);
	try {
		ps = new PacketStream(configFileName.c_str());
		Packet *p = ps->getPacketType("triggered_telescope1_30GEN");
		npix_idx = p->getPacketSourceDataField()->getFieldIndex("Number of pixels");
		nsamp_idx = p->getPacketSourceDataField()->getFieldIndex("Number of samples");
	} catch (PacketException* e)
	{
		cout << "Error during extractWavePacket: ";
		cout << e->geterror() << endl;
	}
	pthread_mutex_unlock(&lockp);
	
	ByteStreamPtr localBuffer[PACKET_NUM];
	int npix[PACKET_NUM];
	int nsamp[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = buff->getNext();
			localBuffer[m] = rawPacket;
			
		}
		pthread_mutex_unlock(&lockp);
		
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr rawPacket = localBuffer[m];
			Packet *p = ps->getPacket(rawPacket);
			// get npixel and nsamples
			
			int npix = p->getPacketSourceDataField()->getFieldValue(npix_idx);
			int nsamp = p->getPacketSourceDataField()->getFieldValue(nsamp_idx);
			ByteStreamPtr data = p->getData();
#ifdef ARCH_BIGENDIAN
			if(!data->isBigendian())
				data->swapWord();
#else
			if(data->isBigendian())
				data->swapWord();
#endif
			sizeMB += (data->size() / 1000000.0);
			byte* rawdata = data->getStream();
			rawdata[0] = rand() % 100 + 50;
#ifdef DEBUG
			std::cout << "npixels " << npix << std::endl;
			std::cout << "nsamples " << nsamp << std::endl;
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
		}
	}
	delete ps;
	return 0;
}



//---------------------------------------------------------
// LZ4 compression
//---------------------------------------------------------

void* compressLZ4(void* buffin)
{
	ByteStreamPtr localBuffer[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = sharedDataAndInfoBuffer[sharedIndex].data;
			sharedIndex = (sharedIndex+1) % sharedDataAndInfoBuffer.size();
			sizeMB += (data->size() / 1000000.0);
#ifdef DEBUG
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "toto size (MB) " << sizeMB << std::endl;
#endif
			localBuffer[m] = data;
		}
		pthread_mutex_unlock(&lockp);

		int compbytes = 0;
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = localBuffer[m];

			// compress
			ByteStreamPtr datacomp = data->compress(LZ4, COMPRESSION_LEVEL);

			// update local byte counter
			compbytes += datacomp->size();
			
			delete datacomp->getStream(); //HACK
		}

		// update global byte counter
		pthread_mutex_lock(&lockp);
		totbytescomp += compbytes;
		pthread_mutex_unlock(&lockp);
	}

	return 0;
}

void* decompressLZ4(void* buffin)
{
	ByteStreamPtr localBuffer[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = sharedDataBuffer[sharedIndex];
			sharedIndex = (sharedIndex+1) % sharedDataBuffer.size();
			sizeMB += (data->size() / 1000000.0);
#ifdef DEBUG
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
			localBuffer[m] = data;
		}
		pthread_mutex_unlock(&lockp);

		int decompbytes = 0;
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = localBuffer[m];

			// decompress
			ByteStreamPtr datadecomp = data->decompress(LZ4, COMPRESSION_LEVEL, 400000);
			

			// update local byte counter
			decompbytes += datadecomp->size();
			
			
			
		}

		// update global byte counter
		pthread_mutex_lock(&lockp);
		totbytesdecomp += decompbytes;
		pthread_mutex_unlock(&lockp);
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

//---------------------------------------------------------
// zlib compression
//---------------------------------------------------------

void* compressZlib(void* buffin)
{
	const size_t SIZEBUFF = 400000;
	

	z_stream defstream;
	defstream.zalloc = Z_NULL;
	defstream.zfree = Z_NULL;
	defstream.opaque = Z_NULL;

	ByteStreamPtr localBuffer[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = sharedDataAndInfoBuffer[sharedIndex].data;
			sharedIndex = (sharedIndex+1) % sharedDataAndInfoBuffer.size();
			sizeMB += (data->size() / 1000000.0);
#ifdef DEBUG
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size (MB) " << sizeMB << std::endl;
#endif
			localBuffer[m] = data;
		}
		pthread_mutex_unlock(&lockp);

		int compbytes = 0;
		for(int m=0; m<PACKET_NUM; m++)
		{
			unsigned char outbuff[SIZEBUFF];
			ByteStreamPtr data = localBuffer[m];

			// compress
			defstream.avail_in = (uInt)data->size();
			defstream.next_in = (Bytef *)data->getStream();
			defstream.avail_out = (uInt) SIZEBUFF;
			defstream.next_out = (Bytef *)outbuff;
			deflateInit(&defstream, COMPRESSION_LEVEL);
			deflate(&defstream, Z_FINISH);
			deflateEnd(&defstream);

			// update local byte counter
			compbytes += ((unsigned char*) defstream.next_out - outbuff);
		}

		// update global byte counter
		pthread_mutex_lock(&lockp);
		totbytescomp += compbytes;
		pthread_mutex_unlock(&lockp);
	}

	return 0;
}

void* decompressZlib(void* buffin)
{
	const size_t SIZEBUFF = 400000;
	

	z_stream infstream;
	infstream.zalloc = Z_NULL;
	infstream.zfree = Z_NULL;
	infstream.opaque = Z_NULL;

	ByteStreamPtr localBuffer[PACKET_NUM];
	for(int n=0; n<NTIMES; n++)
	{
		// copy PACKET_NUM packets data locally
		pthread_mutex_lock(&lockp);
		for(int m=0; m<PACKET_NUM; m++)
		{
			ByteStreamPtr data = sharedDataBuffer[sharedIndex];
			sharedIndex = (sharedIndex+1) % sharedDataBuffer.size();
			sizeMB += (data->size() / 1000000.0);
#ifdef DEBUG
			std::cout << "data size " << data->size() << std::endl;
			std::cout << "tot size MB " << sizeMB << std::endl;
#endif
			localBuffer[m] = data;
		}
		pthread_mutex_unlock(&lockp);

		int decompbytes = 0;
		for(int m=0; m<PACKET_NUM; m++)
		{
			unsigned char outbuff[SIZEBUFF];
			ByteStreamPtr data = localBuffer[m];
			infstream.avail_in = (uInt)data->size();
			infstream.next_in = (Bytef *)data->getStream();
			infstream.avail_out = (uInt)SIZEBUFF;
			infstream.next_out = (Bytef *)outbuff;

			// decompress
			inflateInit(&infstream);
			inflate(&infstream, Z_NO_FLUSH);
			inflateEnd(&infstream);

			decompbytes += ((unsigned char*) infstream.next_out - outbuff);
		}

		// update decompressed byte counter
		pthread_mutex_lock(&lockp);
		totbytesdecomp += decompbytes;
		pthread_mutex_unlock(&lockp);
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

int main(int argc, char *argv[])
{
	cout << "Size of short: " << sizeof(short) << " - Size of int: " << sizeof(int) << " - Size of long: " <<  sizeof(long) << " - Size of long long: " << sizeof(long long) << endl;
	
	struct timespec start, stop;
	

	if(argc <= 4) {
		std::cerr << "ERROR: Please, provide the .raw, .stream, number of threads, algorithm (waveextractdata, waveextractpacket, decodepacket, routingpacket, compresslz4, decompresslz4, compressZlib or decompressZlib) [optional: Number of packet per threads,  Compression level]." << std::endl;
		std::cerr << "Example ./test_pthread $CTARTA/data/Aar.ABTEST.FADC.BIGTRUE.raw rta_fadc_all_BIGTRUE.stream 8 waveextractdata [10000 1]" << std::endl;
		return 0;
	}
	
	configFileName = argv[2];
	NTHREADS = atoi(argv[3]);

	// parse input algorithm
	void* (*alg)(void*);
	std::string algorithmStr(argv[4]);
	if(algorithmStr.compare("waveextractdata") == 0)
		alg = &extractWaveData;
	else if(algorithmStr.compare("waveextractpacket") == 0)
		alg = &extractWavePacket;
	else if(algorithmStr.compare("routingpacket") == 0)
		alg = &routingPacket;
	else if(algorithmStr.compare("decodepacket") == 0)
		alg = &decodePacket;
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
		std::cerr << "Please, provide the .raw and algorithm (waveextractdata, waveextractpacket, decodepacket, routingpacket, compresslz4, decompresslz4, compressZlib or decompressZlib)." << std::endl;
		return 0;
	}
	std::cout << "Using algorithm: " << algorithmStr << std::endl;
	
	if(argc >= 6) {
		NTIMES = atoi(argv[5]);
		COMPRESSION_LEVEL = atoi(argv[6]);
	
	}

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
	std::cout << "Packet number multiplier: " << PACKET_NUM << std::endl;
	std::cout << "Compression level: " << COMPRESSION_LEVEL << std::endl;

	string compType = "";
	// if we are testing decompress we create the buffers for testing
	if(algorithmStr.compare("decompresslz4") == 0)
	{
		sharedDataBuffer = createLZ4Buffer(&buff);
		compType = "lz4";
	}
	else if(algorithmStr.compare("decompressZlib") == 0)
	{
		sharedDataBuffer = createZlibBuffer(&buff);
		compType = "zlib";
	}
	else if(algorithmStr.compare("waveextractpacket") == 0 || algorithmStr.compare("routingpacket") == 0 || algorithmStr.compare("decodepacket") == 0)
	{
		;
	}
	else // otherwise a normal buffer
	{
		sharedDataAndInfoBuffer = createBuffer(&buff);
	}

	if(compType.compare(""))
		std::cout << "Loaded " << sharedDataBuffer.size() << " elements into " << compType << " buffer" << std::endl;

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
	
	std::cout << "Results -----" << endl;
	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << sizeMB / time << " MB/s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << NTHREADS * PACKET_NUM * NTIMES / time << " Hz" << std::endl;
	std::cout << "Input buffer size: " << setprecision(10) << sizeMB << " MB " << std::endl;

	if(algorithmStr.compare("compresslz4") == 0 || algorithmStr.compare("compressZlib") == 0)
	{
		float sizecompMB = (totbytescomp / 1000000.0);
		std::cout << "Output buffer size: " << setprecision(10) << sizecompMB << " MB " << std::endl;
		std::cout << "Compression ratio: " << 1.0 / (sizeMB / sizecompMB) << std::endl;
	}
	else if(algorithmStr.compare("decompresslz4") == 0 || algorithmStr.compare("decompressZlib") == 0)
	{
		float sizedecompMB = (totbytesdecomp / 1000000.0);
		std::cout << "Output buffer size: " << setprecision(10) << sizedecompMB << " MB " <<  std::endl;
		std::cout << "Compression ratio: " << 1.0 / (sizedecompMB / sizeMB) << std::endl;
	}

	// destroy shared variables
	delete ps;
    pthread_mutex_destroy(&lockp);

	return 1;
}
