/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */
#include<signal.h>
#include<fcntl.h>
#include<sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<unistd.h>
#include<stdbool.h>
#include "exec_parser.h"

static so_exec_t *exec;
static int file,sizeofpage;
static struct sigaction oldhandl;
typedef struct Mappage {
	int pages[1025];
	int number;
} Mappage;

void *pageAddr;
int rc;

//Finds if page was already mapped
bool find(struct Mappage *map, int currpage){
	for(int i=0; i<map->number; i++){
		if(map->pages[i]==currpage)
			return true;
		return false;
	}
}

//Copy data to new page
static void copy_to(struct Mappage *map,int currpage,int pageoffset,int i){
	int filesize = exec->segments[i].file_size;
	int memory=currpage*sizeofpage;

	if(filesize>memory){
		if(filesize<(currpage+1)*sizeofpage){
			lseek(file,pageoffset,SEEK_SET);
			rc=read(file,pageAddr,filesize-memory);
		}else{
			lseek(file,pageoffset,SEEK_SET);
			rc=read(file,pageAddr,sizeofpage);
		}
	}
}

static void segv_handler(int signum, siginfo_t *info, void *context) {
	/* TODO - actual loader implementation */
	size_t addr;
	int i=0,currpage,pageoffset,nr;
	struct Mappage *map;

	//Old handler
	if(signum!=SIGSEGV){
		oldhandl.sa_sigaction(signum,info,context);
		return;
	}
	//Address that caused signal
	addr=(size_t)info->si_addr;
	//Takes every segment
label:
	while(i<exec->segments_no){
		map = exec->segments[i].data;
		nr=map->number;

	    //Checks if current segment produce page fault
		if(addr >= exec->segments[i].vaddr + exec->segments[i].mem_size || addr < exec->segments[i].vaddr){
			i++;
			goto label;
		}

		currpage=(addr-exec->segments[i].vaddr)/sizeofpage;
		pageoffset=exec->segments[i].offset+currpage*sizeofpage;

		//Check if page is already mapped and use the old handler
		if(find(map,currpage)){
			oldhandl.sa_sigaction(signum,info,context);
			return ;
		}

		//Map a new memory
		pageAddr=mmap((void *)(exec->segments[i].vaddr+currpage*sizeofpage),getpagesize(),
		PERM_W, MAP_FIXED| MAP_SHARED| MAP_ANONYMOUS, -1, 0);

		//Copy data to the new page
		copy_to(map,currpage,pageoffset,i);
		
		//Add permissions
		mprotect(pageAddr,sizeofpage,exec->segments[i].perm);
	
		//Add the new map to the list of mapped pages
		map->pages[nr]=currpage;
		map->number++;
		return;
		}

		//Address not found so use old handler
		oldhandl.sa_sigaction(signum, info, context);
}


int so_init_loader(void)
{
	sizeofpage = getpagesize();
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
	
}

int so_execute(char *path, char *argv[])
{
	int i=0;

	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	 //Open file where to copy data
	file = open(path, O_RDONLY);

	 //Init structure for each segment
	do{
		exec->segments[i].data=calloc(1,sizeof(struct Mappage));
		i++;
		}while(i<exec->segments_no);
	so_start_exec(exec, argv);
	return 0;
}
