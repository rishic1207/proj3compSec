#define OBJ 7
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "runner.h"
#ifndef OBJ
#define OBJ 7
#endif
#define INPUT_LEN 3
#define MAX_SEEDS 100
#define MAX_EDGES 65536
#define COV_FILE "cov.bin"
#define MAX_FINDINGS 500000
static const char alphabet[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static char seeds[MAX_SEEDS][4];
static int nseeds;
static unsigned char maxima[MAX_EDGES], hits[MAX_EDGES];
static size_t edge_count=0;
static unsigned covered_edges=0;
static unsigned long long executions, total_crashes, total_timeouts, exec_failures;
static double trial_start;
static char *target;
static int timeout_ms;
static unsigned int rng;
struct finding { char input[4]; int signal; unsigned long long occurrences; double first; };
static struct finding *crashes, *timeouts;
static size_t ncrashes, ntimeouts;
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec+t.tv_nsec/1e9; }
#if OBJ == 8
static int valid(const char *s) { if(strlen(s)!=3) return 0; for(int i=0;i<3;i++) if(!strchr(alphabet,s[i]))return 0; return 1; }
#endif
static void fresh(char out[4]) {for(int i=0;i<3;i++)out[i]=alphabet[rand()%62];out[3]=0;}
static void generate(char out[4]) {if(!nseeds){fresh(out);return;}memcpy(out,seeds[rand()%nseeds],4);out[rand()%3]=alphabet[rand()%62];}
static void add_seed(const char s[4]) {for(int i=0;i<nseeds;i++)if(!strcmp(s,seeds[i]))return;if(nseeds<MAX_SEEDS)memcpy(seeds[nseeds++],s,4);else memcpy(seeds[rand()%MAX_SEEDS],s,4);}
static void save_interesting(const char s[4]) {
#if OBJ == 8
  mkdir("interesting",0755);
  static unsigned long long num=0;
  char name[128];snprintf(name,sizeof(name),"interesting/seed_%08llu.txt",++num);
  FILE *fp=fopen(name,"wb");if(fp){fwrite(s,1,3,fp);fclose(fp);}
#else
  (void)s;
#endif
}
static int coverage_increased(void) {
 FILE *fp=fopen(COV_FILE,"rb");if(!fp)return 0;
 size_t n=fread(hits,1,MAX_EDGES,fp);
 int extra=fgetc(fp);
 fclose(fp);
 if(n==0 || extra!=EOF || (edge_count && n!=edge_count))return 0;
 if(!edge_count) edge_count=n;
 int improved=0;
 for(size_t i=0;i<n;i++)if(hits[i]>maxima[i]){if(maxima[i]==0)covered_edges++;maxima[i]=hits[i];improved=1;}
 return improved;
}
static void finding(struct finding *arr,size_t *used,const char input[4],int sig,double elapsed){
 for(size_t i=0;i<*used;i++)if(!strcmp(arr[i].input,input)&&arr[i].signal==sig){arr[i].occurrences++;return;}
 if(*used>=MAX_FINDINGS){fprintf(stderr,"finding capacity exceeded\n");return;}
 struct finding *f=&arr[(*used)++];memcpy(f->input,input,4);f->signal=sig;f->occurrences=1;f->first=elapsed;
 fprintf(stderr,"[%.1fs] NEW %s %s signal=%d\n",elapsed,arr==crashes?"CRASH":"TIMEOUT",input,sig);
}
static void test_one(const char input[4]) {
 unlink(COV_FILE);
 outcome_t o;
 if(run_target(target,input,timeout_ms,&o)!=0){perror("run_target");exec_failures++;return;}
 executions++;
 double elapsed=now()-trial_start;
 if(o.kind==CRASH){total_crashes++;finding(crashes,&ncrashes,input,o.signal_number,elapsed);}
 else if(o.kind==TIMEOUT){total_timeouts++;finding(timeouts,&ntimeouts,input,0,elapsed);}
 else if(o.kind==EXEC_FAIL){exec_failures++;fprintf(stderr,"exec failure errno=%d\n",o.exit_code);}
 else if(coverage_increased()){add_seed(input);save_interesting(input);}
 if(executions%10000==0){double s=now()-trial_start;fprintf(stderr,"[%.0fs] cases=%llu crashes=%llu timeouts=%llu seeds=%d edges=%u rate=%.2f/s\n",s,executions,total_crashes,total_timeouts,nseeds,covered_edges,executions/(s?s:1));fflush(stderr);}
}
#if OBJ == 8
static void load_initial(void){
 DIR *dir=opendir("seeds");if(!dir){perror("seeds directory");exit(2);}
 struct dirent *de;
 while((de=readdir(dir))){
  if(de->d_name[0]=='.')continue;
  char path[1024];snprintf(path,sizeof(path),"seeds/%s",de->d_name);
  struct stat st;if(stat(path,&st)!=0||!S_ISREG(st.st_mode))continue;
  FILE *fp=fopen(path,"rb");if(!fp)continue;
  char b[5]={0};size_t n=fread(b,1,4,fp);fclose(fp);
  if(n!=3||!valid(b)){fprintf(stderr,"Skipping invalid seed %s\n",path);continue;}
  fprintf(stderr,"Initial seed: %s contents=%s\n",path,b);test_one(b);
 }
 closedir(dir);
}
#endif
static void report(double elapsed){
 printf("=== TRIAL COMPLETE ===\nObjective: %d\n",OBJ);
 printf("Per-test-case timeout: %d milliseconds\nRNG seed: %u\n",timeout_ms,rng);
 char host[256];if(gethostname(host,sizeof(host))==0)printf("Rlogin node: %s\n",host);
 printf("Number of test cases: %llu\nWall clock time: %.3f seconds\nTest case throughput: %.3f test cases per second\n",executions,elapsed,executions/elapsed);
 printf("Total crash events: %llu\nDistinct crash-inducing inputs: %zu\nTotal timeout events: %llu\nDistinct timeout-inducing inputs: %zu\nExecution failures: %llu\nEdges covered: %u\n",total_crashes,ncrashes,total_timeouts,ntimeouts,exec_failures,covered_edges);
 for(size_t i=0;i<ncrashes;i++)printf("Crash-inducing test case: %s; signal: %d; occurrences: %llu; first found: %.3f seconds\n",crashes[i].input,crashes[i].signal,crashes[i].occurrences,crashes[i].first);
 for(size_t i=0;i<ntimeouts;i++)printf("Timeout-inducing test case: %s; occurrences: %llu; first found: %.3f seconds\n",timeouts[i].input,timeouts[i].occurrences,timeouts[i].first);
 fflush(stdout);
}
int main(int argc,char **argv){
 if(argc<3){fprintf(stderr,"Usage: %s TARGET TIMEOUT_MS [RNG_SEED] [DURATION_SECONDS]\n",argv[0]);return 2;}
 target=argv[1]; char *end=0;long t=strtol(argv[2],&end,10);
 if(!*argv[2]||*end||t<1||t>3600000){fprintf(stderr,"invalid positive timeout\n");return 2;}timeout_ms=(int)t;
 rng=argc>3?(unsigned)strtoul(argv[3],0,10):(unsigned)(time(NULL)^getpid());srand(rng);
 double duration=argc>4?atof(argv[4]):86400.0;if(duration<=0){fprintf(stderr,"invalid duration\n");return 2;}
 if(access(target,X_OK)!=0){perror(target);return 2;}
 crashes=calloc(MAX_FINDINGS,sizeof(*crashes));timeouts=calloc(MAX_FINDINGS,sizeof(*timeouts));if(!crashes||!timeouts){perror("calloc");return 2;}
 setenv("COV_FILE",COV_FILE,1);
 trial_start=now();
 fprintf(stderr,"Objective %d start target=%s timeout=%d rng=%u duration=%.1f\n",OBJ,target,timeout_ms,rng,duration);
#if OBJ == 8
 load_initial();
#endif
 while(now()-trial_start<duration){char b[4];generate(b);test_one(b);}
 report(now()-trial_start);free(crashes);free(timeouts);return 0;
}
