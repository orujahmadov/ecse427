To build you code run the following commands

$ make clean
$ make


You can test your code by running the test case file provided. You can do that 
with the following commands

$ make test1

This will create a file named "sfs_test1" in your current directory. Run the test by running the file

$ ./sfs_test1

similarly for running test2 execute

$ make test2
$ ./sfs_test2

I have made a few changes in both test files because there were hard-coded file descriptors.
I have mentioned the changes in the test code.
Changes include:
- Not hard-coding file descriptors 0 and 1. I used fds[0] and fds[1]. File descriptor 0 is reserved for the root directory in my implementation
- The buffer in the test cases caused some memory problems when running them and I could not figure out why or how to fix it so I just allocated much more memory and then these problems went away. It is not the best way but I could not figure out how to prevent the memory problem. It happened when malloc'ing and free'ing buffers.
- In test file 2, the TA mentioned that the first sfs_remove(names[0]) should not be there so it is commented out in my tests.
