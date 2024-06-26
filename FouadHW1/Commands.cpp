#include <unistd.h>
//#include <string.h>
//#include <iostream>
//#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iomanip>
#include "Commands.h"

#include <regex>


#include <algorithm>
#include <fstream>

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
    //TODO: check if reserved word.
    return shell.aliases.find(alias)!=shell.aliases.end();
}

// TODO: Add your implementation for classes in Commands.h 

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
ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    pid_t pid = fork();
    if(pid < 0){
        perror("smash error: fork failed");
        return;
    }

    char tmp[COMMAND_ARGS_MAX_LENGTH]{0};
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
            perror("smash error: setgrp failed");
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
                exit(0);
            }
        } else{
            //Simple

            if(execvp(argv[0], argv) == SYS_FAIL){
                perror("smash error: execvp failed");
                delete[] argv;
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
                return;
            }
            smash.current_PID = -1;
            delete [] argv;
        }
        else {
            if(waitpid((pid),&status,WNOHANG)!=pid) {
                SmallShell& smash = SmallShell::getInstance();
                smash.getJobsList()->addJob(this, pid, command, false);
                //smash.printJobsVector();
                delete [] argv;
                return;
            }
            else{
                delete [] argv;
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

    } else{
        //append   >>
        ofstream  output(output_file, ios::app);
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
        //TODO: error handling
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
    else  if (num !=1)  {
        char* buffer = new char[MAX_PATH_LENGTH];
        if(getcwd(buffer,MAX_PATH_LENGTH) == nullptr){
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
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH]{0};
    int n = _parseCommandLine(command, args);
    
    // just the command name
    if (n <= 1) {
        perror("smash error: unalias: not enough argumets");
    }
    else
    {
        for (int i = 1; i <= n-1; i++)
        {
            std::map<std::string, std::string>::iterator it =  SmallShell::getInstance().aliases.find(std::string(args[i]));
            if (it == SmallShell::getInstance().aliases.end())
            {
                fprintf(stderr, "smash error: unalias: %s alias does not exist", args[i]);
//                perror("smash error: unalias: %s alias does not exist", args[i]);

                break;
            }
            else // alias exists
            {
                SmallShell::getInstance().aliases.erase(it);
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
  char* cmd = new char[COMMAND_ARGS_MAX_LENGTH];
  cmd = strcpy(cmd, cmd_line);
  if(_isBackgroundComamnd(cmd_line)){
      strcpy(cmd, cmd_line);
      _removeBackgroundSign(cmd);
  }


  string cmd_s = _trim(string(cmd));
  char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
  _parseCommandLine(cmd, args);

  delete[] cmd;
  string firstWord;

  if(aliases.find(args[0]) != aliases.end()){
    firstWord = aliases[string(args[0])];
    cmd_s = firstWord + cmd_s.substr(string(args[0]).size());
  }
  else{
      firstWord = string(cmd_line).substr(0, cmd_s.find_first_of(" \n"));
  }


  if(strstr(cmd_line, "|") != nullptr || strstr(cmd_line, "|&") != nullptr){
      return new PipeCommand(cmd_line);
  }
  if(strstr(cmd_line, ">") != nullptr || strstr(cmd_line, ">>") != nullptr){
      return new RedirectionCommand(cmd_line);
  }
  if(firstWord == "alias"){
      return new aliasCommand(cmd_line);
  }
  else if(firstWord == "chprompt" ){
      return new Chprompt(cmd_line);  //DONE
  }
  else if(firstWord == "showpid"){
      return new ShowPidCommand(cmd_line); //DONE
  }
  else if(firstWord == "pwd"){
      return new GetCurrDirCommand(cmd_line); //DONE
  }
  else if(firstWord == "cd"){
      return new ChangeDirCommand(cmd_line, &lastpwd); //DONE
  }
  else if(firstWord == "jobs"){
      return new JobsCommand(cmd_line, jobsList);  //WAIT
  }
  else if(firstWord == "fg") {
      return new ForegroundCommand(cmd_line, jobsList);  //80% DONE
  }
  else if(firstWord == "quit"){
      return new QuitCommand(cmd_line, jobsList); //DONE
  }
  else if(firstWord == "kill"){
      return new KillCommand(cmd_line, jobsList);  //DONE
  }
  else if(firstWord == "chmod"){
      return new ChmodCommand(cmd_line);  //DONE
  }
  else{
      return new ExternalCommand(cmd_line); //DONE
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


aliasCommand::aliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line){
}

void aliasCommand::execute(){
    SmallShell& shell = SmallShell::getInstance();
    char **args= new char* [COMMAND_ARGS_MAX_LENGTH];
    int len = _parseCommandLine(command, args);

    if(len == 1){ //should print all aliases
        for(const auto& pair : shell.aliases){
            cout<<pair.first<<"="<<"'"<<pair.second<<"'"<<endl;
        }
        return;
    }

    string str = string(command);
    string alias, original;
    size_t firstSpace = str.find_first_of(" ");
    size_t firstEqual = str.find_first_of("=");
    size_t lastApostrophe = str.find_last_of("'");
    alias = _trim(str.substr(firstSpace, firstEqual - firstSpace));
    original = _trim(str.substr(firstEqual+2, lastApostrophe - firstEqual - 2));

    if(reservedAlias(alias)){
        fprintf(stderr, "smash error: alias: %s already exists or is a reserved command", alias.c_str());
        return;
    }
    if(!validAlias(str)){
        perror("smash error: alias: invalid alias format");
        return;
    }
    shell.aliases[alias] = original;
}