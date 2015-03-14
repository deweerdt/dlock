dlock is a lock debugging library for C programs which aims at instrumenting the lock tacking functions (namely pthread\_mutex_{un,}lock) to display a program's mutex usage. It provides a dumping function which displays the time the lock's been held as well as the dependencies between the different locks.
Most core code is already written, todo list includes:
- Get lock names from source code
- Detect potential deadlocks
- Allow displaying the dump upon signal reception
- Use weak symbols to transparently instrument a given binary
- Support spinlocks
- Support various locking functions_


Sample output:
[locked](A.md) [locked](B.md) [unlocked](B.md) [unlocked](A.md)
cksum 0x2f7b278c                held 0 ms, length: 4, times 2
[locked](C.md) [unlocked](C.md)
cksum 0x4669152a                held 0 ms, length: 2, times 1
[locked](D.md) [unlocked](D.md)
cksum 0x46691782                held 0 ms, length: 2, times 1



[0](0.md)
0xbf9f66f0 locked
0xbf9f6750 A
0xbf9f6738 B
0xbf9f6720 C
0xbf9f6708 D

