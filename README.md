# Signal iOS DB Extractor

This is a unoffical tool for dumping Signal history from iOS.

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
- sqlite3
- libplist ( >= 2.3.0)

~~~
$ meson setup build
$ ninja -C build
$ ninja -C build install
~~~
