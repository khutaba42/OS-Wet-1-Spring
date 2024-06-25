#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
//  // TODO: Add your implementation

    SmallShell& smash = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    if(smash.current_PID != -1)
    {
        cout << "smash: process " << smash.current_PID <<" was killed" << endl;
        if(kill(smash.current_PID,SIGKILL)){
            perror("smash error: kill failed");
            return;
        }

        smash.current_PID = -1;
        smash.current_jobID = -1;
    }
    return;
}

