#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "cortex/cwm.h"
#include "cortex/cortex.h"

int main(int argc, char **argv) {
	if (argc != 3) { fprintf(stderr,"usage: %s model prompt\n",argv[0]); return 2; }
	FILE *f=fopen(argv[1],"rb"); if(!f){perror("fopen");return 1;}
	fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
	uint8_t *buf=malloc((size_t)n); if(!buf||fread(buf,1,(size_t)n,f)!=(size_t)n)return 1; fclose(f);
	cwm_model_t m; if(!cwm_open(&m,buf,(size_t)n)){fprintf(stderr,"bad model\n");return 1;}
	size_t wn=cortex_workspace_bytes(&m); void *ws=calloc(1,wn); cortex_t c;
	if(!cortex_init(&c,&m,ws,wn)){fprintf(stderr,"init failed\n");return 1;}
	const unsigned char *p=(const unsigned char*)argv[2];
	uint8_t last=0;
	while(*p && c.pos<m.h->context_len-1){
		last=*p;
		cortex_feed(&c,*p++);
	}
	if(last!='\n' && c.pos<m.h->context_len-1){
		cortex_feed(&c,(uint8_t)'\n');
	}
	for(int i=0;i<96 && c.pos<m.h->context_len-1;i++){
		uint8_t t=cortex_next(&c);
		if(t==0) break;
		putchar((char)t);
		fflush(stdout);
		if(!cortex_feed(&c,t) || t=='\n') break;
	}
	putchar('\n'); free(ws); free(buf); return 0;
}
