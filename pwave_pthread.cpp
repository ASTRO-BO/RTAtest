#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <pthread.h>
#include "mac_clock_gettime.h"

//#define PRINTALG 1
//#define DEBUG 1

using namespace PacketLib;

const static int NTHREADS = 4;
const static int NTIMES = 10000;
const static int MULTIPLIER = 3;

// shared mutex
pthread_mutex_t lock;
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

void* threadFunc(void* buffin)
{
	PacketBufferV* buff = (PacketBufferV*) buffin;
	unsigned short* maxres = new unsigned short[3000];
	unsigned short* timeres = new unsigned short[3000];

	for(int n=0; n<NTIMES; n++)
	{
		pthread_mutex_lock(&lock);
		ByteStreamPtr rawPacket = buff->getNext();
		Packet *p = ps->getPacket(rawPacket);
		ByteStreamPtr data = p->getData();
		byte* rawdata = data->getStream();
		int npix = p->getPacketSourceDataField()->getFieldValue("Number of pixels");
		int nsamp = p->getPacketSourceDataField()->getFieldValue("Number of samples");
		totbytes += data->size();

#ifdef DEBUG
		std::cout << "npixels " << npix << std::endl;
		std::cout << "nsamples " << nsamp << std::endl;
		std::cout << "data size " << data->size() << std::endl;
		std::cout << "totbytes " << totbytes << std::endl;
#endif
		pthread_mutex_unlock(&lock);

		for(int i=0; i<MULTIPLIER; i++)
			calcWaveformExtraction(rawdata, npix, nsamp, 6, maxres, timeres);
	}
}

int main(int argc, char *argv[])
{
	struct timespec start, stop;
	string configFileName = "/share/rtatelem/rta_fadc_all.stream";
	string ctarta;

	if(argc > 1) {
		/// The Packet containing the FADC value of each triggered telescope
		const char* home = getenv("CTARTA");

		if (!home)
		{
			std::cerr << "ERROR: CTARTA environment variable is not defined." << std::endl;
			return 0;
		}

		ctarta = home;

	} else {
			cerr << "ERROR: Please, provide the .raw" << std::endl;
			return 0;
	}

	configFileName = ctarta + configFileName;

	PacketBufferV buff(configFileName, argv[1]);
	buff.load();
	int npackets = buff.size();
	std::cout << "Loaded " << npackets << " packets " << std::endl;

	// init shared variables
	ps = new PacketStream(configFileName.c_str());
    if(pthread_mutex_init(&lock, NULL) != 0)
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
        int err = pthread_create(&(tid[i]), NULL, &threadFunc, &buff);
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
    pthread_mutex_destroy(&lock);

	return 1;
}
