liftoff
=======

liftoff is a small tool that works similar to the Unix command
`time`. It mainly prints the execution time of a command but in
a higher granularity. Additionally it prints the ressource
usage observed by the kernel. This output can be redirected to
an output file.

 * real execution time
 * user execution time
 * system execution time
 * maximum resident set size
 * soft page faults
 * hard page faults
 * fs input ops
 * fs output ops
 * voluntary context switches
 * involuntary context switches

Example
-------

```sh
$ liftoff -- echo "Hello world!"
Hello world!
real execution time:          0.003334 s
user execution time:          0.000000 s
system execution time:        0.000000 s
maximum resident set size:    2092 kb
soft page faults:             80
hard page faults:             0
fs input ops:                 0
fs output ops:                0
voluntary context switches:   6
involuntary context switches: 3
exit code:                    0
command line:                 echo Hello world!
```
