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
__constant unsigned int windowSize = 6;

__kernel void sum(__global const unsigned short* restrict inBuf,
                  __global unsigned short* restrict sumBuf,
                  __const unsigned int nSamples) {

    unsigned int gid = get_global_id(0);
    unsigned int sample = gid % nSamples;

    if(sample > nSamples-windowSize)
        return;

    unsigned short sum = 0;
    for(unsigned int i=0; i<windowSize; i++) {
        sum += inBuf[gid+i];
    }
    sumBuf[gid] = sum;
}

__kernel void maximum(__global unsigned short* restrict sumBuf,
                      __global unsigned short* restrict maxBuf,
                      __global unsigned short* restrict offBuf,
                      __const unsigned int nSamples) {

    unsigned int pixel = get_global_id(0);

    unsigned short maxv = 0;
    unsigned short off = 0;

    for(unsigned int i=0; i<nSamples-windowSize; i++) {
        unsigned short v = sumBuf[pixel*nSamples+i];
        if(v > maxv) {
            maxv = v;
            off = i;
        }
    }
    maxBuf[pixel] = maxv;
    offBuf[pixel] = off;
}
