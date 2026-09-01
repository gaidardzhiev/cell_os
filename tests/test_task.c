/*
 * Copyright (c) 2025 Mihail Banov and Ivan Gaydardzhiev
 * SPDX-License-Identifier: MIT
 *
 * This file is licensed under the MIT License.
 * See the LICENSE file in the project root for full license text.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "core/cellexec.h"
#include "core/task.h"
#include "core/cellfs.h"
#include "core/vfs.h"
#include "core/capability.h"

#define FS_SECTORS 1024u
static uint8_t disk_bytes[FS_SECTORS*CELLFS_SECTOR_SIZE];
static int rd(void *ctx,uint64_t lba,void *dst,uint32_t sec){(void)ctx;if(lba>=FS_SECTORS||sec>FS_SECTORS-lba)return 0;memcpy(dst,disk_bytes+lba*512u,sec*512u);return 1;}
static int wr(void *ctx,uint64_t lba,const void *src,uint32_t sec){(void)ctx;if(lba>=FS_SECTORS||sec>FS_SECTORS-lba)return 0;memcpy(disk_bytes+lba*512u,src,sec*512u);return 1;}
static uint64_t known(void){uint64_t m=0;for(unsigned i=1;i<=11;i++)m|=1ull<<i;return m;}

static size_t build(uint8_t *out,size_t cap,const cell_exec_insn_t *code,uint32_t count,const uint8_t *data,uint32_t data_n,uint64_t caps,uint32_t gas,uint32_t mem){
	size_t total=64u+count*8u+data_n;if(total>cap)return 0;memset(out,0,total);cell_exec_header_t*h=(cell_exec_header_t*)out;
	h->magic=CELL_EXEC_MAGIC;h->version=CELL_EXEC_VERSION;h->header_bytes=64;h->instruction_bytes=8;h->code_bytes=count*8u;h->data_bytes=data_n;h->capability_mask=caps;h->memory_bytes=mem;h->gas_limit=gas;h->total_bytes=(uint32_t)total;
	memcpy(out+64,code,count*8u);if(data_n)memcpy(out+64+count*8u,data,data_n);h->payload_crc32=cell_exec_crc32(out+64,count*8u+data_n);return total;
}
static int put_program(cell_vfs_t*vfs,const char*name,const uint8_t*blob,size_t n){uint32_t programs=0,id=0;if(!cellfs_find_child(&vfs->fs,1,"programs",&programs))return 0;if(!cellfs_find_child(&vfs->fs,programs,name,&id)&&!cellfs_create_file(&vfs->fs,programs,name,&id))return 0;return cellfs_write_file(&vfs->fs,id,blob,n,0);}
static int must(int cond,const char*msg){if(cond)return 1;fprintf(stderr,"FAIL: %s\n",msg);return 0;}

int main(void){
	int ok=1;memset(disk_bytes,0,sizeof(disk_bytes));cellfs_disk_t d={rd,wr,0,0,FS_SECTORS};cellfs_t fmt;if(!cellfs_format(&fmt,&d))return 1;cell_vfs_t vfs;if(!cell_vfs_mount(&vfs,&d,100,256,256,128,5))return 1;
	cell_capability_env_t env={0};env.vfs=&vfs;env.cortex_ready=1;env.ata0_ready=1;
	cell_task_manager_t tm;cell_task_manager_init(&tm,cell_task_default_policy());env.tasks=&tm;
	uint8_t img[512];char out[1024];uint32_t id=0;
	const uint8_t hello[]="hello task\n";cell_exec_insn_t hc[]={{CELL_EXEC_OP_PUTS,0,11,0,0},{CELL_EXEC_OP_HALT,0,0,0,0}};size_t n=build(img,sizeof(img),hc,2,hello,11,0,16,0);ok&=must(put_program(&vfs,"hello",img,n),"install hello");
	ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/hello",out,sizeof(out),&id),"run hello");ok&=must(strcmp(out,"hello task\n")==0,"hello output");ok&=must(id==1,"task id");
	const uint8_t pfx[]="memory: ";cell_exec_insn_t mc[]={{CELL_EXEC_OP_PUTS,0,8,0,0},{CELL_EXEC_OP_CAP,0,0,0,CELL_CAP_MEMORY_STATUS},{CELL_EXEC_OP_HALT,0,0,0,0}};n=build(img,sizeof(img),mc,3,pfx,8,1ull<<CELL_CAP_MEMORY_STATUS,32,0);ok&=must(put_program(&vfs,"memory",img,n),"install memory");
	ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/memory",out,sizeof(out),&id),"run memory");ok&=must(strstr(out,"memory: Memory:")==out,"capability output");
	cell_exec_insn_t denied[]={{CELL_EXEC_OP_CAP,0,0,0,CELL_CAP_GPU_INFO},{CELL_EXEC_OP_HALT,0,0,0,0}};n=build(img,sizeof(img),denied,2,0,0,1ull<<CELL_CAP_GPU_INFO,16,0);ok&=must(put_program(&vfs,"denied",img,n),"install denied");
	ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/denied",out,sizeof(out),&id),"run denied");ok&=must(strstr(out,"Permission denied")!=0,"policy denial");
	cell_exec_insn_t loop[]={{CELL_EXEC_OP_JMP,0,0,0,-1}};n=build(img,sizeof(img),loop,1,0,0,0,3,0);ok&=must(put_program(&vfs,"loop",img,n),"install loop");
	ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/loop",out,sizeof(out),&id),"run loop");ok&=must(strstr(out,"runtime fault: gas.")!=0,"gas fault");
	cell_exec_insn_t mem[]={{CELL_EXEC_OP_MOVI,0,0,0,0},{CELL_EXEC_OP_MOVI,1,0,0,65},{CELL_EXEC_OP_STORE8,0,0,1,0},{CELL_EXEC_OP_LOAD8,2,0,0,0},{CELL_EXEC_OP_HALT,0,0,0,0}};n=build(img,sizeof(img),mem,5,0,0,0,16,16);ok&=must(put_program(&vfs,"mem",img,n),"install memory ops");
	ok&=must(cell_task_run(&tm,&vfs,&env,"/programs/mem",out,sizeof(out),&id),"run memory ops");ok&=must(tm.regs[2]==65,"memory/register VM");
	ok&=must(cell_vfs_touch(&vfs,"/home/not-program")==CELL_VFS_OK,"home file");ok&=must(cell_task_run(&tm,&vfs,&env,"/home/not-program",out,sizeof(out),&id),"home run result");ok&=must(strstr(out,"Permission denied")!=0,"execution root boundary");
	cell_exec_t ex;ok&=must(cell_exec_open(&ex,img,n,known())==CELL_EXEC_OK,"image remains valid");
	ok&=must(cell_task_list(&tm,out,sizeof(out)),"task list");ok&=must(strstr(out,"1 exited 0 2 /programs/hello")!=0,"task history");
	if(!ok)return 1;
	puts("#TASK lifecycle/history PASS");
	puts("#TASK gas/memory isolation PASS");
	puts("#TASK capability policy PASS");
	puts("#TASK /programs execution boundary PASS");
	return 0;
}
