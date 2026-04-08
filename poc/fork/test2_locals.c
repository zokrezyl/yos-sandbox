// test2_locals.c - verify local variables preserved after fork

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
    prints("TEST2: Local variables\n");

    int a = 111;
    int b = 222;
    int c = 333;

    int pid = fork();

    int pass = (a == 111 && b == 222 && c == 333);

    if (pid == 0) {
        prints("CHILD: a="); printi(a); prints(" b="); printi(b); prints(" c="); printi(c); prints("\n");
        if (pass) prints("TEST2 CHILD PASS\n"); else prints("TEST2 CHILD FAIL\n");
        exit(pass ? 0 : 1);
    } else {
        prints("PARENT: a="); printi(a); prints(" b="); printi(b); prints(" c="); printi(c); prints("\n");
        if (pass) prints("TEST2 PARENT PASS\n"); else prints("TEST2 PARENT FAIL\n");
        exit(pass ? 0 : 1);
    }
}
