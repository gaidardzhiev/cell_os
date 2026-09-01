/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "core/cellfs.h"
#include "core/vfs.h"
#include "core/task.h"
#include "core/capability.h"

static uint8_t *disk_image;
static uint32_t disk_sectors;
static int rd(void *ctx,uint64_t lba,void *dst,uint32_t sectors){(void)ctx;if(!dst||lba>=disk_sectors||sectors>disk_sectors-lba)return 0;memcpy(dst,disk_image+lba*512u,(size_t)sectors*512u);return 1;}
static int wr(void *ctx,uint64_t lba,const void *src,uint32_t sectors){(void)ctx;if(!src||lba>=disk_sectors||sectors>disk_sectors-lba)return 0;memcpy(disk_image+lba*512u,src,(size_t)sectors*512u);return 1;}
static int load(const char*path){FILE*f=fopen(path,"rb");if(!f)return 0;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<=0||n%512){fclose(f);return 0;}disk_image=malloc((size_t)n);if(!disk_image){fclose(f);return 0;}if(fread(disk_image,1,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}fclose(f);disk_sectors=(uint32_t)n/512u;return 1;}
static int must(int cond,const char*what,const char*got){if(cond)return 1;fprintf(stderr,"FAIL %s",what);if(got)fprintf(stderr,": %s",got);fputc('\n',stderr);return 0;}
int main(int argc,char**argv){if(argc!=2){fprintf(stderr,"usage: %s cellfs.img\n",argv[0]);return 2;}if(!load(argv[1])){perror("cellfs");return 1;}cellfs_disk_t d={rd,wr,0,0,disk_sectors};cell_vfs_t vfs;if(!cell_vfs_mount(&vfs,&d,0,0,0,0,0)){fprintf(stderr,"mount failed\n");return 1;}cell_capability_env_t env={0};env.vfs=&vfs;env.cortex_ready=1;env.ata0_ready=1;cell_task_manager_t tm;cell_task_manager_init(&tm,cell_task_default_policy());env.tasks=&tm;char out[1024];uint32_t id=0;int ok=1;
ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/hello",out,sizeof(out),&id),"run hello",out);ok&=must(strcmp(out,"Hello from C on Cell OS.\n")==0,"hello output",out);
ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/observe",out,sizeof(out),&id),"run observe",out);ok&=must(strstr(out,"C observation:\nCell OS is running.")==out,"observe system",out);ok&=must(strstr(out,"Memory:")!=0,"observe memory",out);
free(disk_image);if(!ok)return 1;puts("#CELLEXEC persistent image execution PASS");puts("#CELLEXEC capability call from installed program PASS");return 0;}
