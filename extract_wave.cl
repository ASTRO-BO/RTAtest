/* Kernel for waveform extraction */

__constant int windowSize = 6;

__kernel void waveExtract(__global const unsigned short* restrict inBuf,
                          __global unsigned short* restrict maxBuf,
                          __global float* restrict timeBuf,
                          unsigned int nSamples) {

    int gid = get_global_id(0);
    int pixelOff = gid*nSamples;

    unsigned short sumn = 0;
    unsigned short sum = 0;

    for(unsigned int winIdx=0; winIdx<windowSize; winIdx++) {
        sum += inBuf[pixelOff + winIdx];
        sumn += inBuf[pixelOff + winIdx] * winIdx;
    }

    unsigned short maxv = sum;
    float maxt = sumn / (float)sum;

    for(unsigned int sampleIdx=1; sampleIdx<nSamples-windowSize; sampleIdx++) {
        unsigned int prev = sampleIdx-1;
        unsigned int succ = sampleIdx+windowSize-1;
        sum = sum - inBuf[pixelOff+prev] + inBuf[pixelOff+succ];
        sumn = sumn - inBuf[pixelOff+prev] * prev + inBuf[pixelOff+succ] * succ;
        if(sum > maxv) {
            maxv = sum;
            maxt = (sumn / (float)sum);
        }
    }

    maxBuf[gid] = maxv;
    timeBuf[gid] = maxt;
}
