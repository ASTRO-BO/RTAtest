/* Kernel for waveform extraction */

__constant uint windowSize = 6;

__kernel void waveExtract(__global const ushort* restrict inBuf,
                          __global ushort* restrict maxBuf,
                          __global float* restrict timeBuf,
                          uint nSamples) {

    size_t gid = get_global_id(0);
    size_t pixelOff = gid*nSamples;

    ushort sumn = 0;
    ushort sum = 0;

    for(uint winIdx=0; winIdx<windowSize; winIdx++) {
        sum += inBuf[pixelOff + winIdx];
        sumn += inBuf[pixelOff + winIdx] * winIdx;
    }

    ushort maxv = sum;
    float maxt = sumn / (float)sum;

    for(uint sampleIdx=1; sampleIdx<nSamples-windowSize; sampleIdx++) {
        uint prev = sampleIdx-1;
        uint succ = sampleIdx+windowSize-1;
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
