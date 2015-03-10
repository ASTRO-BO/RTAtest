#include <CTACameraTriggerData1.h>
#include <CTAStream.h>
#include <CTADecoder.h>
#include <packet/PacketBufferV.h>
#include <iomanip>
#include <omp.h>

#include <ctautils/mac_clock_gettime.h>

//#define PRINTALG 1

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

	RTATelem::CTAStream stream(ctarta + "/share/rtatelem/rta_fadc1.stream", argv[1], "");
	RTATelem::CTADecoder decoder(ctarta + "/share/rtatelem/rta_fadc1.stream");

	PacketLib::PacketBufferV buff(ctarta + configFileName, argv[1]);
	buff.load();
	int npackets = buff.size();
	std::cout << "Loaded " << npackets << " packets " << std::endl;

	unsigned long totbytes = 0;
	byte** buffermemory = new byte*[npackets];
	for(int i=0; i<npackets; i++)
	{
		std::cout << "i=" << i << std::endl;

		ByteStreamPtr rawPacket = buff.getNext();
		RTATelem::CTAPacket& packet = decoder.getPacket(rawPacket);

		dword size = decoder.getInputPacketDimension(rawPacket);

		RTATelem::CTACameraTriggerData1& trtel = (RTATelem::CTACameraTriggerData1&) packet;
		totbytes += size;

		//access to blocks using only ByteStream
		ByteStreamPtr camera = trtel.getCameraDataSlow();
		std::cout << "camera size: " << camera->size() << std::endl;
		buffermemory[i] = new byte[2000*50*sizeof(word)];
		memcpy(buffermemory[i], camera->stream, camera->size());
	}

	int ntimes = 100;

	clock_gettime( CLOCK_MONOTONIC, &start);
	for(int n=0; n<ntimes; n++)
	{
		#pragma omp parallel for
		for(int npacket=0; npacket < npackets; npacket++)
		{
			int npixels = 1141;
			int nsamples = 40;
			unsigned short* maxres = new unsigned short[npixels];
			unsigned short* timeres = new unsigned short[npixels];
			calcWaveformExtraction4(buffermemory[npacket], npixels, nsamples, 6, maxres, timeres);
			delete[] maxres;
			delete[] timeres;
		}
	}
	clock_gettime( CLOCK_MONOTONIC, &stop);

	double time = timediff(start, stop);

	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << (totbytes / 1000000 * ntimes) / time << " MiB/s" << std::endl;

	}
    catch(PacketException* e)
    {
        cout << e->geterror() << endl;
    }

	return 0;
}
