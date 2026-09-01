/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include "cortex/cortex.h"
#include <stdint.h>
#include <string.h>
#if defined(__x86_64__) && defined(__SSE2__)
#include <emmintrin.h>
#endif

typedef struct { float scale; } q8_hdr_t;

typedef struct {
	const int8_t *tok_emb; float tok_scale;
	const int8_t *pos_emb; float pos_scale;
	const uint8_t *layers;
	const float *final_norm;
} weights_view_t;

static size_t align16(size_t n) { return (n + 15u) & ~15u; }

static float inv_sqrt(float x) {
	if (x <= 0.0f) return 0.0f;
	union { float f; uint32_t i; } u = { x };
	u.i = 0x5f3759dfu - (u.i >> 1);
	float y = u.f;
	y = y * (1.5f - 0.5f * x * y * y);
	y = y * (1.5f - 0.5f * x * y * y);
	return y;
}

static float fast_exp(float x) {
	if (x < -12.0f) return 0.0f;
	if (x > 12.0f) x = 12.0f;
	float z = x * (1.0f / 16.0f);
	float z2 = z * z;
	float y = 1.0f + z + z2 * 0.5f + z2*z * (1.0f/6.0f) + z2*z2 * (1.0f/24.0f);
	for (int i = 0; i < 4; ++i) y *= y;
	return y;
}

static const uint8_t *take_q8(const uint8_t *p, uint32_t count, float *scale, const int8_t **data) {
	const q8_hdr_t *h = (const q8_hdr_t *)p;
	*scale = h->scale;
	*data = (const int8_t *)(p + sizeof(q8_hdr_t));
	return p + sizeof(q8_hdr_t) + count;
}

static void q8_row(const int8_t *w, float s, uint32_t cols, uint32_t row, float *out) {
	const int8_t *r = w + (size_t)row * cols;
	for (uint32_t i = 0; i < cols; ++i) out[i] = (float)r[i] * s;
}

static float dot_q8(const int8_t *w, const float *x, uint32_t n) {
	uint32_t i = 0;
	float sum = 0.0f;
#if defined(__x86_64__) && defined(__SSE2__)
	__m128 acc = _mm_setzero_ps();
	const __m128i zero = _mm_setzero_si128();
	for (; i + 8u <= n; i += 8u) {
		__m128i b = _mm_loadl_epi64((const __m128i *)(const void *)(w + i));
		__m128i s8 = _mm_cmpgt_epi8(zero, b);
		__m128i v16 = _mm_unpacklo_epi8(b, s8);
		__m128i s16 = _mm_cmpgt_epi16(zero, v16);
		__m128i lo32 = _mm_unpacklo_epi16(v16, s16);
		__m128i hi32 = _mm_unpackhi_epi16(v16, s16);
		__m128 flo = _mm_cvtepi32_ps(lo32);
		__m128 fhi = _mm_cvtepi32_ps(hi32);
		acc = _mm_add_ps(acc, _mm_mul_ps(flo, _mm_loadu_ps(x + i)));
		acc = _mm_add_ps(acc, _mm_mul_ps(fhi, _mm_loadu_ps(x + i + 4u)));
	}
	float lanes[4]; _mm_storeu_ps(lanes, acc);
	sum = lanes[0] + lanes[1] + lanes[2] + lanes[3];
#endif
	for (; i < n; ++i) sum += (float)w[i] * x[i];
	return sum;
}

static void matvec_q8(const int8_t *w, float s, uint32_t rows, uint32_t cols,
			 const float *x, float *y) {
	for (uint32_t r = 0; r < rows; ++r) {
		const int8_t *rw = w + (size_t)r * cols;
		y[r] = dot_q8(rw, x, cols) * s;
	}
}

static void rmsnorm(float *out, const float *x, const float *w, uint32_t n) {
	float ss = 0.0f;
	for (uint32_t i = 0; i < n; ++i) ss += x[i] * x[i];
	float inv = inv_sqrt(ss / (float)n + 1.0e-5f);
	for (uint32_t i = 0; i < n; ++i) out[i] = x[i] * inv * w[i];
}

static size_t layer_q8_bytes(const cwm_header_t *h) {
	uint64_t d = h->d_model, f = h->d_ff;
	uint64_t q8 = 4u + d*d;
	return (size_t)(d * 4u + q8*4u + d*4u + (4u + f*d) + (4u + d*f));
}

static int parse_top(const cwm_model_t *m, weights_view_t *v) {
	const cwm_header_t *h = m->h;
	const uint8_t *p = m->weights;
	p = take_q8(p, h->vocab_size * h->d_model, &v->tok_scale, &v->tok_emb);
	p = take_q8(p, h->context_len * h->d_model, &v->pos_scale, &v->pos_emb);
	v->layers = p;
	p += layer_q8_bytes(h) * h->n_layers;
	v->final_norm = (const float *)p;
	p += h->d_model * sizeof(float);
	return (size_t)(p - m->weights) == m->h->weights_bytes;
}

size_t cortex_workspace_bytes(const cwm_model_t *m) {
	if (!m || !m->h) return 0;
	const cwm_header_t *h = m->h;
	size_t floats = (size_t)h->d_model * 4u + h->d_ff + h->vocab_size;
	floats += h->context_len;
	floats += (size_t)h->n_layers * h->context_len * h->d_model * 2u;
	return align16(floats * sizeof(float));
}

int cortex_init(cortex_t *c, const cwm_model_t *m, void *workspace, size_t workspace_bytes) {
	if (!c || !m || !m->h || !workspace) return 0;
	weights_view_t v;
	if (!parse_top(m, &v)) return 0;
	if (workspace_bytes < cortex_workspace_bytes(m)) return 0;
	memset(c, 0, sizeof(*c));
	c->model = *m;
	float *p = (float *)workspace;
	uint32_t d = m->h->d_model;
	c->x = p; p += d;
	c->q = p; p += d;
	c->tmp = p; p += d;
	/* one extra d_model-sized scratch immediately after tmp */
	p += d;
	c->ff = p; p += m->h->d_ff;
	c->logits = p; p += m->h->vocab_size;
	c->scores = p; p += m->h->context_len;
	c->k_cache = p; p += (size_t)m->h->n_layers * m->h->context_len * d;
	c->v_cache = p;
	cortex_reset(c);
	return 1;
}

void cortex_reset(cortex_t *c) {
	if (!c || !c->model.h) return;
	c->pos = 0;
	size_t n = (size_t)c->model.h->n_layers * c->model.h->context_len * c->model.h->d_model;
	memset(c->k_cache, 0, n * sizeof(float));
	memset(c->v_cache, 0, n * sizeof(float));
}

static const uint8_t *layer_parse(const cwm_header_t *h, const uint8_t *p,
	const float **n1, const int8_t **wq, float *sq, const int8_t **wk, float *sk,
	const int8_t **wv, float *sv, const int8_t **wo, float *so, const float **n2,
	const int8_t **w1, float *s1, const int8_t **w2, float *s2) {
	*n1 = (const float *)p; p += h->d_model * 4u;
	p = take_q8(p, h->d_model*h->d_model, sq, wq);
	p = take_q8(p, h->d_model*h->d_model, sk, wk);
	p = take_q8(p, h->d_model*h->d_model, sv, wv);
	p = take_q8(p, h->d_model*h->d_model, so, wo);
	*n2 = (const float *)p; p += h->d_model * 4u;
	p = take_q8(p, h->d_ff*h->d_model, s1, w1);
	p = take_q8(p, h->d_model*h->d_ff, s2, w2);
	return p;
}

static void forward_token(cortex_t *c, uint8_t token) {
	const cwm_header_t *h = c->model.h;
	weights_view_t top;
	(void)parse_top(&c->model, &top);
	uint32_t d = h->d_model;
	uint32_t pos = c->pos < h->context_len ? c->pos : h->context_len - 1u;
	q8_row(top.tok_emb, top.tok_scale, d, token % h->vocab_size, c->x);
	q8_row(top.pos_emb, top.pos_scale, d, pos, c->tmp);
	for (uint32_t i = 0; i < d; ++i) c->x[i] += c->tmp[i];

	const uint8_t *lp = top.layers;
	float *att = c->tmp + d; /* the reserved d-sized scratch */
	for (uint32_t l = 0; l < h->n_layers; ++l) {
		const float *n1, *n2; const int8_t *wq,*wk,*wv,*wo,*w1,*w2;
		float sq,sk,sv,so,s1,s2;
		const uint8_t *next = layer_parse(h, lp, &n1,&wq,&sq,&wk,&sk,&wv,&sv,&wo,&so,&n2,&w1,&s1,&w2,&s2);
		rmsnorm(c->q, c->x, n1, d);
		matvec_q8(wq,sq,d,d,c->q,c->tmp);
		matvec_q8(wk,sk,d,d,c->q,att);
		float *kc = c->k_cache + ((size_t)l*h->context_len + pos)*d;
		for (uint32_t i=0;i<d;++i) kc[i]=att[i];
		matvec_q8(wv,sv,d,d,c->q,att);
		float *vc = c->v_cache + ((size_t)l*h->context_len + pos)*d;
		for (uint32_t i=0;i<d;++i) vc[i]=att[i];

		uint32_t hd = d / h->n_heads;
		for (uint32_t i=0;i<d;++i) att[i]=0.0f;
		for (uint32_t head=0; head<h->n_heads; ++head) {
			float *scores = c->scores;
			uint32_t upto = pos + 1u;
			float maxs=-1.0e30f;
			for (uint32_t t=0;t<upto;++t) {
				const float *k = c->k_cache + ((size_t)l*h->context_len+t)*d + head*hd;
				float s=0.0f;
				for (uint32_t j=0;j<hd;++j) s += c->tmp[head*hd+j]*k[j];
				s *= inv_sqrt((float)hd);
				scores[t]=s; if(s>maxs)maxs=s;
			}
			float denom=0.0f;
			for(uint32_t t=0;t<upto;++t){ scores[t]=fast_exp(scores[t]-maxs); denom+=scores[t]; }
			float invd=denom>0.0f?1.0f/denom:0.0f;
			for(uint32_t t=0;t<upto;++t){
				const float *v = c->v_cache + ((size_t)l*h->context_len+t)*d + head*hd;
				float a=scores[t]*invd;
				for(uint32_t j=0;j<hd;++j) att[head*hd+j]+=a*v[j];
			}
		}
		matvec_q8(wo,so,d,d,att,c->q);
		for(uint32_t i=0;i<d;++i)c->x[i]+=c->q[i];
		rmsnorm(c->q,c->x,n2,d);
		matvec_q8(w1,s1,h->d_ff,d,c->q,c->ff);
		for(uint32_t i=0;i<h->d_ff;++i) if(c->ff[i]<0.0f)c->ff[i]=0.0f;
		matvec_q8(w2,s2,d,h->d_ff,c->ff,c->q);
		for(uint32_t i=0;i<d;++i)c->x[i]+=c->q[i];
		lp=next;
	}
	rmsnorm(c->q,c->x,top.final_norm,d);
	for(uint32_t v=0;v<h->vocab_size;++v){
		const int8_t *r=top.tok_emb+(size_t)v*d;
		c->logits[v]=dot_q8(r,c->q,d)*top.tok_scale;
	}
}

int cortex_feed(cortex_t *c, uint8_t token) {
	if (!c || !c->model.h || c->pos >= c->model.h->context_len) return 0;
	forward_token(c, token);
	c->pos++;
	return 1;
}

uint8_t cortex_next(cortex_t *c) {
	if (!c || !c->model.h) return 0;
	uint32_t best=0; float bv=-1.0e30f;
	for(uint32_t i=0;i<c->model.h->vocab_size;++i){ if(c->logits[i]>bv){bv=c->logits[i];best=i;} }
	return (uint8_t)best;
}
