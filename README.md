# nush
This was a simple command line shell that I wrote for Northeastern University's CS 3650 (Computer Systems class)

This code uses the fork and exec Linux syscalls to create processes to run the commands.

This shell supports the basic operators:
1. \> (redirect output)
2. < (redirect input)
3. | (pipe)
4. & (background)
5. &&
6. ||
7. ; (do left, then right)

Furthermore, the user can use the cd and exit commands.


## Installation

To install and run this program, use the make command and then run the binary.
To clean up the program, run the 'make clean' command.

May only work on Linux or Mac.


