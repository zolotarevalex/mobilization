
to build the code:
gcc -o test *.c -I./

to run the test:
./test 10000 1024 0.9 test.in test.out
where:
- 10000 - number of packets supported by the CHANNEL
- 1024 - PACKET size
- 0.9 PACKET LOSS rate
- test.in binary file with input data (for example created with head -c 123 </dev/urandom > test.in)
- test.out file name for the result file

