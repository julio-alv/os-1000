# os-1000

this is an implementation in C++ of the guide [OS in 1K LoC](https://operating-system-in-1000-lines.vercel.app/en)

## Running the project

```bash
./run.sh
```

## OS Guide

The operating system contais a basic scheduler, file system and shell

here are the programs that you can run:

```sh
hello # prints "Hello world from shell!"
exit # closes the process
readfile # reads the contents from hello.txt
writefile # writes a file into the file system (should be visible if you uncompress the resulting disk.tar)
```

the OS mounts the folder /disk as a tar, which is the format that the file system uses
