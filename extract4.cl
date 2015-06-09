/*                   P1                                 P2
     ---------------------------------  ---------------------------------
     | 1 | 3 | 4 | 5 | 6 | 2 | 3 | 5 |  | 6 | 1 | 3 | 2 | 3 | 8 | 2 | 4 |
     ---------------------------------  ---------------------------------
        gid=0                               gid=8
     <----------->                      <----------->
             gid=1                              gid=9
         <----------->                      <----------->
                 gid=2                              gid=10
             <----------->                      <----------->
                     gid=3                              gid=11
                  <---------->                      <----------->
                         gid=4                              gid=12
                     <----------->                      <----------->
                             gid=5                              gid=13
                         <----------->                      <----------->
    skip gid = 6, 7, 14 and 15

    n samples = 8
    window size = 3
    n pixels = 2
 */
__constant uint windowSize = 6;

__kernel void sum(__global const ushort* restrict inBuf,
                  __global ushort* restrict sumBuf,
                  uint nPixels,
                  uint nSamples) {

    size_t pixelid = get_global_id(0);
    size_t sampleid = get_global_id(1);

    if(sampleid > nSamples-windowSize)
        return;

    ushort sum = 0;
    for(uint i=0; i<windowSize; i++) {
        sum += inBuf[pixelid*nSamples+sampleid+i];
    }
    sumBuf[pixelid*nSamples+sampleid] = sum;
}

__kernel void maximum(__global ushort* restrict sumBuf,
                      __global ushort* restrict maxBuf,
                      __global ushort* restrict offBuf,
                      uint nSamples) {

    uint pixel = get_global_id(0);

    ushort maxv = 0;
    ushort off = 0;

    for(uint i=0; i<nSamples-windowSize; i++) {
        ushort v = sumBuf[pixel*nSamples+i];
        if(v > maxv) {
            maxv = v;
            off = i;
        }
    }
    maxBuf[pixel] = maxv;
    offBuf[pixel] = off;
}
