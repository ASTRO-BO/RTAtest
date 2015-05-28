/* Kernel for waveform extraction */

void waveExtract(__global const unsigned short* inBuf,
                 __global unsigned short* maxBuf,
                 __global unsigned short* timeBuf,
                 unsigned int nPixels,
                 unsigned int nSamples,
                 unsigned int windowSize) {

    int gid = get_global_id(0);
    int pixelOff = gid*nSamples;

    unsigned short sumn = 0;
    unsigned short sum = 1;

    for(unsigned int winIdx=0; winIdx<windowSize; winIdx++) {
        sum += inBuf[pixelOff + winIdx];
        sumn += inBuf[pixelOff + winIdx] * winIdx;
    }

    unsigned short maxv = sum;
    unsigned short maxt = sumn / (float)sum;

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
