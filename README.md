# Implementation-of-a-custom-Linux-shell
This is an implementation of a shell called smallsh. It provides a prompt for running
commands, handles blank lines and comments, provides expansion for the variable $$, executes
the commands exit, cd and status, executes other commands by creating new process using exec
functions, supports input and output redirection, supports running commands in foreground
and background processes, and implements customs handlers for 2 signals, SIGINT and SIGSTP.
