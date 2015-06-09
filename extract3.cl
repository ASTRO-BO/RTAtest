/* Kernel for waveform extraction */

__constant uint windowSize = 6;

__kernel void waveExtract(__global const ushort* restrict inBuf,
                          __global ushort* restrict maxBuf,
                          __global float* restrict timeBuf,
                          uint nSamples) {

    const uint lid = get_local_id(1);
    const uint size = get_local_size(1);
    const uint groupid = get_group_id(1);
    const uint inid = get_global_id(2)*get_global_size(1)*get_global_size(0) +
                      get_global_id(0)*get_global_size(1) +
                      get_global_id(1);
    const uint outid = get_global_id(0);

    local ushort buf[100];
    local ushort s_partial[100];
    local ushort sn_partial[100];

    buf[lid] = inBuf[inid];

    barrier(CLK_LOCAL_MEM_FENCE);

    if(lid < size-windowSize) {
        ushort sum = 0;
        ushort sumn = 0;

        for(uint j=0; j<windowSize; j++) {
            sum += buf[lid+j];
            sumn += buf[lid+j] * (lid+j);
        }

        s_partial[lid] = sum;
        sn_partial[lid] = sumn;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    if(lid == 0) {
        uint maxv = 0;
        float maxt = 0;

        for(uint i=0; i<size-windowSize; i++) {
            if(s_partial[i] > maxv) {
                maxv = s_partial[i];
                maxt = (sn_partial[i] / (float)s_partial[i]);
            }
        }

        maxBuf[outid] = maxv;
        timeBuf[outid] = maxt;
    }
}
