// test1_simple.c - basic fork, check return values

__attribute__((import_module("env"), import_name("fork"))) int fork(void);
__attribute__((import_module("env"), import_name("write"))) int write(int fd, const void* buf, unsigned int count);
__attribute__((import_module("env"), import_name("exit"))) void exit(int status);
__attribute__((import_module("env"), import_name("getpid"))) int getpid(void);

static void prints(const char* s) { int l=0; while(s[l])l++; write(1,s,l); }
static void printi(int n) {
    if(n==0){write(1,"0",1);return;}
    char b[16];int i=0;
    while(n>0){b[i++]='0'+(n%10);n/=10;}
    while(i>0)write(1,&b[--i],1);
}

void _start(void) {
    prints("TEST1: Simple fork\n");

    int pid = fork();

    if (pid == 0) {
        prints("CHILD: fork returned 0, mypid="); printi(getpid()); prints("\n");
        prints("TEST1 CHILD PASS\n");
        exit(0);
    } else {
        prints("PARENT: fork returned "); printi(pid); prints(", mypid="); printi(getpid()); prints("\n");
        prints("TEST1 PARENT PASS\n");
        exit(0);
    }
}
