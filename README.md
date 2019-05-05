"what" Tool:
------------

A tool to read "what" strings from ELF files.

ELF file can be binary or core file.

Build:
------
```
  $ CXX=clang++
  $ make
```

Usage:
------
```
  $ ./bin/what <elf_file>
```
Test:
-----
```
  $ cd test
  $ ./build.sh
  $ ../bin/what test
    < shows what strings >
  $ ../bin/what core.test
    < shows what strings >
```
