Arhan Nagavelli - arn97
Keith Miquela - kvm3

Testing Plan
-------------
1. BASIC EXECUTION & PATH SEARCH
   - Input: 'ls', 'pwd', 'whoami'
   - Goal: Verify the shell searches /usr/local/bin, /usr/bin, and /bin in order.
   - Result: Success. Programs were located and executed correctly.

2. BUILT-IN COMMANDS
   - Input: 'cd ..', 'cd', 'pwd', 'which ls', 'which cd'
   - Goal: Verify 'cd' updates the process directory, 'pwd' prints the 
     correct path, and 'which' correctly identifies program paths but 
     properly fails for built-ins.
   - Result: Success. 'cd' correctly handles no-argument calls by returning 
     to $HOME.

3. PIPELINES (|)
   - Input: 'ls | sort | head -n 3'
   - Goal: Verify that the stdout of one process is connected to the stdin 
     of the next, and that the shell waits for all processes to complete.
   - Result: Success. Multi-stage pipes execute without hanging or 
     orphaning processes.

4. REDIRECTION (<, >)
   - Input: 'echo "hello world" > test.txt', 'cat < test.txt'
   - Goal: Verify output redirection creates files with mode 0640 and input 
     redirection correctly feeds data to a process.
   - Result: Success. test.txt was created with correct permissions.

5. WILDCARD EXPANSION (GLOBBING)
   - Input: 'ls *.c', 'echo *.h', 'ls zzz*'
   - Goal: Verify '*' is expanded to matching files before execution. 
     If no match is found, the literal string should be passed.
   - Result: Success. 'ls *.c' correctly listed mysh.c, parse.c, and exec.c.

6. SIGNAL HANDLING
   - Input: 'sleep 100' followed by 'Ctrl+C'
   - Goal: Ensure the shell ignores SIGINT but the child process terminates 
     gracefully, reporting the signal name.
   - Result: Success. Shell printed "Terminated by signal Interrupt" and 
     returned to the prompt.

7. BATCH MODE
   - Input: './mysh test_script.sh'
   - Goal: Ensure no welcome messages or prompts are printed when reading 
     from a file or pipe.
   - Result: Success. Output is clean and matches standard bash behavior.3
