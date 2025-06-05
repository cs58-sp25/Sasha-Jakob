#include <yuser.h>

int main(int argc, char *argv[]) {
    TracePrintf(0, "Hello, test_exec!\n");
    TracePrintf(0, "argc=%d.\n", argc);
    if (argc!=2){
        TracePrintf(1, "Sorry, expected 2 arguments");
    }

    int argument = atoi(argv[1]);
    TracePrintf(0, "The value %d was passed in.\n", argument);
    TracePrintf(0, "Will delay for 3 ticks\n");
    Delay(3);

    TracePrintf(0, "Done delaying, exiting with status 42\n");
    Exit(42);
}
