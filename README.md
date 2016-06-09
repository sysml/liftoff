liftoff
=======

liftoff is a small tool that works similar to the Unix command
`time`. It mainly prints the execution time of a command but in
a higher granularity. Additionally it prints the ressource
usage observed by the kernel. This output can be redirected to
an output file.

 * start time
 * real execution time
 * user execution time
 * system execution time
 * initial scheduler affinity set
 * maximum resident set size
 * soft page faults
 * hard page faults
 * file input operations
 * file output operations
 * voluntary context switches
 * involuntary context switches
 * exit code
 * command line

Example
-------

```sh
$ liftoff -- echo "Hello world!"
Hello world!
start time:                   2016-06-09 16:29:39.713507
real execution time:          0.001824 s
user execution time:          0.000000 s
system execution time:        0.000000 s
initial affinity set:         0 1 2 3 
maximum resident set size:    2104 kb
soft page faults:             82
hard page faults:             0
fs input ops:                 0
fs output ops:                0
voluntary context switches:   18
involuntary context switches: 1
exit code:                    0
command line:                 echo Hello world!
```
