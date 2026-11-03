#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "runner.h"

/* Objective 9: arbitrary binary PDF seed corpus, mutation, edge hit counts. */
#define MAX_INPUT (2U * 1024U * 1024U)
#define MAX_CORPUS 1000
struct seed { unsigned char *bytes; size_t length; };
static struct seed corpus[MAX_CORPUS];
static size_t ncorpus;
static unsigned char *maxima;
static size_t total_edges;
static unsigned covered_edges;
static unsigned long long executions, crashes, timeouts, interesting;
static double start;
static unsigned int rng;

static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec + t.tv_nsec/1e9;}
static int load_file(const char *path, unsigned char **out, size_t *length){
 FILE *f=fopen(path,"rb");if(!f)return -1;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return -1;}
 long n=ftell(f);if(n<1||(unsigned long)n>MAX_INPUT){fclose(f);return -1;}
 rewind(f);unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return -1;}
 if(fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return -1;}
 fclose(f);*out=p;*length=(size_t)n;return 0;
}
static int write_file(const char *path,const unsigned char *data,size_t n){
 FILE *f=fopen(path,"wb");if(!f)return -1;
 int ok=fwrite(data,1,n,f)==n && fflush(f)==0;fclose(f);return ok?0:-1;
}
static void add_seed(unsigned char *data,size_t n){
 if(ncorpus<MAX_CORPUS){corpus[ncorpus].bytes=data;corpus[ncorpus++].length=n;}
 else{size_t i=(size_t)rand()%MAX_CORPUS;free(corpus[i].bytes);corpus[i].bytes=data;corpus[i].length=n;}
}
static int seed_copy(const unsigned char *p,size_t n){
 unsigned char *b=malloc(n);if(!b)return -1;memcpy(b,p,n);add_seed(b,n);return 0;
}
static int load_seeds(void){
 DIR *d=opendir("seeds");if(!d){perror("seeds");return -1;}
 struct dirent *e;int n=0;
 while((e=readdir(d))){
  if(e->d_name[0]=='.')continue;
  char name[2048];snprintf(name,sizeof(name),"seeds/%s",e->d_name);
  struct stat st;if(stat(name,&st)!=0||!S_ISREG(st.st_mode))continue;
  unsigned char *b=NULL;size_t len=0;
  if(load_file(name,&b,&len)!=0)continue;
  if(ncorpus>=MAX_CORPUS){free(b);break;}
  add_seed(b,len);fprintf(stderr,"Loaded seed %s (%zu bytes)\n",name,len);n++;
 }
 closedir(d);return n;
}
static int mutate(unsigned char **out,size_t *len){
 if(!ncorpus)return -1;
 struct seed *s=&corpus[(size_t)rand()%ncorpus];
 size_t n=s->length;unsigned char *b=malloc(n+32);if(!b)return -1;
 memcpy(b,s->bytes,n);
 /* Byte replacement, insertion and deletion; retain the original PDF prefix. */
 int steps=1+rand()%8;
 for(int k=0;k<steps;k++){
  int op=rand()%3;
  if(op==0 && n){size_t pos=(size_t)rand()%n;b[pos]=(unsigned char)(rand()%256);}
  else if(op==1 && n<MAX_INPUT && n<s->length+31){size_t pos=(size_t)rand()%(n+1);memmove(b+pos+1,b+pos,n-pos);b[pos]=(unsigned char)(rand()%256);n++;}
  else if(op==2 && n>1){size_t pos=(size_t)rand()%n;memmove(b+pos,b+pos+1,n-pos-1);n--;}
 }
 *out=b;*len=n;return 0;
}
/* Returns true iff this run increases an observed per-edge 8-bit maximum. */
static int update_coverage(void){
 FILE *f=fopen("cov.bin","rb");if(!f)return 0;
 if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
 long size=ftell(f);if(size<=0||size>100000000){fclose(f);return 0;}
 rewind(f);size_t n=(size_t)size;
 unsigned char *hit=malloc(n);if(!hit){fclose(f);return 0;}
 if(fread(hit,1,n,f)!=n){free(hit);fclose(f);return 0;}fclose(f);
 if(total_edges==0){maxima=calloc(n,1);if(!maxima){free(hit);return 0;}total_edges=n;}
 if(total_edges!=n){fprintf(stderr,"WARNING changing/incomplete coverage size %zu expected %zu\n",n,total_edges);free(hit);return 0;}
 int changed=0;
 for(size_t i=0;i<n;i++)if(hit[i]>maxima[i]){if(maxima[i]==0)covered_edges++;maxima[i]=hit[i];changed=1;}
 free(hit);return changed;
}
static void run_case(const char *target,const unsigned char *data,size_t len,int timeout_ms){
 if(write_file("current.pdf",data,len)!=0){perror("current.pdf");return;}
 unlink("cov.bin");outcome_t result;
 if(run_target_pdf(target,"current.pdf",timeout_ms,&result)!=0){perror("run_target_pdf");return;}
 executions++;
 if(result.kind==CRASH||result.kind==TIMEOUT){
  if(result.kind==CRASH)crashes++;else timeouts++;
  mkdir("abnormal",0755);
  char name[256];snprintf(name,sizeof name,"abnormal/%s_%012llu_sig%d.pdf",result.kind==CRASH?"crash":"timeout",executions,result.signal_number);
  write_file(name,data,len);
  fprintf(stderr,"[%.1fs] %s saved %s\n",now()-start,result.kind==CRASH?"CRASH":"TIMEOUT",name);
 } else if(result.kind==EXEC_FAIL){fprintf(stderr,"EXEC_FAIL errno=%d\n",result.exit_code);}
 else if(update_coverage()){
  interesting++;
  if(interesting<=MAX_CORPUS){mkdir("interesting",0755);char name[256];snprintf(name,sizeof name,"interesting/case_%012llu.pdf",executions);write_file(name,data,len);}
  seed_copy(data,len);
 }
 if(executions%1000==0){double d=now()-start;fprintf(stderr,"[%.0fs] cases=%llu edges=%u/%zu interesting=%llu crashes=%llu timeouts=%llu rate=%.2f/s\n",d,executions,covered_edges,total_edges,interesting,crashes,timeouts,executions/(d?d:1));fflush(stderr);}
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"Usage: %s ABSOLUTE_PDFTOTEXT TIMEOUT_MS [RNG_SEED] [DURATION_SECONDS]\n",argv[0]);return 2;}
 const char *target=argv[1];int timeout_ms=atoi(argv[2]);if(timeout_ms<=0){fprintf(stderr,"timeout must be positive\n");return 2;}
 rng=argc>3?(unsigned)strtoul(argv[3],NULL,10):(unsigned)(time(NULL)^getpid());srand(rng);
 double duration=argc>4?atof(argv[4]):86400;if(duration<=0)return 2;
 if(access(target,X_OK)){perror(target);return 2;}
 setenv("COV_FILE","cov.bin",1);
 int n=load_seeds();if(n<=0){fprintf(stderr,"Need at least one binary PDF seed\n");return 2;}
 start=now();fprintf(stderr,"Objective 9 started with %d PDFs rng=%u timeout=%d duration=%.0f\n",n,rng,timeout_ms,duration);
 /* Include the initial corpus evaluations within the timed experiment. */
 size_t initial=ncorpus;
 for(size_t i=0;i<initial&&now()-start<duration;i++)run_case(target,corpus[i].bytes,corpus[i].length,timeout_ms);
 while(now()-start<duration){unsigned char *b=NULL;size_t len=0;if(mutate(&b,&len))break;run_case(target,b,len,timeout_ms);free(b);}
 double elapsed=now()-start;char host[256]="unknown";gethostname(host,sizeof(host));
 printf("=== TRIAL COMPLETE ===\nRlogin node: %s\nRNG seed: %u\nPer-test-case timeout: %d milliseconds\nTotal number of possible edges: %zu\nNumber of edges covered: %u\nNumber of test cases: %llu\nWall clock time: %.3f seconds\nTest case throughput: %.3f cases/s\nCrashes: %llu\nTimeouts: %llu\n",host,rng,timeout_ms,total_edges,covered_edges,executions,elapsed,executions/(elapsed?elapsed:1),crashes,timeouts);
 for(size_t i=0;i<ncorpus;i++) free(corpus[i].bytes);
 free(maxima);
 return 0;
}
