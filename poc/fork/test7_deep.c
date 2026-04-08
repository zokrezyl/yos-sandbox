// test7_deep.c - deep nested forks (child forks child forks child...)

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
    int depth = 0;
    int max_depth = 10;

    while (depth < max_depth) {
        int pid = fork();
        if (pid == 0) {
            // Child continues deeper
            depth++;
        } else {
            // Parent prints and exits
            prints("depth="); printi(depth); prints(" pid="); printi(getpid());
            prints(" child="); printi(pid); prints("\n");
            exit(0);
        }
    }
    // Deepest child
    prints("DEEPEST depth="); printi(depth); prints(" pid="); printi(getpid()); prints("\n");
    if (depth == max_depth) prints("TEST7 PASS\n");
    else prints("TEST7 FAIL\n");
    exit(0);
}
