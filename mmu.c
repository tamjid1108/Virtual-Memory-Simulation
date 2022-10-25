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

void printPageTable(int n, page_table_entry* pageTable);;
void continueHandler(int Signal); 

int main(int argc, char *argv[]){

    int sharedMemoryKey;
    int no_of_pages;
    int OSPID;
    int segmentId;

    page_table_entry* pageTable;

    if( argc <2 ||
        (OSPID = sharedMemoryKey = atoi(argv[argc-1])) == 0 || 
        (no_of_pages = atoi(argv[1])) == 0)
    {
    printf("[USAGE] %s 'no of pages' 'reference string' 'ospid/key'\n", argv[0]);
    exit(EXIT_FAILURE);
    }

    segmentId = shmget(sharedMemoryKey, no_of_pages*sizeof(page_table_entry), 0);
    if(segmentId == -1){
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    pageTable = (page_table_entry *)shmat(segmentId, NULL, 0);

    if(pageTable==NULL){
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGCONT, continueHandler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    printf("[INITIALIZED PAGE TABLE]\n");
    printPageTable(no_of_pages, pageTable);

    
    for (int req=2; req < argc-1; req++) {
        int mode = argv[req][0];
        int page = atoi(&argv[req][1]);
       
        //checking if the page number in reference string is applicable
        if (page >= no_of_pages){
            printf("[ERROR] The page-%d in %c%c is outside the process\n", page, mode, page);
        }
        else{
            printf("[REFERENCE] for page-%d in %c mode\n", page, mode);
           
            //checking if it is in memory
            if (pageTable[page].valid != 1){
                printf("[PAGE FAULT] Page-%d is not in RAM\n", page);
                pageTable[page].requested = getpid();

                sleep(1);

                if (kill(OSPID,SIGUSR1) == -1) {
                    perror("Kill to OS");
                    exit(EXIT_FAILURE);
                }

                //suspend the MMU process until a signal arrives from os to execute the handler or to terminate the process
                pause();

                if (!pageTable[page].valid) {
                    printf("[OOPS! SOMETHING WENT WRONG]\n");
                }

            }
            else{
                printf("[PAGE AVAILABLE IN RAM]\n");
            }

            printf("[ACCESSING] Page-%d in %c mode\n", page, mode);

            pageTable[page].last_accessed_at = clock();
            if (mode == 'W') {
                printf("[DIRTY BIT SET] for Page-%d\n", page);
                pageTable[page].dirty = 1;
            }

            printPageTable(no_of_pages, pageTable);

        }
    }

    int dtResult;

    dtResult = shmdt(pageTable);

    if(dtResult == -1){
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    //signalling OS one last time when there is no request
    printf("[MMU FINISHED SUCCESSFULLY]\n");

    sleep(1);

    if (kill(OSPID,SIGUSR1) == -1) {
        perror("Kill to OS");
        exit(EXIT_FAILURE);
    }

    return(EXIT_SUCCESS);

}


void printPageTable(int n, page_table_entry* pageTable){
    printf("\n");
    printf("| %12s | %10s | %10s | %10s | %12s |\n", "Page No.", "Valid", "Frame No.", "Dirty", "Requested");
    for(int i=0; i<n; i++){
        printf("| %12d | %10d | %10d | %10d | %12d |\n",  i,
                                                        pageTable[i].valid,
                                                        pageTable[i].frame,
                                                        pageTable[i].dirty,
                                                        pageTable[i].requested);
    }
    printf("\n");
}

void continueHandler(int Signal) {
    printf("[CONTROL BACK TO MMU]\n");
}