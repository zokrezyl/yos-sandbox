// test5_loop.c - fork in a loop

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
    prints("TEST5: Fork in loop\n");

    for (int i = 0; i < 5; i++) {
        int pid = fork();
        if (pid == 0) {
            prints("CHILD i="); printi(i); prints(" pid="); printi(getpid()); prints("\n");
            exit(0);
        }
        prints("PARENT: forked child "); printi(pid); prints(" at i="); printi(i); prints("\n");
    }

    prints("PARENT DONE\n");
    exit(0);
}
