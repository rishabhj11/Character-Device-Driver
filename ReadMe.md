# Setup:
The project contains the code files- chrdriver.c, userapp.c and the make file-Makefile, in
order to compile the code.

# Steps to follow:
* To compile: $ make
* To run: $ sudo insmod chrdriver.ko NUM_DEVICES=<no_of_devices>
For Example: sudo insmod chrdriver.ko NUM_DEVICES=10
* To compile the userapp , run: make app
* To test the driver using the app, run: ./userapp <deviceNo>
* Use rmmod chrdriver to release the devices.
* To clean: $ make clean

# Details of implementation:
The character device driver has the following implementations:
1. The devices are created from within the program.
2. The devices have been put together in the form of kernel specific linked list.
3. The driver specific file operations, read, write, lseek, ioctl have been
implemented.
4. Synchronization has been achieved in the read, write and ioctl operations using
semaphores.
5. All the acquired resources have been released/freed using the exit function which
gets called on calling rmmod.
* The userapp provided in order to test the application has been modified slightly.
1. An infinite while loop was used in order to prevent the test application from
exiting to test subsequent operations.
2. A new variable ‘rand’ was introduced to handle the new file character which was
getting added after each input.

# Test Results:
Written – abcdefghij
1. REGULAR
* read – abcdefghij
2. REVERSE
* read - jihgfedcba
