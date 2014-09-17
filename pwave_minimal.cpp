#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <omp.h>

#include "mac_clock_gettime.h"

//#define DEBUG 1
//#define PRINTALG 1

using namespace PacketLib;

void calcWaveformExtraction4(byte* buffer, int npixels, int nsamples, int ws, unsigned short* maxresext, unsigned short* timeresext) {
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

int main(int argc, char *argv[])
{
	try {

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

	PacketStream ps(configFileName.c_str());
	std::vector<ByteStreamPtr> streams[3];
	int npixels[3];
	int nsamples[3];
	unsigned long totbytes[3];

	totbytes[0] = totbytes[1] = totbytes[2] = 0;
	for(int i=0; i<npackets; i++)
	{
		ByteStreamPtr rawPacket = buff.getNext();
		Packet *p = ps.getPacket(rawPacket);

		int id = p->getPacketID();
		if(id == 0)
			continue;

		npixels[id] = p->getPacketSourceDataField()->getFieldValue("Number of pixels");
		nsamples[id] = p->getPacketSourceDataField()->getFieldValue("Number of samples");
		streams[id].push_back(p->getData());
		totbytes[id] += p->getData()->size();

#ifdef DEBUG
		std::cout << "i " << i << std::endl;
		std::cout << "id " << id << std::endl;
		std::cout << "pixels " << npixels[id] << std::endl;
		std::cout << "samples " << nsamples[id] << std::endl;
		std::cout << "data size " << p->getData()->size() << std::endl;
#endif
	}

	const int ntimes = 100;
	const int ID = 1;

#ifdef DEBUG
	std::cout << "Using id " << ID << std::endl;
	std::cout << "ntimes " << ntimes << std::endl;
	std::cout << "npackets " << streams[ID].size() << std::endl;
	std::cout << "pixels " << npixels[ID] << std::endl;
	std::cout << "samples " << nsamples[ID] << std::endl;
	std::cout << "totbytes " << totbytes[ID] << std::endl;
#endif

	clock_gettime( CLOCK_MONOTONIC, &start);
	for(int n=0; n<ntimes; n++)
	{
		#pragma omp parallel for
		for(int npacket=0; npacket < streams[ID].size(); npacket++)
		{
			int npix = npixels[ID];
			int nsamp = nsamples[ID];
			unsigned short* maxres = new unsigned short[npix];
			unsigned short* timeres = new unsigned short[npix];
			calcWaveformExtraction4(streams[ID][npacket]->getStream(), npix, nsamp, 6, maxres, timeres);
			delete[] maxres;
			delete[] timeres;
		}
	}
	clock_gettime( CLOCK_MONOTONIC, &stop);

	double time = timediff(start, stop);

	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << (totbytes[ID] / 1000000 * ntimes) / time << " MiB/s" << std::endl;

	}
    catch(PacketException* e)
    {
        cout << e->geterror() << endl;
    }

	return 0;
}
