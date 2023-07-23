# Signal iOS DB Extractor

This is a unoffical tool for dumping Signal history from iOS, written in [ib](https://github.com/Im-0xea/ibranching) C.

### Introduction

This tool takes the database aquired following [this](https://web.archive.org/web/20220723075618/https://cight.co/backup-signal-ios-jailbreak/) excellent guide, and dumps it in a Human readable format.

![example image](https://socki.moe/sig.png "example output")

### Usage

Features:
- list groups and contacts
- dump chats (detects calls, attachments and quotes)
- formats (html, irc-like)

~~~
$ ./seqdump -s db.sql -l
Xea
Mom
Dad
You

$ ./seqdump -s db.sql -g Xea -o out.html
~~~

### Building

#### Dependencies:
- [ib](https://github.com/Im-0xea/ibranching)
- sqlite3
- libplist ( >= 2.3.0)

You can use this script to install the required version of ib, libplist and the inter font, this is not recommended but will work for the lazy.
~~~
$ ./depstrap 
~~~
~~~
$ make (IB=./ib if installed by depstrap)

(optional)
root $ make install
~~~
