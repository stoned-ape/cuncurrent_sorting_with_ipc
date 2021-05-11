#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sys/wait.h>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h> 
using namespace std;

//ipc = inter process communication

//if placed after a system call, this function will print a meaningfull
//error message if an error occures
void error(){
    if(errno!=0){
        cout<<"error ... "<<strerror(errno)<<endl;
        exit(-1);
    }
}

//multi-threaded mergesort, no ipc necessary.
//this is the only function in this file that has any practical value.
//this rest simply exist to demotrate ipc methods
void mergethread(int *a,int n){
    if(n<=1) return;
    thread t(mergethread,a,n/2);
    mergethread(a+n/2,n-n/2);
    int j=0,k=n/2;
    int *c=new int[n];
    t.join();
    for(int i=0;i<n;i++){
        if(j<n/2 && (a[j]<a[k] || n==k)){
            c[i]=a[j];
            j++;
        }else{
            c[i]=a[k];
            k++;
        }
    }
    for(int i=0;i<n;i++) a[i]=c[i];
    free(c);
}

//multi process merge sort using pipes for ipc.
//the child and parent processes sort the low and high halves of the array
//cuncurrently.  When the child is done, it writes the sorted low half
//of the array to the pipe then exits.  The parent sorts the high half
//then copys the low half from the pipe and merges the two halves.
void mergefork(int *a,int n){
    if(n<=1) return;
    int p[2];
    pipe(p);
    if(fork()==0){
        mergefork(a,n/2);
        write(p[1],a,sizeof(int)*(n/2));
        exit(0);
    }else{
        mergefork(a+n/2,n-n/2);
        read(p[0],a,sizeof(int)*(n/2));
    }
    int j=0,k=n/2;
    int *c=new int[n];
    for(int i=0;i<n;i++){
        if(j<n/2 && (a[j]<a[k] || n==k)){
            c[i]=a[j];
            j++;
        }else{
            c[i]=a[k];
            k++;
        }
    }
    for(int i=0;i<n;i++) a[i]=c[i];
    free(c);
}

//This works the same was as the previous function,
//except it uses named pipes.  Name pipes allow pipes to exits between
//processes that do not have a parent-child relationship
void mergefifo(int *a,int n,string name="pipe"){
    if(n<=1) return;
    int id=fork();
    if(id==0){
        mkfifo(name.c_str(),0600);
        int fd=open(name.c_str(),O_WRONLY);
        mergefifo(a,n/2,name+"0");
        write(fd,a,sizeof(int)*(n/2));
        exit(0);
    }else{
        int fd=-1;
        while(fd==-1) fd=open(name.c_str(),O_RDONLY);
        mergefifo(a+n/2,n-n/2,name+"1");
        read(fd,a,sizeof(int)*(n/2));
    }
    int j=0,k=n/2;
    int *c=new int[n];
    for(int i=0;i<n;i++){
        if(j<n/2 && (a[j]<a[k] || n==k)){
            c[i]=a[j];
            j++;
        }else{
            c[i]=a[k];
            k++;
        }
    }
    for(int i=0;i<n;i++) a[i]=c[i];
    free(c);
}

//this function performs multi process mergsort on an array that exists
//in shared memory
void merge_mem_rec(int *a,int n){
    if(n<=1) return;
    if(fork()==0){
        merge_mem_rec(a,n/2);
        exit(0);
    }else{
        merge_mem_rec(a+n/2,n-n/2);
    }
    int j=0,k=n/2;
    int *c=new int[n];
    wait(0);
    for(int i=0;i<n;i++){
        if(j<n/2 && (a[j]<a[k] || n==k)){
            c[i]=a[j];
            j++;
        }else{
            c[i]=a[k];
            k++;
        }
    }
    for(int i=0;i<n;i++) a[i]=c[i];
    free(c);
}

//for merge_mem_rec() to work the array must exist in shared memory
//the simplest way to to that is to use mmap() to get shared memory
//then copy the array into shared memory.  Once the array is sorted,
//we copy the sorted version back into the orginal array.
void mergemap(int *a,int n){
    void *v=mmap(nullptr,n*sizeof(int),
                 PROT_READ|PROT_WRITE,
                 MAP_SHARED|MAP_ANONYMOUS,-1,0);
    error();
    memcpy(v,a,n*sizeof(int));
    merge_mem_rec((int*)v,n);
    memcpy(a,v,n*sizeof(int));
    munmap(v,n*sizeof(int));
    error();
}

//the mergemap() method of creating shared memory only allows sharing
//between a parent and child.  This function uses named shared memory
//which can be accessed by any process.
void mergeshm(int *a,int n,string name="shm"){
    int fd=open(name.c_str(),O_RDWR|O_CREAT);
    error();
    truncate(name.c_str(),n*sizeof(int));
    error();
    key_t k=ftok(name.c_str(),65);
    int id=shmget(k,n*sizeof(int),0666|IPC_CREAT);
    error();
    void *v=shmat(id,0,0);
    error();
    memcpy(v,a,n*sizeof(int));
    merge_mem_rec((int*)v,n);
    memcpy(a,v,n*sizeof(int));
    shmdt(v);
    error();
}


int main(){
    int n=20;  //number of elements in the array
    int *a=new int[n];
    string fnames[]={"mergemap","mergeshm","mergethread","mergefork","mergefifo"};
    for(int j=0;j<5;j++){
        cout<<fnames[j]<<":\n";
        for(int i=0;i<n;i++) a[i]=n-i-1; //load array
        cout<<"\tunsorted: ";
        for(int i=0;i<n;i++) cout<<a[i]<<" ";  //print unsorted array
        cout<<endl<<endl;
        switch (j){     //sort the array with a perticular ipc method
            case 0:
                mergemap(a,n);
                break;
            case 1:
                mergeshm(a,n);
                break;
            case 2:
                mergethread(a,n);
                break;
            case 3:
                mergefork(a,n);
                break;
            case 4:
                mergefifo(a,n);
                break;
            
        }
        cout<<"\tsorted  : ";
        for(int i=0;i<n;i++) cout<<a[i]<<" ";  //print sorted array
        cout<<endl;
        //verify that the array is sorted
        for(int i=0;i<n-1;i++) if(a[i]>a[i+1]) cout<<"error: "<<i<<endl;
    }
    
    delete []a;
}




