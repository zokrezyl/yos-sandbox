// test3_multiple.c - multiple sequential forks from parent

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
    prints("TEST3: Multiple forks\n");

    int c1 = fork();
    if (c1 == 0) {
        prints("CHILD1 pid="); printi(getpid()); prints("\n");
        exit(0);
    }
    prints("PARENT: first fork returned "); printi(c1); prints("\n");

    int c2 = fork();
    if (c2 == 0) {
        prints("CHILD2 pid="); printi(getpid()); prints("\n");
        exit(0);
    }
    prints("PARENT: second fork returned "); printi(c2); prints("\n");

    int c3 = fork();
    if (c3 == 0) {
        prints("CHILD3 pid="); printi(getpid()); prints("\n");
        exit(0);
    }
    prints("PARENT: third fork returned "); printi(c3); prints("\n");

    prints("PARENT DONE children="); printi(c1); prints(","); printi(c2); prints(","); printi(c3); prints("\n");
    exit(0);
}
