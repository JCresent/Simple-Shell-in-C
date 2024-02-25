# Simple Shell in C!
> Project was a learning experience into how to run basic processes though replicating a bash shell in c  
> Runs on Linux systems

## Implemented functions:
- Runs basic commands from bash: cd, ls, pwd, etc.
- **exit** command exits from the shell, killing all background processes
-  & at the end of a command will send it to run in the background, prinitng a message when it completes
-  Ctrl + C interupts the current or any parallel (&&&) running processes in the foregorund
     - (Like regular bash shell)
### && function - Sequential 
> Allows for input commands to run in sequence left to right when specified  
- Example: echo hello && sleep 3 && echo hi

### &&& function - Parallel 
> Allows for input commands to run in parallel (same time) when specified
- Example: echo hello &&& pwd &&& ls

#### Note:
> &&, &&&, and & do not currently have implemented functionality to be ran at same time  
> Wouldn't exactly make sense to run both && and &&&, but implementing functionality for & into both might be  

#### Future Improvemenets / Add-ons 
- Implementing piping and redirection operators  
- Possible periodic cleanup for zombie processes (like in regular bash)?
- Better handling of memory and cleanup


