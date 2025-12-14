/*
* Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
* SPDX-License-Identifier: MIT
*
* This file is licensed under the MIT License.
* See the LICENSE file in the project root for full license text.
*/

#include <string.h>
#include "core/hash.h"

typedef struct {
	uint32_t s[8]; uint64_t bits; uint8_t buf[64]; size_t n;
} shactx;

static uint32_t ROR(uint32_t x, int n){
	return (x>>n)|(x<<(32-n));
}

static uint32_t Ch(uint32_t x,uint32_t y,uint32_t z) {
	return (x & y) ^ (~x & z);
}

static uint32_t Maj(uint32_t x,uint32_t y,uint32_t z) {
	return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t S0(uint32_t x) {
	return ROR(x,2)^ROR(x,13)^ROR(x,22);
}

static uint32_t S1(uint32_t x) {
	return ROR(x,6)^ROR(x,11)^ROR(x,25);
}

static uint32_t s0(uint32_t x) {
	return ROR(x,7)^ROR(x,18)^(x>>3);
}

static uint32_t s1(uint32_t x) {
	return ROR(x,17)^ROR(x,19)^(x>>10);
}

static const uint32_t K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};

static void init(shactx* c) {
	c->s[0]=0x6a09e667; c->s[1]=0xbb67ae85; c->s[2]=0x3c6ef372; c->s[3]=0xa54ff53a;
	c->s[4]=0x510e527f; c->s[5]=0x9b05688c; c->s[6]=0x1f83d9ab; c->s[7]=0x5be0cd19;
	c->bits=0; c->n=0;
}

static void blk(shactx* c,const uint8_t b[64]) {
	uint32_t w[64],a,b0,c0,d,e,f,g,h;
	for (int i=0;i<16;i++) w[i]=(b[4*i]<<24)|(b[4*i+1]<<16)|(b[4*i+2]<<8)|b[4*i+3];
	for (int i=16;i<64;i++) w[i]=s1(w[i-2])+w[i-7]+s0(w[i-15])+w[i-16];
	a=c->s[0]; b0=c->s[1]; c0=c->s[2]; d=c->s[3]; e=c->s[4]; f=c->s[5]; g=c->s[6]; h=c->s[7];
	for (int i=0;i<64;i++){
		uint32_t t1=h+S1(e)+Ch(e,f,g)+K[i]+w[i];
		uint32_t t2=S0(a)+Maj(a,b0,c0);
		h=g; g=f; f=e; e=d+t1; d=c0; c0=b0; b0=a; a=t1+t2;
	}
	c->s[0]+=a; c->s[1]+=b0; c->s[2]+=c0; c->s[3]+=d; c->s[4]+=e; c->s[5]+=f; c->s[6]+=g; c->s[7]+=h;
}

static void upd(shactx* c,const uint8_t* p,size_t n) {
	c->bits += (uint64_t)n*8;
	while(n--){
		c->buf[c->n++]=*p++;
		if(c->n==64){ blk(c,c->buf); c->n=0;
		}
	}
}

static void fin(shactx* c,uint8_t out[32]) {
	size_t i=c->n;
	c->buf[i++]=0x80;
	if(i>56){ while(i<64) c->buf[i++]=0; blk(c,c->buf); i=0; }
	while(i<56) c->buf[i++]=0;
	uint64_t bits=c->bits;
	for(int j=7;j>=0;j--) c->buf[i++]=(uint8_t)((bits>>(8*j))&0xFF);
	blk(c,c->buf);
	for(int k=0;k<8;k++){
		out[4*k+0]=(uint8_t)(c->s[k]>>24);
		out[4*k+1]=(uint8_t)(c->s[k]>>16);
		out[4*k+2]=(uint8_t)(c->s[k]>>8);
		out[4*k+3]=(uint8_t)(c->s[k]);
	}
}

void cell_sha256(const uint8_t* buf, size_t len, uint8_t out[32]) {
	shactx c; init(&c); upd(&c, buf, len); fin(&c, out);
}
