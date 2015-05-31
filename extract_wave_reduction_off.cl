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

const unsigned int windowSize = 6;

__kernel void sum(__global const unsigned short* sampleBuf,
                         __global unsigned short* sumBuf,
                         unsigned int nSamples) {

    unsigned int gid = get_global_id(0);
    unsigned int sample = gid % nSamples;

    if(sample > nSamples-windowSize)
        return;

    unsigned int pixel = gid / nSamples;

    #pragma loop unroll
    unsigned short sum = 0;
    for(unsigned int i=0; i<windowSize; i++) {
        sum += inBuf[gid+i];
    }
    sumBuf[gid] = sum;
}

/* reduction opencl call it recusively

0  1  2  3  4  5    8  9 10 11 12 13
 \/    \/    \/      \/   \/    \/      n = 1
 0     2     4       8    10    12
  \   /      |        \   /     |       n = 2
    0        4          8      12
      \    /              \   /         n = 4
        0                   8
            \            /              n = 8
                   0
*/
__kernel void maximumWithOff(__global unsigned short* sumBuf,
                             __global unsigned int* off,
                             __constant unsigned int n) {

    unsigned int gid = get_global_id(0);
    unsigned int sample = gid / n;

    if(sample % iteration != 0)
        return;

    if(sample > nSamples-windowSize)
        return;

    unsigned short lvalue = sumBuf[gid];
    unsigned short rvalue = sumBuf[gid+n];
    if(rvalue > lvalue) {
        sumBuf[gid] = rvalue;
        off[gid] = gid+n;
    }
    else {
        off[gid] = gid;
    }
}
