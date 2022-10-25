#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <time.h>

#include "PageTable.h"


int no_of_pages;
int no_of_frames;
int frames_avaiable;
int no_of_disk_accesses=0;
int segmentId;

page_table_entry* pageTable;

void sigHandler(int sigNo);
void requestHandler();
int allocate();
int victimAllocate();


int main(int argc, char* argv[]){
    no_of_pages = atoi(argv[1]);
    no_of_frames = atoi(argv[argc-1]);
    frames_avaiable = no_of_frames;

    //will use the OS process id as the key for shared memory
    key_t key = getpid();

    //create a shared memory region that will hold the page table
    segmentId = shmget(key, no_of_pages* sizeof(page_table_entry), IPC_CREAT | 0660);

    if(segmentId==-1){
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    //attaching it to the process' address space
    pageTable = (page_table_entry *)shmat(segmentId, NULL, 0);

    if(pageTable==NULL){
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    printf("[SHARED MEMORY KEY] %d\n", key);


    //initializing the page table

    for(int i=0; i<no_of_pages; i++){
        pageTable[i].valid = 0;
        pageTable[i].frame =-1;
        pageTable[i].dirty = 0;
        pageTable[i].requested = 0;
    }
    printf("[PAGE TABLE INITIALIZED SUCCESSFULLY]\n");


    if(signal(SIGUSR1, sigHandler) == SIG_ERR){
        perror("signal");
        exit(EXIT_FAILURE);
    }

    //keep waiting for signal from mmu process.
    while(1);

    return 0;
}

void sigHandler(int sigNo){
    if(sigNo == SIGUSR1){
        requestHandler();
    }
}

void requestHandler(){
    int frame_to_replace = -1;

    int requestedBy = -1;

    for(int i=0; i< no_of_pages; i++){
        if(pageTable[i].requested !=0){
            printf("[REQUEST FROM PROCESS %d] Page-%d\n", pageTable[i].requested, i);

            if(frames_avaiable>0){
                frame_to_replace = allocate();
                printf("[FREE FRAME FOUND] Frame-%d\n", frame_to_replace);
            }

            else{
                printf("[NO FREE FRAMES FOUND]\n");
                frame_to_replace = victimAllocate();
            }

            requestedBy = pageTable[i].requested;
            pageTable[i].valid = 1;
            pageTable[i].dirty = 0;
            pageTable[i].frame = frame_to_replace;
            pageTable[i].requested = 0;
            break;
        }
    }

    if(requestedBy != -1){
        //simulating disk read
        no_of_disk_accesses +=1;
        sleep(rand()%2+1);

        printf("[UNBLOCK MMU]\n");
        kill(requestedBy, SIGCONT);
    }

    else{
        int dtResult, ctlResult;

        dtResult = shmdt(pageTable);

        if(dtResult == -1){
            perror("shmdt");
            exit(EXIT_FAILURE);
        }

        ctlResult = shmctl(segmentId, IPC_RMID, NULL);

        if(ctlResult == -1){
            perror("[ERROR] removing segment");
            exit(EXIT_FAILURE);
        }

        printf("[NO OF DISK ACCESSES REQUIRED] %d\n", no_of_disk_accesses);
        exit(EXIT_SUCCESS);
    }
}

int allocate(){
    return frames_avaiable--;
    return (no_of_frames - frames_avaiable -1);
}

int victimAllocate(){
    clock_t LRUTime = clock();
    
    int row=-1;
    int hold_frame=-1;
    for(int i=0; i<no_of_pages; i++){
        if(pageTable[i].valid==1){
            if(LRUTime > pageTable[i].last_accessed_at){
                LRUTime = pageTable[i].last_accessed_at;
                row = i;
            }
        }
    }
    hold_frame = pageTable[row].frame;

    printf("[VICTIM FRAME FOUND] Frame-%d\n", hold_frame);

    if(pageTable[hold_frame].dirty==1){
        printf("[VICTIM IS DIRTY] Write out\n");
        //simulating disk write
        no_of_disk_accesses +=1;
        sleep(rand()%2+1);
    }

    pageTable[row].valid = 0;
    pageTable[row].dirty = 0;
    pageTable[row].frame =-1;
    pageTable[row].requested = 0;

    return hold_frame;
}