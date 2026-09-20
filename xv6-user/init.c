// init: The initial user-level program

#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/file.h"
#include "kernel/include/fcntl.h"
#include "xv6-user/user.h"

// char *argv[] = { "sh", 0 };
char *cmds[] = {
  "getcwd",
  "write",
  "getpid",
  "times",
  "uname",
};
static int cnt = sizeof(cmds) / sizeof(cmds[0]);

int
main(void)
{
  int pid, wpid;

  // if(open("console", O_RDWR) < 0){
  //   mknod("console", CONSOLE, 0);
  //   open("console", O_RDWR);
  // }
  dev(O_RDWR, CONSOLE, 0);
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    // starting shell
    // printf("init: starting sh\n");
    // pid = fork();
    // if(pid < 0){
    //   printf("init: fork failed\n");
    //   exit(1);
    // }
    // if(pid == 0){
    //   exec("sh", argv);
    //   printf("init: exec sh failed\n");
    //   exit(1);
    // }

    // running tests
    for(int i = 0; i < cnt; i++){
      // printf("init: running %s\n", cmds[i]);
      pid = fork();
      if(pid < 0){
        printf("init: fork faild\n");
        // parent process must use shutdown
        shutdown();
      }
      if(pid == 0){
        char *argv[] = {cmds[i], 0};
        exec(cmds[i], argv);
        printf("init: exec %s failed\n", cmds[i]);
        // child process can use exit
        exit(1);
      }

      // parent waits until test finishes
      // to make sure tests are sequential
      for(;;){
        // this call to wait() returns if the shell exits,
        // or if a parentless process exits.
        wpid = wait((int *) 0);
        if(wpid == pid){
          // the shell exited; restart it.
          break;
        } else if(wpid < 0){
          printf("init: wait returned an error\n");
          shutdown();
        } else {
          // it was a parentless process; do nothing.
        }
      }
      
      // printf("init: %s finished\n", cmds[i]);
    }

    shutdown();

    // shutdown() should never return
    for(;;);
  }
}
