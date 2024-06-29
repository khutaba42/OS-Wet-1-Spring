#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <iostream>
#include <string>
#include <map>



#define COMMAND_ARGS_MAX_LENGTH (200)
#define MAX_PATH_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define SYS_FAIL -1

class Command {
// TODO: Add your data members
protected:
    const char* command;
 public:
    Command(const char *cmd_line);
  Command(const Command&) = delete;
  virtual ~Command();
  virtual void execute() = 0;
  void printCommand() const;
  const char* getCommand() const;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
  std::string command1;
  std::string command2;
  bool errFlag;
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand();
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 char* sub_cmd;
 char* output_file;
 bool override;
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
  void redirectionAux(std::ofstream& output);
};

class Chprompt : public BuiltInCommand{
public:
    Chprompt(const char* cmd_line);
    virtual ~Chprompt();
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
// TODO: Add your data members public:
public:
    char** plastPwd;
  ChangeDirCommand(const char* cmd_line, char** plastPwd);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
// TODO: Add your data members public:
  JobsList *jList;
public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand();
  void execute() override;
};




class JobsList {
 public:
  class JobEntry {
   // TODO: Add your data members
   pid_t processID;
   const char* jobEntryCommand;
   int jobId;
   bool isStopped;
   Command *jobCommand;
  public:
   JobEntry(int jobID, Command* jobCommand, pid_t processID, const char* entryCommand);
   ~JobEntry();
   int getJobID(){
       return jobId;
   }
   pid_t getProcessID(){
       return processID;
   }
   bool getIsStopped()
   {
       return isStopped;
   }
   void setIsStopped(bool status)
   {
       isStopped = status;
   }
   const char* getEntryCommand(){
       return jobEntryCommand;
   }

   Command* getJobCommand(){
       return jobCommand;
   }


   void printDetails()
   {
       std::cout << "processID: " << processID << " jobCommand: " << jobCommand->getCommand() << std::endl;
   }
  };
 // TODO: Add your data members
    JobEntry *maxJob;
    std::vector<JobEntry*>* jobsVector;

 public:
  JobsList();
  ~JobsList();
  JobsList(const JobsList&) = delete;
  void addJob(Command* cmd, pid_t processID, const char* entryCommand, bool isStopped = false); //DONE
  void printJobsList(); //DONE  REVIEW
  void killAllJobs(); //DONE
  void removeFinishedJobs(); //DONE
  JobEntry * getJobById(int jobId);//Done
  void removeJobById(int jobId);//DONE
  JobEntry * getLastJob();//Done
  JobEntry *getLastStoppedJob(); //DONE
  // TODO: Add extra methods or modify exisitng ones as needed
  size_t jobsCount(); //DONE
  void updateMaxJob(); //DONE
  JobEntry* getMaxJob(); //DONE
};

class JobsCommand : public BuiltInCommand {
 // TODO: Add your data members
 JobsList* jList;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 JobsList* jList;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 JobsList* jList;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class ChmodCommand : public BuiltInCommand {
 public:
  ChmodCommand(const char* cmd_line);
  virtual ~ChmodCommand() {}
  void execute() override;
};


class WatchCommand : public Command {
    // TODO: Add your data members
public:
    WatchCommand(const char *cmd_line);

    virtual ~WatchCommand() {}

    void execute() override;
};


class SmallShell {
 private:
  // TODO: Add your data members
  std::string prompt;
  JobsList* jobsList;
  char* lastpwd;

    //JobsList::JobEntry* currentJob;
  /*
   * current running process (in fg) for error handling
   * */

  SmallShell(); //DONE
 public:
    pid_t smashPID;
    pid_t current_PID;
    int current_jobID;
    std::map<std::string, std::string> aliases;
    std::vector<std::string> insertion_order;
    Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  std::string getPrompt()
  {
      return prompt;
  }

  void setPrompt(const std::string prompt)
  {
      this->prompt = prompt + "> ";
  }

  JobsList* getJobsList(){
      return jobsList;
  }

  void printJobsVector();

  ~SmallShell();
  void executeCommand(const char* cmd_line);
  // TODO: add extra methods as needed
};

class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char *cmd_line);

    virtual ~ListDirCommand() {}

    void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
    GetUserCommand(const char *cmd_line);

    virtual ~GetUserCommand() {}

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand() {}

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand() {}

    void execute() override;
};

#endif //SMASH_COMMAND_H_