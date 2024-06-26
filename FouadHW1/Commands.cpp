#include <unistd.h>
// #include <string.h>
// #include <iostream>
// #include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include "Commands.h"
#include <pwd.h>
#include <grp.h>
#include <regex>

#include <algorithm>
#include <fstream>

#include <fcntl.h> // open
#include <dirent.h>

// for the use getdents in listdir
#include <dirent.h> /* Defines DT_* constants */
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <sys/types.h>
#include <unistd.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string COMPLEX = "*?";


#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    //args[i] = (char*)malloc(s.length()+1);
    args[i] = new char[s.length()+1];
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    //args[++i] = NULL;
    args[++i] = nullptr;
  }
  return i;

  FUNC_EXIT()
}


bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool isNumber(char* arg){
    try {
        stoi(arg);
    } catch (...){
        return false;
    }
    return true;
}

bool KillFormat(char* sig, char* job){
    if(!isNumber(sig) || !isNumber(job)){
        return false;
    }
    int signum = stoi(sig);
    if(signum >= 0){
        return false;
    }
    return true;
}

bool isOctal(const char* s)
{
    if (*s == '\0')
        return false;
    if (*s == '0')
        s++;
    while (*s != '\0') {
        if (!isdigit(*s) || *s > '7')
            return false;
        s++;
    }
    return true;
}
bool ChmodFormat(char* mode/*, char* path*/){
    if(!isOctal(mode))
        return false;
    return true;
}

bool validAlias(const string& command){
    const regex pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
    return regex_match(command, pattern);
}

bool reservedAlias(const string& alias){
    SmallShell& shell = SmallShell::getInstance();

    if((alias == "chprompt") || (alias == "showpid") || (alias == "pwd") || (alias == "cd") || (alias == "jobs") ||
            (alias == "fg") || (alias == "quit") || (alias == "kill") || (alias == "alias") || (alias == "unalias") ||
            (alias == ">") || (alias == ">>") || (alias == "|") || (alias == "listdir") || (alias == "getuser") || (alias == "watch")){
        return true;
    }

    return shell.aliases.find(alias)!=shell.aliases.end();
}

// TODO: Add your implementation for classes in Commands.h

Command::Command(const char *cmd_line) {
    command = cmd_line;
}

SmallShell::SmallShell() {
// TODO: add your implementation
prompt = "smash> ";
jobsList = new JobsList();
smashPID = getpid();
current_jobID = 1;
current_PID = -1;
}

SmallShell::~SmallShell() {
// TODO: add your implementation
delete jobsList;
}

JobsList::~JobsList() {
    delete jobsVector;
}

/////////////////////////////////

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
//    string str = string(cmd_line);
    char* temp = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(temp, cmd_line);
    _removeBackgroundSign(temp);
    this->command = temp;
}

//-------------------------------------------- PIPE ----------------------------------//


//remove & sign
PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    string str(cmd_line);
    int place;
    errFlag=false;
    if (_isBackgroundComamnd(str.c_str())) {
        char tmp[COMMAND_ARGS_MAX_LENGTH]{0};
        strcpy(tmp, cmd_line);
        _removeBackgroundSign(tmp);
        cmd_line = tmp;
    }
    if ((place=str.find("|&")) != -1) {
        command1 = _trim(str.substr(0,place));
        command2 = _trim(str.substr(place+2,str.length()));
    }

    else {
        if ((place=str.find("|")) != -1){
            errFlag=true;
            command1 = _trim(str.substr(0,place));
            command2= _trim(str.substr(place+1,str.length()));
        }
    }
}

void PipeCommand::execute() {
    char **args = new char *[COMMAND_ARGS_MAX_LENGTH]{0};
    _parseCommandLine(command, args);
    int my_pipe[2];
    pipe(my_pipe);
    pid_t pid1=fork();
    if (pid1 <0){
        if(close(my_pipe [0])==SYS_FAIL)
        {
            perror("smash error: close failed");
        }
        if(close(my_pipe [1])==SYS_FAIL)
        {
            perror("smash error: close failed");
        }
        perror("smash error: fork failed");
        delete [] args;
        return;
    }
    if(pid1==0){ //child 1
        if (setpgrp()==SYS_FAIL)
        {
            if(close(my_pipe [0])==SYS_FAIL)
            {
                perror("smash error: close failed");
            }
            if(close(my_pipe [1])==SYS_FAIL)
            {
                perror("smash error: close failed");
            }
            perror("smash error: setpgrp failed");
            delete [] args;
            return;
        }
        if(errFlag){ // if |
            if(close(my_pipe [0])==SYS_FAIL)
            {
                perror("smash error: close failed");
                delete [] args;
                return;
            }
            if (dup2(my_pipe[1],1)==SYS_FAIL){
                if(close(my_pipe [0])==SYS_FAIL)
                {
                    perror("smash error: close failed");
                }
                if(close(my_pipe [1])==SYS_FAIL)
                {
                    perror("smash error: close failed");
                }
                perror("smash error: dup2 failed");
                delete [] args;
                return;
            }
        }
        else{ // if |&

            if(close(my_pipe[0])==SYS_FAIL)
            {
                perror("smash error: close failed");
                delete [] args;
                return;
            }

            if (dup2(my_pipe[1],2)==SYS_FAIL){
                if(close(my_pipe [0])==SYS_FAIL)
                {
                    perror("smash error: close failed");
                }
                if(close(my_pipe [1])==SYS_FAIL)
                {
                    perror("smash error: close failed");
                }
                perror("smash error: dup2 failed");
                delete [] args;
                return;
            }
        }


        SmallShell  &shell =SmallShell ::getInstance();
        shell.executeCommand(command1.c_str());
        delete [] args;
        exit(0);
    }
    pid_t pid2= fork();
    if (pid2 <0){
        if(close(my_pipe [0])==SYS_FAIL)
        {
            perror("smash error: close failed");
        }
        if(close(my_pipe [1])==SYS_FAIL)
        {
            perror("smash error: close failed");
        }
        perror("smash error: fork failed");
        delete [] args;
        return;
    }
    if(pid2==0){ //child 2
        if (setpgrp()==SYS_FAIL)
        {
            if(close(my_pipe [0])==SYS_FAIL)
            {
                perror("smash error: close failed");
            }
            if(close(my_pipe [1])==SYS_FAIL)
            {
                perror("smash error: close failed");
            }
            perror("smash error: setpgrp failed");
            delete [] args;
            return;
        }
        if(close(my_pipe[1])==SYS_FAIL)
        {
            perror("smash error: close failed");
            delete [] args;
            return;
        }
        if( dup2(my_pipe[0],0)==SYS_FAIL){
            if(close(my_pipe [0])==SYS_FAIL)
            {
                perror("smash error: close failed");
            }
            perror("smash error: dup2 failed");
            delete [] args;
            return;
        }
        SmallShell  &shell =SmallShell ::getInstance();
        shell.executeCommand(command2.c_str());
        delete [] args;
        exit(0);
    }
    delete [] args;
    //closing the pipe for the parent
    if(close(my_pipe [0])==SYS_FAIL)
    {
        perror("smash error: close failed");
    }
    if(close(my_pipe [1])==SYS_FAIL)
    {
        perror("smash error: close failed");
    }
    if (waitpid(pid1,nullptr,WUNTRACED)==SYS_FAIL){
        perror("smash error: waitpid failed");
    }
    if (waitpid(pid2,nullptr,WUNTRACED)==SYS_FAIL){
        perror("smash error: waitpid failed");
        return;
    }
}

PipeCommand::~PipeCommand() {}

//------------------------------------------------------------------------------------//


Command::~Command() noexcept {

}


//----------------------------------EXTERNAL----------------------------------//
ExternalCommand::ExternalCommand(const char *cmd_line, const char* alias) : Command(cmd_line), alias(alias) {}

void ExternalCommand::execute() {
    pid_t pid = fork();
    if(pid < 0){
        perror("smash error: fork failed");
        return;
    }

    char* tmp = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(tmp,command);
    if(_isBackgroundComamnd(tmp)){
        _removeBackgroundSign(tmp);
    }
    char flag[] = "-c";
    char bashDir[] = "/bin/bash";
    char** argv= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    _parseCommandLine(tmp, argv);
    int status = 0;
    if(pid == 0){ //child
        if(setpgrp() == SYS_FAIL){
            perror("smash error: setpgrp failed");
            delete[] tmp;
            delete[] argv;
            return;
        }

        const string s = string(command);
        if(s.find('*') != string::npos || s.find('?') != string::npos) {
            char* complex_argv[] = {bashDir, flag, tmp, nullptr};
            //Complex

            if(execv(bashDir, complex_argv) == SYS_FAIL){
                perror("smash error: execv failed");
                delete[] argv;
                delete[] tmp;
                exit(0);
            }
        } else{
            //Simple
            if(execvp(argv[0], argv) == SYS_FAIL){
                perror("smash error: execvp failed");
                delete[] argv;
                delete[] tmp;
                exit(0);
            }
        }
    }
    else{  //parent
        if(!_isBackgroundComamnd(command)) {
            SmallShell& smash = SmallShell::getInstance();

            smash.current_PID = pid;
            if(waitpid(pid, &status, WUNTRACED ) == SYS_FAIL  || status==pid){
                perror("smash error: waitpid failed");
                delete[] argv;
                delete[] tmp;
                return;
            }
            smash.current_PID = -1;
            delete [] argv;
            delete[] tmp;
        }
        else {
            Command* realCommand;
            char* origCommand = new char[COMMAND_MAX_ARGS];
            if(alias){
                strcpy(origCommand, alias);
                realCommand = new ExternalCommand(tmp, nullptr);
            } else{
                strcpy(origCommand, command);
                realCommand = this;
            }
            if(waitpid((pid),&status,WNOHANG)!=pid) {
                SmallShell& smash = SmallShell::getInstance();
                smash.getJobsList()->addJob(realCommand, pid, origCommand, false);
                //smash.printJobsVector();
                delete [] argv;
                delete[] tmp;
                return;
            }
            else{
                delete [] argv;
                delete[] tmp;
                return;
            }
        }
    }
}
//----------------------------------------------------------------------//


JobsList::JobEntry::JobEntry(int jobID, Command *jobCommand, pid_t processID, const char* entryCommand) {
    this->jobId = jobID;
    this->jobCommand = jobCommand;
    this->processID = processID;
    this->jobEntryCommand = entryCommand;
    isStopped = false;
}

//----------------------------------------------REDIRECTION---------------------------------//

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    _parseCommandLine(command, args);

    sub_cmd = new char[COMMAND_ARGS_MAX_LENGTH];
    output_file = new char[COMMAND_ARGS_MAX_LENGTH];

    string cmd_s = _trim(string(command));
    string subCommand = _trim(cmd_s.substr(0, cmd_s.find_first_of(">")));
    string fileString = _trim(cmd_s.substr(cmd_s.find_last_of(">") + 1, cmd_s.length() - 1));

    strcpy(sub_cmd, subCommand.c_str());
    strcpy(output_file, fileString.c_str());


    if(cmd_s.find(">>") != string::npos){
        override = false;
    } else{
        override = true;
    }
    delete[] args;
}

void RedirectionCommand::redirectionAux(std::ofstream& output){
    SmallShell& smash = SmallShell::getInstance();

    streambuf *originalCoutBuffer = std::cout.rdbuf();
    cout.rdbuf(output.rdbuf());
    if(output.is_open()){
        smash.executeCommand(sub_cmd);
        output.close();
    } else{
        perror("smash error: open failed");
    }
    cout.rdbuf(originalCoutBuffer);
}

void RedirectionCommand::execute() {
    if(override){
        //override >
        ofstream  output(output_file);
        redirectionAux(output);
    }
    else
    {
        // append   >>
        ofstream output(output_file, ios::app);
        redirectionAux(output);
    }
}

//------------------------------------------------------------------------------------------//

//----------------------------------------------SHOWPID---------------------------------//
ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()  {
    pid_t pid  = getpid();
    cout << "smash pid is " << pid << endl;
}
//------------------------------------------------------------------------------------------//


//-------------------------------------------PWD------------------------------------------//

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void GetCurrDirCommand::execute()  {
    char buffer[COMMAND_ARGS_MAX_LENGTH];

    // Attempt to get the current working directory
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        // Display the current working directory
        cout << buffer << endl;
    }
    else{
        perror("smash error: chdir failed");
    }
}
//------------------------------------------------------------------------------------------//

//---------------------------------------COMMAND--------------------------------------------//
JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line) {
    SmallShell&  smash = SmallShell::getInstance();
    jList = smash.getJobsList();
}


//TIMER
void JobsCommand::execute()  {
    jList->removeFinishedJobs();
    SmallShell&  smash = SmallShell::getInstance();
    //cout<<"Jobs here"<<endl;
    smash.printJobsVector();
}
//------------------------------------------------------------------------------------------//


//-------------------------FOREGROUND--------------------------------//
ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jList(jobs) {}

void ForegroundCommand::execute()  {
//    jList->removeFinishedJobs();
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int n = _parseCommandLine(command, args);
    int jID = 0;

    if(n==1 && jList->jobsVector->empty())
    {
         cerr << "smash error: fg: jobs list is empty" << endl;
         delete[] args;
         return;
    }
    if(n==1)
    {
          jID = jList->getMaxJob()->getJobID();
    }
    else
    {
        //checking format
        if(!isNumber(args[1])){
            cerr << "smash error: fg: invalid arguments" << endl;
            delete[] args;
            return;
        }
        jID = stoi(args[1]);
        if(jList->getJobById(jID) == nullptr){
            cerr<<"smash error: fg: job-id "<<jID<<" does not exist"<<endl;
            delete[] args;
            return;
        }

        if(n>2){
            cerr << "smash error: fg: invalid arguments" << endl;
            delete[] args;
            return;
        }
    }

    JobsList::JobEntry* job = jList->getJobById(jID);
    pid_t jPID = job->getProcessID();
    if(job->getIsStopped())
    {
        kill(jPID,SIGCONT);
        job->setIsStopped(false);
    }

    //update current
    SmallShell& smash = SmallShell::getInstance();
    smash.current_PID = jPID;
    smash.current_jobID = jID;

    cout << job->getEntryCommand() << " " << jPID <<endl;
    if(waitpid(jPID, nullptr, WUNTRACED) == SYS_FAIL){
        perror("smash error: waitpid failed");
        delete[] args;
        return;
    }
    jList->removeJobById(jID);
    delete[] args;
}
//-------------------------------------------------------------------//


//-----------------------------QUIT---------------------------------//

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jList(jobs){}

void QuitCommand::execute() {
    jList->removeFinishedJobs();
    char *args[COMMAND_ARGS_MAX_LENGTH];
    int num = _parseCommandLine(command, args);

    if (num >= 2 ) {
        string second = args[1];
        if (second != "kill"){
            //delete this;
            exit(0); // the first word after quit is not kill
        }
        else{
            vector<JobsList::JobEntry*>* jVector = jList->jobsVector;
            cout << "smash: sending SIGKILL signal to " << jVector->size() << " jobs:" << endl;
            for (vector<JobsList::JobEntry*>::iterator it = jVector->begin(); it != jVector->end(); ++it) {

                cout << (*it)->getProcessID()<<": " << (*it)->getEntryCommand() << endl;
            }
            jList->killAllJobs();
            //delete this;
            exit(0);
        }
    }
    else{
        //delete this;
        exit(0);
    }
}

QuitCommand::~QuitCommand() noexcept {}
//-------------------------------------------------------------------//


//-----------------------------KILL---------------------------------//
KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jList(jobs) {}

void KillCommand::execute()  {
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int n = _parseCommandLine(command, args);

    if(!isNumber(args[2])){
        cerr << "smash error: kill: invalid arguments" << endl;
        delete[] args;
        return;
    }
    int jobID = stoi(args[2]);
    if(jList->getJobById(jobID) == nullptr){
        cerr<<"smash error: kill: job-id "<< jobID << " does not exist"<<endl;
        delete[] args;
        return;
    }

    if(!KillFormat(args[1], args[2])){
        cerr << "smash error: kill: invalid arguments" << endl;
        delete[] args;
        return;
    }
    if(n!=3){
        cerr << "smash error: kill: invalid arguments" << endl;
        delete[] args;
        return;
    }

    SmallShell& smash = SmallShell::getInstance();
    JobsList::JobEntry* killedJob = jList->getJobById(jobID);
    pid_t jobProcessID = killedJob->getProcessID();
    int signum  = stoi(args[1]) * (-1);

    if(signum == SIGCONT) {
        if (killedJob->getIsStopped()){
            smash.current_PID = jobProcessID;
            smash.current_jobID = jobID;
//            smash.was_in_jobs=true;
            jList->removeJobById(jobID);
        }
    }
    else if(signum==SIGSTOP) {
        killedJob->setIsStopped(true);
//        smash.was_in_jobs=false;
        smash.current_PID = -1;
        smash.current_jobID = -1;
    }

    if (kill(jobProcessID, signum) == 0) {
        cout << "signal number " << signum << " was sent to pid " << jobProcessID << endl;
    }
    else { //syscall failed
        perror("smash error: kill failed");
        delete[] args;
        return;
    }
    delete[] args;
}
//--------------------------------------------------------------//


void Command::printCommand() const {
    cout << command <<endl;
}


const char* Command::getCommand() const {
    return command;
}

void JobsList::addJob(Command *cmd, pid_t processID,  const char* entryCommand, bool isStopped) {
    int newJobID = 1;
    if(maxJob)
    {
        newJobID = maxJob->getJobID() + 1;
    }
    SmallShell& smash = SmallShell::getInstance();
    removeFinishedJobs();
    JobEntry* newJob = new JobEntry(newJobID, cmd, processID, entryCommand);
    smash.getJobsList()->jobsVector->push_back(newJob);
    maxJob = newJob;
    newJob->setIsStopped(isStopped);
}


void JobsList::printJobsList() {
    for (vector<JobsList::JobEntry*>::iterator it = jobsVector->begin(); it != jobsVector->end(); ++it) {
        cout << "[" << (*it)->getJobID() << "] " << (*it)->getProcessID() << "  " << (*it)->getEntryCommand() << endl;

    }
}


size_t JobsList::jobsCount() {
    return jobsVector->size();
}

JobsList::JobEntry* JobsList::getMaxJob() {
    return maxJob;
}

void JobsList::updateMaxJob() {
    int maxID = -1;
    for (vector<JobsList::JobEntry*>::iterator it = jobsVector->begin(); it != jobsVector->end(); ++it) {
        if((*it)->getJobID() > maxID){
            maxID = (*it)->getJobID();
            maxJob = (*it);
        }
    }
}

void JobsList::killAllJobs() {
    for (vector<JobsList::JobEntry*>::iterator it = jobsVector->begin(); it != jobsVector->end(); ++it) {
        kill((*it)->getProcessID(), SIGKILL);
    }
    updateMaxJob();
}

void JobsList::removeFinishedJobs() {
    if(jobsVector->empty()){
        maxJob = nullptr;
        return;
    }
    int ret = 0;
    int status = 0;

    for(vector<JobEntry*>::iterator it = jobsVector->begin(); it!=jobsVector->end();it++)
    {
        ret=waitpid((*it)->getProcessID(),&status,WNOHANG);
        if(ret==(*it)->getProcessID())
        {
            jobsVector->erase(it);
            --it;
        }
    }
    if (jobsVector->empty()){
        maxJob = nullptr;
        return;
    }
    updateMaxJob();
}

JobsList::JobsList() {
    jobsVector = new vector<JobEntry*>();
    maxJob = nullptr;
}

JobsList::JobEntry* JobsList::getLastJob() {
    //return getJobById(*lastJobId);
    if(!jobsVector->empty()){
        return jobsVector->back();
    }
    return nullptr;
}

JobsList::JobEntry* JobsList::getLastStoppedJob() {
    JobEntry* entry = nullptr;
    for (vector<JobsList::JobEntry*>::iterator it = jobsVector->begin(); it != jobsVector->end(); ++it) {
        if((*it)->getIsStopped()){
            entry = (*it);
        }
    }
    return entry;
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
    for(JobEntry* job : *jobsVector)
        if(job->getJobID() == jobId)
            return job;
    return nullptr;
}

void JobsList::removeJobById(int jobId)
{
    JobsList::JobEntry* job = getJobById(jobId);
    auto it = std::find(jobsVector->begin(),jobsVector->end(),job);
    if(it != jobsVector->end())
        jobsVector->erase(it);

    //update maxJob
    updateMaxJob();
}

//------------------------------------------CD------------------------------//
ChangeDirCommand::ChangeDirCommand (const char* cmd_line, char** plastPwd): BuiltInCommand(cmd_line), plastPwd(plastPwd) {}

void ChangeDirCommand::execute(){
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int num = _parseCommandLine(command, args);
    if (num > 2) {
        cerr<< "smash error: cd: too many arguments" << endl;
        delete[] args;
    }
    else if (num != 1)
    {
        char *buffer = new char[MAX_PATH_LENGTH];
        if (getcwd(buffer, MAX_PATH_LENGTH) == nullptr)
        {
            perror("smash error: chdir failed");
            delete[] args;
            delete[] buffer;
            return;
        }
        string first= args[1];
        if( first == "-"){
            if (*plastPwd == nullptr){
                cerr << "smash error: cd: OLDPWD not set" << endl;
                delete[] args;
                delete[] buffer;
                return;
            }
            else{
                if (chdir(*plastPwd)==SYS_FAIL){
                    perror("smash error: chdir failed");
                    delete[] args;
                    delete[] buffer;
                    return;
                }
                //delete plastPwd;
                *plastPwd=buffer;
            }
        }
        else{
            if (chdir(args[1])==SYS_FAIL) {
                perror("smash error: chdir failed");
                delete[] args;
                return;
            }
            //delete plastPwd;
            *plastPwd=buffer;
        }
    }
}
//-------------------------------------------------------------------------//


//--------------------------------CHPROMPT-------------------------------//
Chprompt::Chprompt(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void Chprompt::execute() {
    SmallShell& shell = SmallShell::getInstance();
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
    int len = _parseCommandLine(command, args);
    if(len == 1){
        shell.setPrompt("smash");
    }
    else{
        shell.setPrompt(args[1]);
    }

    delete[] args;
}

Chprompt::~Chprompt() noexcept {}
//----------------------------------------------------------------------//

//---------------------------------CHMOD----------------------------------//
ChmodCommand::ChmodCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void ChmodCommand::execute() {
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int n = _parseCommandLine(command, args);
    if(n != 3 || !ChmodFormat(args[1])){
        cerr << "smash error: chmod: invalid arguments" << endl;
        delete[] args;
        return;
    }
    if (chmod(args[2], (mode_t)(stoi(args[1],nullptr,8))) == SYS_FAIL)
    {
        perror("smash error: chmod failed");
        delete[] args;
        return;
    }
    delete[] args;

}
//----------------------------------------------------------------------//




//---------------------------------UNALIAS----------------------------------//

unaliasCommand::unaliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void unaliasCommand::execute()
{
    SmallShell& shell = SmallShell::getInstance();
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int n = _parseCommandLine(command, args);

    // just the command name
    if (n <= 1) {
        cerr << "smash error: unalias: not enough arguments" << endl;
    }
    else
    {
        for (int i = 1; i <= n-1; i++)
        {
            std::map<std::string, std::string>::iterator it =  shell.aliases.find(std::string(args[i]));
            if (it == shell.aliases.end())
            {
                cerr << "smash error: unalias: " << args[i] <<  " alias does not exist\n" << endl;
                break;
            }
            else // alias exists
            {
                shell.aliases.erase(it);
                shell.insertion_order.erase(remove(shell.insertion_order.begin(), shell.insertion_order.end(), args[i]), shell.insertion_order.end());
            }
        }
        
    }
}

//----------------------------------------------------------------------//

/////////////////////////////////

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    if(*cmd_line == '\0'){
        return nullptr;
    }
    bool isAlias = false;
    char* cmd = new char[COMMAND_ARGS_MAX_LENGTH];
    cmd = strcpy(cmd, cmd_line);
    string rest;
    if(_isBackgroundComamnd(cmd_line)){
        string cmd_string = string(cmd);

        size_t amp_index = cmd_string.find_last_of('&');

        size_t non_space_index = cmd_string.find_last_not_of(' ', amp_index - 1);

        //cmd = cmd_string.substr(0, non_space_index + 1);

        rest = cmd_string.substr(non_space_index + 1);
        _removeBackgroundSign(cmd);
    }

    string cmd_s = _trim(string(cmd));
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
    int len = _parseCommandLine(cmd, args);

    string firstWord;
    string alias_command;
    auto it = aliases.find(args[0]);
    if(it != aliases.end()){
        isAlias = true;
        alias_command = args[0];
        string command = it->second;
        char **inner_args= new char* [COMMAND_ARGS_MAX_LENGTH];
        int inner_len = _parseCommandLine(command.c_str(), inner_args);
        if (inner_len > 1)
        {
            firstWord = inner_args[0];
            cmd_s = command;
        }
        else 
        {
            firstWord = string(inner_args[0]);
            cmd_s = firstWord;
            for(int i = 1; i < len; i++) {
                alias_command += " ";
                cmd_s += " ";
                cmd_s += string(args[i]);
                alias_command += string(args[i]);
            }
        }
    }
    else{
        firstWord = string(cmd_line).substr(0, cmd_s.find_first_of(" \n"));
    }
    if(_isBackgroundComamnd(cmd_line)){
        cmd_s += rest;
        alias_command += rest;
    }

    delete[] cmd;
    char* temp = new char[COMMAND_ARGS_MAX_LENGTH];
    char* tempAlias = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(temp,cmd_s.c_str());
    strcpy(tempAlias,alias_command.c_str());

  if(strstr(cmd_line, "|") != nullptr || strstr(cmd_line, "|&") != nullptr){
      return new PipeCommand(temp);
  }
  if(strstr(cmd_line, ">") != nullptr || strstr(cmd_line, ">>") != nullptr){
      return new RedirectionCommand(temp);
  }

  if(firstWord == "alias"){
      return new aliasCommand(temp);
  }
  else if (firstWord == "unalias"){
      return new unaliasCommand(temp);
  }
  else if(firstWord == "chprompt"){
      return new Chprompt(temp);
  }
  else if(firstWord == "showpid"){
      return new ShowPidCommand(temp); //DONE
  }
  else if(firstWord == "pwd"){
      return new GetCurrDirCommand(temp); //DONE
  }
  else if(firstWord == "cd"){
      return new ChangeDirCommand(temp, &lastpwd); //DONE
  }
  else if(firstWord == "jobs"){
      return new JobsCommand(temp, jobsList);  //WAIT
  }
  else if(firstWord == "fg") {
      return new ForegroundCommand(temp, jobsList);  //80% DONE
  }
  else if(firstWord == "quit"){
      return new QuitCommand(temp, jobsList); //DONE
  }
  else if(firstWord == "kill"){
      return new KillCommand(temp, jobsList);  //DONE
  }
  else if(firstWord == "chmod"){
      return new ChmodCommand(temp);  //DONE
  }
  else if(firstWord == "listdir"){
      return new ListDirCommand(temp);
  }
  else if(firstWord == "getuser"){
      return new GetUserCommand(temp);
  }
  else{
    if(isAlias){
        return new ExternalCommand(temp, tempAlias); //DONE
    }
    return new ExternalCommand(temp, nullptr);
  }

  return nullptr;
}

void SmallShell::printJobsVector() {

    for (auto& je: *jobsList->jobsVector)
        std::cout << "[" << je->getJobID() << "] " << je->getEntryCommand() << endl;

}

void SmallShell::executeCommand(const char *cmd_line) {
    jobsList->removeFinishedJobs();
    char* temp = new char[COMMAND_ARGS_MAX_LENGTH];
    strcpy(temp,cmd_line);
    Command* command = CreateCommand(temp);


  if(command){
      command->execute();
      delete command;
  }
  //delete[] temp;
}

//---------------------------------LISTDIR----------------------------------//

ListDirCommand::ListDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

// max sort, i have no energy to write a more complex sorting function :)
//nigga who cares about a complex sorting function, 💀
void sort_vectors_alphabetically(vector<string>& vec)
{
    size_t vec_size = vec.size();
    for (size_t i = vec_size - 1; i > 0; i--) // i > 0 , cuz if we reach element 0 it is already in its place
    {
        size_t max = 0;
        for (size_t j = 0; j < i; ++j)
            if (vec[j + 1] > vec[max])
                max = j + 1;

        std::swap(vec[i], vec[max]);
    }
}

// The linux_dirent structure is declared as follows:
struct linux_dirent
{
    unsigned long d_ino;
    off_t d_off;
    unsigned short d_reclen;
    char d_name[];
};

// in https://piazza.com/class/lwp40o5qxt33t6/post/106 they decided to drop support for links
// only print the files and subdirectories
void ListDirCommand::execute()
{
    //const size_t FILES_LIMIT = 100; // 100 files/subdirectories is the max number
    //SmallShell &shell = SmallShell::getInstance();
    char **args = new char *[COMMAND_ARGS_MAX_LENGTH];
    int num = _parseCommandLine(command, args);

    string path; // `path` holds the path of the dir to list its contents
    if (num > 2) // more than 1 argument
    {
        cerr << "smash error: listdir: too many arguments" << endl;
        return;
    }


    // i got this from an example in https://man7.org/linux/man-pages/man2/getdents.2.html
    const size_t BUF_SIZE = 4096;
    // deduce the path
    path = (num == 2 ? args[1] : ".");
    int fd;
    char d_type;
    char buf[BUF_SIZE];
    int nread;
    struct linux_dirent *d;

    fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1){
        perror("smash error: open failed");
        return;
    }

    vector<string> files;
    vector<string> directories;
    // vector<string> symlinks;
    for (;;)
    {
        nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
        if (nread == -1){
            close(fd);
            perror("smash error: getdents failed");
            return;
        }

        if (nread == 0)
            break;

        for (int bpos = 0; bpos < nread;)
        {
            d = (struct linux_dirent *)(buf + bpos);
            d_type = *(buf + bpos + d->d_reclen - 1);

            const string name = d->d_name;
            if (name.size() && name[0] != '.')
            {
                if (d_type == DT_REG) // regular file
                {
                    files.push_back(name);
                }
                else if (d_type == DT_DIR) // directory
                {
                    directories.push_back(name);
                }
                // else if (d_type == DT_LNK) // symlink
                // {
                //     symlinks.push_back(name);
                // }
                else
                {
                    // should not be tested
                }
            }

            bpos += d->d_reclen;
        }
    }
    close(fd);

    sort_vectors_alphabetically(files);
    sort_vectors_alphabetically(directories);
    // sort_vectors_alphabetically(symlinks);

    for (const string &file_name : files)
    {
        std::cout << "file: " << file_name << std::endl;
    }

    for (const string &dir_name : directories)
    {
        std::cout << "directory: " << dir_name << std::endl;
    }

    // prolly more complex than this simple print, in piazza they dropped support
    // for (const string &link_name : symlinks) 
    // {
    //     std::cout << "link: " << link_name << std::endl;
    // }
}

//---------------------------------ALIAS----------------------------------//

aliasCommand::aliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line){
}

void aliasCommand::execute(){
    SmallShell& shell = SmallShell::getInstance();
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
    int len = _parseCommandLine(command, args);

    if(len == 1){ //should print all aliases
        for(const auto& alias : shell.insertion_order){
            cout<<alias<<"="<<"'"<<shell.aliases[alias]<<"'"<<endl;
        }
        return;
    }

    string str = string(command);
    string alias, original;
    size_t firstLetter = (str.substr(5)).find_first_not_of(" ") + 5;
    size_t firstEqual = str.find_first_of("=");
    size_t lastApostrophe = str.find_last_of("'");
    alias = _trim(str.substr(firstLetter, firstEqual - firstLetter));
    original = str.substr(firstEqual+2, lastApostrophe - firstEqual - 2);

    if(reservedAlias(alias)){
        cerr << "smash error: alias: "<< alias <<" already exists or is a reserved command" << endl;
        return;
    }
    if(!validAlias(str)){
        cerr << "smash error: alias: invalid alias format" << endl;
        return;
    }
    shell.aliases[alias] = original;
    shell.insertion_order.push_back(alias);
}
//--------------------------------------------------------------------------//


//---------------------------------GETUSER----------------------------------//
GetUserCommand::GetUserCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}


string getUsernameFromUID(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return string(pw->pw_name);
    }
    return "";
}

string getUsernameOfProcess(char* number) {
    pid_t pid = stoi(number);
    string statusPath = "/proc/" + to_string(pid) + "/status";
    ifstream statusFile(statusPath);
    if (!statusFile.is_open()) {
        cerr << "smash error: getuser: process " << number << " does not exist" << endl;
        return "";
    }

    string line;
    while (getline(statusFile, line)) {
        if (line.substr(0, 5) == "Uid:\t") {
            istringstream ss(line.substr(5));
            uid_t uid;
            ss >> uid;
            return getUsernameFromUID(uid);
        }
    }
    return "";
}

string getGroupnameFromGID(gid_t gid) {
    struct group *gr = getgrgid(gid);
    if (gr) {
        return string(gr->gr_name);
    }
    return "";
}

string getGroupnameOfProcess(char* number) {
    pid_t pid = stoi(number);
    string statusPath = "/proc/" + to_string(pid) + "/status";
    ifstream statusFile(statusPath);
    if (!statusFile.is_open()) {
        cerr << "smash error: getuser: process "<< number <<" does not exist" << endl;
        return "";
    }

    string line;
    while (getline(statusFile, line)) {
        if (line.substr(0, 5) == "Gid:\t") {
            istringstream ss(line.substr(5));
            gid_t gid;
            ss >> gid;
            return getGroupnameFromGID(gid);
        }
    }
    return "";
}


void GetUserCommand::execute() {
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
    int len = _parseCommandLine(command, args);

    if(len > 2){
        cerr << "smash error: getuser: too many arguments" << endl;
        return;
    }

    if(!isNumber(args[1])){
        cerr << "smash error: getuser: process " << args[1] << " does not exist" << endl;
        return;
    }

   char* userPID = args[1];
    string username = getUsernameOfProcess(userPID);
    string groupname;
    if(!username.empty()){
        groupname = getGroupnameOfProcess(userPID);
    }

    if(!groupname.empty()){
        cout<<"User: "<<username<<endl;
        cout<<"Group: "<<groupname<<endl;
    }

}
//--------------------------------------------------------------------------//

