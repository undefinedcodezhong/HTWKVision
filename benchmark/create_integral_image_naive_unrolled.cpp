#include "create_integral_image_naive_unrolled.h"

#include "planar_yuv.h"
#include "create_integral_image_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <xmmintrin.h>

static const int iWidth=IMAGE_WIDTH/INTEGRAL_SCALE;
static const int iHeight=IMAGE_HEIGHT/INTEGRAL_SCALE;
extern int integralImg[IMAGE_WIDTH * IMAGE_HEIGHT];

static int8_t *lutCr;
static int8_t *lutCb;

void createIntegralImageNaiveUnrolledInit()
{
    lutCb=(int8_t*)malloc(sizeof(*lutCb)*IMAGE_WIDTH);
    lutCr=(int8_t*)malloc(sizeof(*lutCr)*IMAGE_WIDTH);
    for(int i=1;i<IMAGE_WIDTH;i++){
        if((i&1)==0){
            lutCb[i]=5;
            lutCr[i]=3;
        }else{
            lutCb[i]=3;
            lutCr[i]=5;
        }
        while(lutCr[i]+i*2>=IMAGE_WIDTH*2)
            lutCr[i]-=4;
        while(lutCb[i]+i*2>=IMAGE_WIDTH*2)
            lutCb[i]-=4;
    }
    lutCb[0]=1;
    lutCr[0]=3;
}

inline uint8_t getCr(const uint8_t * const img, int32_t x, int32_t y) {
    return img[((x + y * IMAGE_WIDTH) << 1) + lutCr[x]];
}

inline uint8_t getY(const uint8_t * const img, int32_t x, int32_t y) {
    return img[(x + y * IMAGE_WIDTH) << 1];
}

#define getValue getCr

void createIntegralImageNaiveUnrolled(uint8_t *img) {
    integralImg[0]=getValue(img,0,0);
    for(int x=1;x<iWidth;x++){
        integralImg[x]=integralImg[x-1]+(getValue(img,x*INTEGRAL_SCALE,0));
    }

    const int factor = 16;

    for(int y=1;y<iHeight;y++){
        int sum=0;
        for(int x=0;x<iWidth/factor;x++){
            const int addr0=x*factor+y*iWidth+0;
            const int addr1=x*factor+y*iWidth+4;
            const int addr2=x*factor+y*iWidth+8;
            const int addr3=x*factor+y*iWidth+12;

            const __m128i i0 = _mm_load_si128((__m128i*)&(integralImg[addr0-iWidth]));
            const __m128i i1 = _mm_load_si128((__m128i*)&(integralImg[addr1-iWidth]));
            const __m128i i2 = _mm_load_si128((__m128i*)&(integralImg[addr2-iWidth]));
            const __m128i i3 = _mm_load_si128((__m128i*)&(integralImg[addr3-iWidth]));

            const int cr0  = sum  + getValue(img,(x*factor+ 0)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr1  = cr0  + getValue(img,(x*factor+ 1)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr2  = cr1  + getValue(img,(x*factor+ 2)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr3  = cr2  + getValue(img,(x*factor+ 3)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr4  = cr3  + getValue(img,(x*factor+ 4)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr5  = cr4  + getValue(img,(x*factor+ 5)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr6  = cr5  + getValue(img,(x*factor+ 6)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr7  = cr6  + getValue(img,(x*factor+ 7)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr8  = cr7  + getValue(img,(x*factor+ 8)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr9  = cr8  + getValue(img,(x*factor+ 9)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr10 = cr9  + getValue(img,(x*factor+10)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr11 = cr10 + getValue(img,(x*factor+11)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr12 = cr11 + getValue(img,(x*factor+12)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr13 = cr12 + getValue(img,(x*factor+13)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr14 = cr13 + getValue(img,(x*factor+14)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            const int cr15 = cr14 + getValue(img,(x*factor+15)*INTEGRAL_SCALE,y*INTEGRAL_SCALE);
            sum = cr15;

            const __m128i iSum0 = _mm_setr_epi32( cr0,  cr1,  cr2,  cr3);
            const __m128i iSum1 = _mm_setr_epi32( cr4,  cr5,  cr6,  cr7);
            const __m128i iSum2 = _mm_setr_epi32( cr8,  cr9, cr10, cr11);
            const __m128i iSum3 = _mm_setr_epi32(cr12, cr13, cr14, cr15);

            const __m128i r0 = _mm_add_epi32(i0, iSum0);
            const __m128i r1 = _mm_add_epi32(i1, iSum1);
            const __m128i r2 = _mm_add_epi32(i2, iSum2);
            const __m128i r3 = _mm_add_epi32(i3, iSum3);

            _mm_storeu_si128((__m128i*)&integralImg[addr0], r0);
            _mm_storeu_si128((__m128i*)&integralImg[addr1], r1);
            _mm_storeu_si128((__m128i*)&integralImg[addr2], r2);
            _mm_storeu_si128((__m128i*)&integralImg[addr3], r3);
        }
    }
}
