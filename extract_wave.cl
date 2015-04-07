/* Kernel for waveform extraction */

__kernel void waveextract(
	__global const unsigned short* restrict buffer,
	int npixels,
	int nsamples,
	int ws,
	__global unsigned short* restrict maxres,
	__global unsigned short* restrict timeres)
{
	int gid = get_global_id(0);

	long sumn = 0;
	long sumd = 0;

	// terribru slow s[j] access to global memory (false sharing) copy s to local buffer.
	//unsigned short* s = (unsigned short*) (buffer + gid * nsamples);

	int j;
	for(j=0; j<ws; j++) {
		sumn += (buffer+gid*nsamples)[j] * j;
		sumd += (buffer+gid*nsamples)[j];
	}

	unsigned short max = sumd;
	double t = 0;
	if(sumd != 0)
		t = sumn / (double)sumd;

	double maxt = t;
	long maxj = 0;

	for(j=1; j<nsamples-ws; j++) {
		sumn = sumn - (buffer+gid*nsamples)[j-1] * (j-1) + (buffer+gid*nsamples)[j+ws-1] * (j+ws-1);
		sumd = sumd - (buffer+gid*nsamples)[j-1] + (buffer+gid*nsamples)[j+ws-1];

		if(sumd > max) {
			max = sumd;
			if(sumd != 0)
				t = sumn / (double)sumd;
			maxt = t;
			maxj = j;
		}
	}

	maxres[gid] = max;
	timeres[gid] = maxt;
}
