// test6_stress.c - stress test: 100 forks in loop

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
    int count = 0;
    for (int i = 0; i < 100; i++) {
        int pid = fork();
        if (pid == 0) {
            exit(0);  // Child exits immediately
        }
        if (pid > 0) count++;
    }
    prints("PARENT forked "); printi(count); prints(" children\n");
    if (count == 100) prints("TEST6 PASS\n");
    else prints("TEST6 FAIL\n");
    exit(0);
}
