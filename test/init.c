#include <yuser.h>

int main(void) {
    TracePrintf(0, "Hello, init!\n");

    TracePrintf(0, "Will delay for 3 ticks\n");
    Delay(3);
    TracePrintf(0, "Back from delay\n");

    TracePrintf(0, "NOW TESTING FORK --------------------------------------------------\n");

    int pid = Fork();
    //int pid = 0;
    TracePrintf(1, "The output of fork is %d.\n", pid);

    if (pid == -1) {
        TracePrintf(0, "ERROR: Fork() failed!\n");
        Exit(1);
    } else if (pid == 0) {
        // Child process
        TracePrintf(0, "CHILD: I am the child process\n");
        TracePrintf(0, "CHILD: My PID is %d\n", GetPid());
        TracePrintf(0, "CHILD: Fork() returned %d (should be 0)\n", pid);

        TracePrintf(0, "CHILD: Will delay for 1 tick\n");
        Delay(5);
        TracePrintf(0, "CHILD: Back from child delay\n");

        TracePrintf(0, "CHILD: Exiting with status 42\n");
        Exit(42);
    } else {
        // Parent process
        TracePrintf(0, "PARENT: I am the parent process\n");
        TracePrintf(0, "PARENT: My PID is %d\n", GetPid());
        TracePrintf(0, "PARENT: My child PID is %d\n", pid);

        TracePrintf(0, "PARENT: Will delay for 2 ticks\n");
        Delay(2);
        TracePrintf(0, "PARENT: Back from parent delay\n");

        // Wait for child to complete
        int status;
        int child_pid = Wait(&status);
        TracePrintf(0, "PARENT: Child %d exited with status %d\n", child_pid, status);
    }

    TracePrintf(0, "PID %d: Testing invalid delay\n", GetPid());
    int rc = Delay(-1);  // This should not work and return with an error
    if (rc != -1) {
        TracePrintf(0, "PID %d: Delay returned %d instead of -1\n", GetPid(), rc);
        Exit(1);
    }
    TracePrintf(0, "PID %d: Delay returned -1 as expected\n", GetPid());

    Delay(0);  // Should return immediately

    int my_pid = GetPid();
    TracePrintf(0, "PID %d: Final PID check\n", my_pid);
    Exit(0);
}
