#include <packet/PacketBufferV.h>
#include <packet/PacketLibDefinition.h>
#include <iomanip>
#include <omp.h>
#include "mac_clock_gettime.h"

using namespace PacketLib;

//#define DEBUG 1

const int ntimes = 10;
const int ID = 1;

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
	std::vector<ByteStreamPtr> streamscomp[3];
	unsigned long totbytes[3];
	unsigned long totbytescomp[3];
	unsigned long totbytesdecomp[3];

	totbytes[0] = totbytes[1] = totbytes[2] = 0;
	totbytescomp[0] = totbytescomp[1] = totbytescomp[2] = 0;
	totbytesdecomp[0] = totbytesdecomp[1] = totbytesdecomp[2] = 0;
	for(int i=0; i<npackets; i++)
	{
		ByteStreamPtr rawPacket = buff.getNext();
		Packet *p = ps.getPacket(rawPacket);

		int id = p->getPacketID();
		if(id == 0)
			continue;

		streams[id].push_back(p->getData());
		totbytes[id] += p->getData()->size();

#ifdef DEBUG
		std::cout << "i " << i << std::endl;
		std::cout << "id " << id << std::endl;
		std::cout << "data size " << p->getData()->size() << std::endl;
#endif
	}

#ifdef DEBUG
	std::cout << "Using id " << ID << std::endl;
	std::cout << "ntimes " << ntimes << std::endl;
	std::cout << "npackets " << streams[ID].size() << std::endl;
	std::cout << "totbytes " << totbytes[ID] << std::endl;
#endif

	for(int npacket=0; npacket < streams[ID].size(); npacket++)
	{
		ByteStreamPtr comp = streams[ID][npacket]->compress(LZ4, 9);
		streamscomp[ID].push_back(comp);
		totbytescomp[ID] += comp->size();
	}

	clock_gettime( CLOCK_MONOTONIC, &start);
	#pragma omp parallel
	{
	for(int n=0; n<ntimes; n++)
	{
		unsigned long sum = 0;
		#pragma omp for nowait
		for(int npacket=0; npacket < streams[ID].size(); npacket++)
		{
			ByteStreamPtr decomp = streamscomp[ID][npacket]->decompress(LZ4, 9, 400000);
			sum += decomp->size();
		}
		#pragma omp critical
		totbytesdecomp[ID] += sum;
	}
	}
	clock_gettime( CLOCK_MONOTONIC, &stop);

	double time = timediff(start, stop);

	std::cout << "Result: it took  " << time << " s" << std::endl;
	std::cout << "Result: rate: " << setprecision(10) << (totbytes[ID] / 1000000 * ntimes) / time << " MiB/s" << std::endl;
	std::cout << "Tot: " << setprecision(5) << totbytes[ID] / 1000000.0 * ntimes << " MB" << std::endl;
	std::cout << "Tot compressed: " << setprecision(5) << totbytescomp[ID] / 1000000.0 << " MB" << std::endl;
	std::cout << "Tot decompressed: " << setprecision(5) << totbytesdecomp[ID] / 1000000.0 << " MB" << std::endl;
	}
    catch(PacketException* e)
    {
		cout << "Packet exception: " << e->geterror() << endl;
    }

	return 0;
}
