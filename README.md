
to build the code:
gcc -o test *.c -I./

to run the test:
./test 10000 1024 0.9
where:
- 10000 - number of packets supported by the CHANNEL
- 1024 - PACKET size
- 0.9 PACKET LOSS rate

