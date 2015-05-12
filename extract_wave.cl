/* Kernel for waveform extraction */
#pragma OPENCL EXTENSION cl_khr_fp64: enable

__kernel void waveextract(
	__global const unsigned short* restrict buffer,
	int npixels,
	int nsamples,
	int ws,
	__global unsigned short* restrict maxres,
	__global unsigned short* restrict timeres)
{
    int gid = get_global_id(0);
	unsigned int idx = gid*nsamples;

    // copy samples into private memory
    unsigned short samples[100];
    for(unsigned int i=0; i<nsamples; i++) {
        samples[i] = buffer[idx+i];
    }

	long sumn = 0;
	long sumd = 1;

	for(unsigned int j=0; j<ws; j++) {
		sumn += samples[j] * j;
		sumd += samples[j];
	}

	unsigned short max = sumd;
	double t = (double)sumn / (double)sumd;

	double maxt = t;

	for(unsigned int j=1; j<nsamples-ws; j++) {
	    unsigned int prev = j-1;
	    unsigned int succ = j+ws-1;
		sumn = sumn - samples[prev] * prev + samples[succ] * succ;
		sumd = sumd - samples[prev] + samples[succ];

		if(sumd > max) {
			max = sumd;
			t = (double) sumn / (double)sumd;
			maxt = t;
		}
	}

	maxres[gid] = max;
	timeres[gid] = maxt;
}
