#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <ios>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include <array>

using namespace std;

const int stdoutfd(dup(fileno(stdout)));

typedef struct process
{
  struct process *next; //next process to execute
  char **argv; //arguments
  pid_t pid;
  char completed;
  char stopped;
  int status;
  //vector<string> redirects;
  vector<string> strCmd;
} process;

struct process fakeproc;

typedef struct job
{
  struct job *next;
  string cmd;
  process *head_process;
  pid_t pgid;
  char notified;
  string status;
  int in, out, err;
} job; 

void pipeCommands(vector<vector<string>>,bool);
vector<char *> mk_cstrvec(vector<string> & strvec);
void dl_cstrvec(vector<char *> & cstrvec);
void nice_exec(vector<string> args);
void help();
void change_prompt();
inline void nope_out(const string & sc_name);
void defaultSignal();
void defaultIO();
void redirc(string file);
void handleIO(vector<string>);
void readIO(vector<string>, int&, int&, int&);
int argCheck(string);
void handleProc(process *, pid_t, int, int, int, int);
void handleJob(job *, int);
void wait_for_job(job *);
void format_job_info(job *, const char *);
void do_job_notification(void);
void update_status(void);
int job_is_stopped(job *);
int job_is_completed(job *);
int mark_process_status(pid_t pid, int status);
void printjobs(vector<job>jobs);
void continue_job (job *j, int foreground);
void mark_job_as_running (job *j);
void put_job_in_background (job *j, int cont);
void put_job_in_foreground (job *j, int cont);
job * find_job (pid_t pgid);
void updateJobs(int pid);
void update_status(pid_t pgid, string status);
void close_pipe(int pipefd [2]);
/**
   struct job{
   string pstatus;
   string command;
   int jid;
   pid_t pid;
   };*/


job *head_job = NULL;
pid_t shellPID;
int shell_terminal;
  vector<job> jobList;

//{}
//pgid
//exit status
//head
//

int main(int argc, char * argv[]) {
  //determine the process ID of the shell itself
  shellPID = getpid();
  //
  shell_terminal = STDIN_FILENO;
  //give shell terminal control
  tcsetpgrp(STDIN_FILENO, shellPID);

  /**
     vector<string> test;
     test.push_back("cat");
     test.push_back("tt.txt");
     struct job tjob;
     vector<char *> cstrargs = mk_cstrvec(test);
     fakeproc.argv = &cstrargs.at(0);
     tjob.head_process = &fakeproc;
     handleJob(&tjob, 1);
     exit(1);
  */
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  //defaultSignal();

  while (true){
	
    //place the shell in a process group by itself
    if(setpgid(shellPID, shellPID) < 0) nope_out("Shell PID");
   
    change_prompt();
    string input = "", arg;
    string in = "STDIN_FILENO",
      out = "STDOUT_FILENO",
      err = "STDERR_FILENO";
    stringstream ss;  
    char buffer[1];
    int n, count = 0;
    vector<string> redirects;
    vector<string> processOld;
    //char **argv
    vector<vector<string>> commands;
    vector<process> pvect;
    struct process pbuf;
    struct job jbuf;
    jbuf.in = STDIN_FILENO;
    jbuf.out = STDOUT_FILENO;
    jbuf.err = STDERR_FILENO;
    bool bg = false;
    bool redirIO = false;
    while((n = read(STDIN_FILENO,buffer,1)) > 0){
      if(buffer[0] == '\n')
	break;
      input += buffer[0];
    }
    if(input.length() != 0){
      ss << input;
      int pipes = 0;
      int argc = 0;
      bool first = true;
      while (ss >> arg) {//symbol checking
	if (arg == "&"){
	  bg = true;
	  cout << "& detected" << endl;
	}
	else if(arg == "|"){
	  commands.push_back(processOld);
  	  /*
	    vector<char *> cstrargs = mk_cstrvec(processOld);
	    pbuf.argv = &cstrargs.at(0);
	    if(first)
	    {
	    jbuf.head_process = &pbuf;
	    first = false;
	    }
	    pvect.push_back(pbuf);
	    //pbuf = new process;
	    pbuf.argv = nullptr;
	    pvect.back().next = &pbuf;
	    pbuf.next = nullptr;
	  */
	  pipes++;
	  processOld.clear();
	}
	//output redirections
	
	  else if(argCheck(arg) == 2)
	  {
	  do{
	  if(argCheck(arg) != 2) break;
	  redirIO = true;
	  string out = "";
	  redirects.push_back(arg);
	  ss >> arg;
	  redirects.push_back(arg);
	  //continue;
	  } while(ss >> arg);
	  break;
	  }
	  //else if (arg == "&")
	  //	bg = true;
	  //if arg == any io redirection symbols change bools?
	else
	
	  processOld.push_back(arg);
      }

      vector<char *> cstrargs = mk_cstrvec(processOld);
      pbuf.argv = &cstrargs.at(0);
      pbuf.strCmd = processOld;
      if(first)
	{
	  jbuf.head_process = &pbuf;
	  first = false;
	}
      pbuf.next = nullptr;
      // vector<char> temp(input.length() + 1);
      // strcpy(&temp[0], input.c_str());
      // char * commandLine = &temp[0];
      // jbuf.cmd = commandLine;
      // cout << jbuf.cmd << endl;

      jbuf.cmd = input;
      //readIO(redirects, jbuf.in, jbuf.out, jbuf.err);
    
      commands.push_back(processOld);
      if(commands.size() != 0){
	if(commands[0][0] == "cd"){
	  if(commands[0].size() == 1){
	    char * homedir = getenv("HOME");
	    chdir(homedir);
	  }
	  else if(commands[0].size() == 2) {
	    if(chdir(commands[0][1].c_str()) < 0)
	      perror(commands[0][1].c_str());
	  }
	  else 
	    cout << "Too many arguments" << endl;
	}
	else if(commands[0][0] == "exit"){
	  exit(getpid());
	}
	else if(commands[0][0] == "help"){
	  help();
	}
	else if(commands[0][0] == "jobs"){
	  printjobs(jobList);
	}
	else if(commands[0][0] == "bg"){
	  if(commands[0].size() < 2)
	    cout <<"bg: file JID must be specified" << endl;
	  else{
	    int tempjid;
	    try{
	      tempjid = stoi(commands[0][1]);
	    }
	    catch (const std::invalid_argument& e){
	      cout << "must be number" <<endl;
	    }
	    //cout << tempjid << endl;
	    //for job vector iteration, search each job
	    //struct for the matching pid
	    //
	    //call put_job_in_background(matching job, true ?
	    job* j = find_job(tempjid);
	    put_job_in_background(j,0);
	  }
	}
	else if(commands[0][0] == "fg"){
	  if(commands[0].size() < 2)
	    cout <<"fg: file JID must be specified" << endl;
	  else{
	    int tempjid = stoi(commands[0][1]);
	    cout << tempjid << endl;
	    //for job vector iteration, search each job
	    //struct for the matching pid
	    //
	    //call put_job_in_background(matching job, true ?)
	    job* j = find_job(tempjid);
	    continue_job(j,0);
	  }
	}
	else if(commands[0][0] == "export"){
	  string str= "";
	  string name = "";
	  string word = "";
	  if(commands[0].size() < 2)
	    cout << "export command: export  NAME[=WORD]" << endl;
	  else{
	    str = commands[0][1];
	    for (int i = 0; i < str.length(); i++){
	      if(str.substr(i,1) == "[")
		break;
	      else
		name+=str.at(i);
	    }
	    for(int i = name.length() + 1; i < str.length(); i++){
	      if (str.substr(i,1) == "]")
		break;
	      else if(str.substr(i,1) != "=")
		word+=str.at(i);
	    }
	    if (setenv(name.c_str(), word.c_str(), 1) == -1)
	      perror("setenv");
	    cout << "set var" << endl;
	  }
	}
	else{
	  //pbuf.argv
	  //pipeCommands(commands, pipes, redirIO, redirects);
	  if(!bg){
	    //handleJob(&jbuf, 0);
	    pipeCommands(commands,bg);
	  }/*
	  else if(bg){
	  //jobList.push_back(jbuf);
	  put_job_in_background(&jobList.at(count), -1);
	  count++;
	  }
	   */

	}
      }
    }
  }
}


void handleProc(process *p, pid_t pgid, int in, int out, int err, int fg)
{
  pid_t pid = getpid();
  //create new process group if appropriate
  if(pgid == 0) pgid = pid;
  setpgid(pid, pgid);

  if(fg) tcsetpgrp(shellPID, pgid);

  //handle for/back ground
	
  defaultSignal();

  //handle IO
  //if(p->redirects != nullptr)
  //	handleIO(p->redirects);
	
  if(in != STDIN_FILENO)
    {
      dup2(in, STDIN_FILENO);
      close(in);
    }
  if(out != STDOUT_FILENO)
    {
      dup2(out, STDOUT_FILENO);
      close(out);
    }
  if(err != STDERR_FILENO)
    {
      dup2(err, STDERR_FILENO);
      close(err);
    }

  //execute
  //string file = "seg.txt";
  //int fd = open(file.c_str(), O_RDWR, 0644);
  //write(fd, p->argv, 1000);
  //cout << p->argv << endl;
  nice_exec(p->strCmd);
  //execvp(p->argv[0], p->argv);
  perror("execvp");
  exit(1);
}

void handleJob(job *jb, int fg)
{
  process *p;
  pid_t pid;
  int pipefd[2], in, out;
  in = jb->in;

  //iterate linked list of processes
  for(p = jb->head_process; p; p = p->next)
    {
      if(p->next)
	{
	  if(pipe(pipefd) < 0)
	    {
	      nope_out("pipe");
	    }
	  out = pipefd[1];
	} else {
	out = jb->out;
      }

      pid = fork();
      if(pid == 0){
	handleProc(p, jb->pgid, in, out, jb->err, fg);
      } else if (pid < 0) {
	nope_out("fork");
      } else {
	p->pid = pid;
	if(!jb->pgid)
	  jb->pgid = pid;
	setpgid(pid, jb->pgid);
	cout << jb->pgid << endl;
      }
    }
  if(in != jb->in) close(in);
  dup2(out, STDOUT_FILENO);
  if(out != jb->out) close(out);
  in = pipefd[0];
  wait_for_job(jb);
  out = STDOUT_FILENO;
  //	close(in);
  //	close(out);
}

job* find_job(pid_t pgid)
{
  job *j;
  for(j = head_job; j; j = j->next)
    if(j->pgid == pgid)
      return j;
  return NULL;
}

int job_is_stopped(job *j)
{
  process *p;
  for(p = j->head_process; p; p = p->next)
    if(!p->completed && !p->stopped)
      return 0;
  return -1;
}

int job_is_completed(job *j)
{
  process *p;
  for(p = j->head_process; p; p = p->next)
    if(!p->completed)
      return 0;
  return 1;
}

void nice_exec(vector<string> strargs) {
  vector<char *> cstrargs = mk_cstrvec(strargs);
  if((execvp(cstrargs.at(0), &cstrargs.at(0))) == -1){
    perror("execvp");
  }
  dl_cstrvec(cstrargs);
  exit(EXIT_FAILURE);
}

vector<char *> mk_cstrvec(vector<string> & strvec) {
  vector<char *> cstrvec;
  for (unsigned int i = 0; i < strvec.size(); ++i) {
    cstrvec.push_back(new char [strvec.at(i).size() + 1]);
    strcpy(cstrvec.at(i), strvec.at(i).c_str());
  } // for
  cstrvec.push_back(nullptr);
  return cstrvec;
} // mk_cstrvec

void dl_cstrvec(vector<char *> & cstrvec) {
  for (unsigned int i = 0; i < cstrvec.size(); ++i) {
    delete[] cstrvec.at(i);
  } // for
}

void help(){
  cout.setf(ios::unitbuf);
  cout << "Available Commands: \n"
       << "bg JID             : Resume the stopped job JID in the background, as if it had been started with &.\n"
       << "cd [PATH]          : Change the current directory to PATH. The environmental variable HOME is the default PATH.\n"
       << "exit [N]           : Cause the shell to exit with a status of N. If N is omitted, the exit status is that of the last job executed.\n"
       << "export NAME[=WORD] : NAME is automatically included in the environment of subsequently executed jobs.\n"
       << "fg JID             : Resume job JID in the foreground, and make it the current job.\n"
       << "help               : Display helpful information about builtin commands.\n"
       << "jobs               : List current jobs."<< endl;
}

//update the prompt
void change_prompt(){
  struct passwd *pw = getpwuid(getuid());
  string homedir = pw->pw_dir;
  char pwd [PATH_MAX];
  getcwd(pwd,PATH_MAX + 1);
  char pathbuf [1024];
  string path = pwd;
  string temp = path.substr(0,homedir.size());
  if(temp == homedir && path != homedir){
    path.erase(0,homedir.size());
    path = "~" + path;
  }
  strcpy(pathbuf,"1730sh:");
  strcat(pathbuf, path.c_str());
  strcat(pathbuf, "$ ");

  // defaultIO(); 
  cout.setf(ios::unitbuf);
  string promptStr = string(pathbuf);
  //printf(promptStr.c_str());
  cout << promptStr;

  /**
     write(STDOUT_FILENO,pathbuf,1024);
     for(int i = 0; i < PATH_MAX; i++)
     pathbuf[i] ='\0'; */
}

//handle pipes and commands
void pipeCommands( vector<vector<string>> commands, bool bg){
  pid_t pgid, pid;
  int status;
  vector<array<int, 2>> pipes;
  int backupIn = dup(STDIN_FILENO);
  int backupOut = dup(STDOUT_FILENO);
  int fdErr,fdIn,fdOut;
  

  pid_t chld_pid; 
  while ((chld_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
    if (WIFEXITED(status)){
      update_status(chld_pid, "exited"); 
    } else if (WIFSTOPPED(status)){
      update_status(chld_pid, "stopped"); 
    } else if (WIFSIGNALED(status)){
      update_status(chld_pid, "killed"); 
    } else {
      perror("waitpid");
    }
  }

  for (unsigned int i = 0; i < commands.size(); ++i) 
    {
      if (i != commands.size() - 1)//if there isn't just one command, make pipes 
	{
	  int pipefd [2];
	  if (pipe(pipefd) == -1) 
	    {
	      perror("pipe");
	    } // if
	  pipes.push_back({pipefd[0], pipefd[1]});
	} // if

     
      if ((pid = fork()) == -1) 
	{
	  perror("fork");
	  exit(EXIT_FAILURE);
	} 
      // else if (pid > 0){
      // }
      else if (pid == 0)//if in child process 
	{
	  defaultSignal();
    
	  if (i != 0) 
	    {
	      if(bg)
		{
		  setpgid(0, pgid);
		}

	      if (dup2(pipes.at(i-1)[0], STDIN_FILENO) == -1)
		{
		  perror("dup2 i!=0");
		  exit(EXIT_FAILURE);
		} // if
			            
	    } // if
	  else
	    {

	      if(bg)
		{
		  setpgid(0,0);
		}
	    }
  
	  if (i != commands.size() - 1) 
	    {
	      if (dup2(pipes.at(i)[1], STDOUT_FILENO) == -1) 
		{
		  perror("dup2 i!= -1");
		  exit(EXIT_FAILURE);
		} // if
	    } // if
  
	  // close all pipes created so far
	  for (unsigned int i = 0; i < pipes.size(); ++i) 
	    {
	      close_pipe(pipes.at(i).data());
	    } // for
	  nice_exec(commands.at(i));
	  dup2(backupIn,STDIN_FILENO);
	  dup2(backupOut,STDOUT_FILENO);
	} // end if in child process
      else //in parent process
	{
	  if(tcsetpgrp(STDIN_FILENO, pid) < 0)
	    {
	      perror("tcsetgrp");
	    }
			     
	  if(bg && (i==0))
	    {
	      pgid = pid;
	      updateJobs(pgid);
	    }

	  if (i == 0){
	    if (setpgid(pid, pid) == -1){ 
	      perror("setpgid"); 
	    }
	  } else {
	    if (setpgid(pid, pgid) == -1){
	      perror("setpgid"); 
	    }
	  }
            
	  if (!bg){
	    if (tcsetpgrp(STDIN_FILENO, pid) < 0){
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE); 
	    }
	  } else {
	    if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0){
	      perror("tcsetpgrp");
	      exit(EXIT_FAILURE); 
	    }
	  }
           
	}
      
    } // for

  for (unsigned int i = 0; i < pipes.size(); ++i) 
    {
      close_pipe(pipes.at(i).data());
    } // for
  

  if (!bg){  
    for (unsigned int i = 0; i < commands.size(); i++){ 
      waitpid(pid, &status, WUNTRACED); 
      if (WIFSTOPPED(status)){
	setpgid(pid, pid);  // make the new background process its own process group
      }
    }
  }
  
  if(tcsetpgrp(STDIN_FILENO, getpgrp()) < 0)
    {
      perror("tcsetgrp");
    }
}


void handleIO(vector<string> arg){
	
  for(unsigned int i =0;i<arg.size();i++){
    if(arg[i] == ">")
      {
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDOUT_FILENO);
	close(newfd);
	continue;
	//out += " (truncate)";
      }

    else if(arg[i] == ">>")
      {
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDOUT_FILENO);
	close(newfd);
	continue;
	//out += " (append)";
      }

    //error redirections
    else if(arg[i] == "e>")
      {
	//err = data.at(i+1);
	//err = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDERR_FILENO);
	close(newfd);
	continue;
      }

    else if(arg[i] == "e>>")
      {
	//err = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDERR_FILENO);
	close(newfd);
	continue;
	//err += " (append)";
      }

    //input redirections
    else if(arg[i] == "<")
      {
	//in = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(newfd < 0) nope_out("Input Redirection");
	dup2(newfd, STDIN_FILENO);
	close(newfd);
	continue;
      }
  }
}


void readIO(vector<string> arg, int &in, int &out, int &err){
	
  for(unsigned int i =0;i<arg.size();i++){
    if(arg[i] == ">")
      {
	int outfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(outfd, STDOUT_FILENO);
	out = outfd;
	close(outfd);
	continue;
	//out += " (truncate)";
      }

    else if(arg[i] == ">>")
      {
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDOUT_FILENO);
	close(newfd);
	continue;
	//out += " (append)";
      }

    //error redirections
    else if(arg[i] == "e>")
      {
	//err = data.at(i+1);
	//err = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDERR_FILENO);
	close(newfd);
	continue;
      }

    else if(arg[i] == "e>>")
      {
	//err = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(newfd, STDERR_FILENO);
	close(newfd);
	continue;
	//err += " (append)";
      }

    //input redirections
    else if(arg[i] == "<")
      {
	//in = data.at(i+1);
	int newfd = open(arg[++i].c_str(), O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(newfd < 0) nope_out("Input Redirection");
	dup2(newfd, STDIN_FILENO);
	close(newfd);
	continue;
      }
  }
}

int argCheck(string cmd)
{
  string builtins[] = {"cat","grep","more","less", "echo"};
  string redirects[] = {"<",">",">>","e>", "e>>"};
  //determine if the argument is an executable program
  for(string str: builtins)
    {
      if(str.compare(cmd) == 0)
	{
	  return 1;
	}
    }

  //determine if th argument is a I/O redirection character
  for(string str: redirects)
    {
      if(str == cmd)
	{
	  return 2;
	}
    }

  //determine if pipe character
  if(cmd.compare("|") == 0) return 3;

  return 0;
}

inline void nope_out(const string & sc_name) {
  perror(sc_name.c_str());
  exit(EXIT_FAILURE);
}

//set signal handlers back to default
void defaultSignal(){
  signal(SIGINT,  SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  signal(SIGTSTP, SIG_DFL);
  signal(SIGTTIN, SIG_DFL);
  signal(SIGTTOU, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
}

void defaultIO(){
  //dup2(STDIN_FILENO, fileno(stdin));
  fflush(stdout);
  //dup2(fileno(stdin), STDIN_FILENO);
  dup2(stdoutfd, fileno(stdout));
  //dup2(STDERR_FILENO, fileno(stderr));

  close(stdoutfd);
  //close(STDOUT_FILENO);
  //close(STDERR_FILENO);
}

void sig_handler(int signo)
{
  struct sigaction sigact;
  //sigact.sa_handler = 
  //if(signo == SIGABRT)
}

/* Store the status of the process pid that was returned by waitpid.
   Return 0 if all went well, nonzero otherwise.  */
int mark_process_status (pid_t pid, int status)
{
  job *j;
  process *p;

  if (pid > 0)
    {
      /* Update the record for the process.  */
      for (j = head_job; j; j = j->next)
        for (p = j->head_process; p; p = p->next)
          if (p->pid == pid)
            {
              p->status = status;
              if (WIFSTOPPED (status))
                p->stopped = 1;
              else
                {
                  p->completed = 1;
                  if (WIFSIGNALED (status))
                    fprintf (stderr, "%d: Terminated by signal %d.\n",
                             (int) pid, WTERMSIG (p->status));
                }
              return 0;
	    }
      //fprintf (stderr, "No child process %d.\n", pid);
      return -1;
    }
  else if (pid == 0 || errno == ECHILD)
    /* No processes ready to report.  */
    return -1;
  else {
    /* Other weird errors.  */
    perror ("waitpid");
    return -1;
  }
}

/* Check for processes that have status information available,
   without blocking.  */

void
update_status (void)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED|WNOHANG);
  while (!mark_process_status (pid, status));
}

/* Check for processes that have status information available,
   blocking until all processes in the given job have reported.  */

void
wait_for_job (job *j)
{
  int status;
  pid_t pid;

  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED);
  while (!mark_process_status (pid, status)
         && !job_is_stopped (j)
         && !job_is_completed (j));
}

/* Format information about job status for the user to look at.  */

void
format_job_info (job *j, const char *status)
{
  fprintf (stderr, "%ld (%s): %s\n", (long)j->pgid, status, j->cmd);
}

/* Notify the user about stopped or terminated jobs.
   Delete terminated jobs from the active job list.  */

void
do_job_notification (void)
{
  job *j, *jlast, *jnext;
  process *p;

  /* Update status information for child processes.  */
  update_status ();

  jlast = NULL;
  for (j = head_job; j; j = jnext)
    {
      jnext = j->next;

      /* If all processes have completed, tell the user the job has
         completed and delete it from the list of active jobs.  */
      if (job_is_completed (j)) {
        format_job_info (j, "completed");
        if (jlast)
          jlast->next = jnext;
        else
          head_job = jnext;
	//  free_job (j);
      }

      /* Notify the user about stopped jobs,
         marking them so that we won't do this more than once.  */
      else if (job_is_stopped (j) && !j->notified) {
        format_job_info (j, "stopped");
        j->notified = 1;
        jlast = j;
      }

      /* Don't say anything about jobs that are still running.  */
      else
        jlast = j;
    }
}

void
mark_job_as_running (job *j)
{
  process *p;

  for (p = j->head_process; p; p = p->next)
    p->stopped = 0;
  j->notified = 0;
}

/* Continue the job J.  */

void
continue_job (job *j, int foreground)
{
  mark_job_as_running (j);
  if (foreground)
    put_job_in_foreground (j, 1);
  else
    put_job_in_background (j, 1);
}

/* Put job j in the foreground.  If cont is nonzero,
   restore the saved terminal modes and send the process group a
   SIGCONT signal to wake it up before we block.  */

void put_job_in_foreground (job *j, int cont)
{
  /* Put the job into the foreground.  */
  tcsetpgrp (shell_terminal, j->pgid);

  /* Send the job a continue signal, if necessary.  */
  if (cont)
    {
      //tcsetattr (shell_terminal, TCSADRAIN, &j->tmodes);
      if (kill (- j->pgid, SIGCONT) < 0)
        perror ("kill (SIGCONT)");
    }

  /* Wait for it to report.  */
  wait_for_job (j);

  /* Put the shell back in the foreground.  */
  tcsetpgrp (shell_terminal, shellPID);

  /* Restore the shell’s terminal modes.  */
  //tcgetattr (shell_terminal, &j->tmodes);
  //tcsetattr (shell_terminal, TCSADRAIN, &shell_tmodes);
}
/* Put a job in the background.  If the cont argument is true, send
   the process group a SIGCONT signal to wake it up.  */

void
put_job_in_background (job *j, int cont)
{
  /* Send the job a continue signal, if necessary.  */
  setpgid(0,j->pgid);
  if (cont >= 0)
    if (kill (-j->pgid, SIGCONT) < 0)
      perror ("kill (SIGCONT)");
}

void printjobs(vector<job> jobs){

  string status;
  cout << "JID\tSTATUS\t\tCOMMAND" << endl;
  for(int i = 0; i < jobs.size(); i++){
    struct job temp = jobs.at(i);
    int pid = temp.pgid;
    string command = (string)temp.cmd;
    status = temp.status;
    cout << pid << "\t" << status << "\t" << command << endl;
  }
}      

void updateJobs(int pid){
  job tempJ;
  tempJ.pgid = pid;
  string status;
  if (job_is_stopped(&tempJ) == 0)
    status = "stopped";
  else if (job_is_completed(&tempJ) == 1)
    status = "completed";
  else
    status = "running";
  tempJ.status = status;
  jobList.push_back(tempJ);
}
  
void update_status(pid_t pgid, string status)
{
  for (auto entry = jobList.begin(); entry != jobList.end(); entry++) 
    {
      job &job = *entry; 
      
      if (job.pgid != pgid)
	{
	  continue; 
	} 
      else 
	{
	  job.status = status; 
	  //printf("UPDATE: [%d] %s %s\n", job.groupid, job.input.c_str(), job.running.c_str());
	}
    }
}

void close_pipe(int pipefd [2]) {
  if (close(pipefd[0]) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  } // if
  if (close(pipefd[1]) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  } // if
} // close_pipe
