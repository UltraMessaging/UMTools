# DEBUG - Tools for UM Debug Logs

# Table of contents

- [DEBUG - Tools for UM Debug Logs](#debug---tools-for-um-debug-logs)
- [Table of contents](#table-of-contents)
  - [COPYRIGHT AND LICENSE](#copyright-and-license)
  - [REPOSITORY](#repository)
  - [INTRODUCTION](#introduction)
- [dbg_timestamp.pl](#dbg_timestamppl)
  - [Example](#example)

<sup>(table of contents from https://luciopaiva.com/markdown-toc/)</sup>


## COPYRIGHT AND LICENSE

All of the documentation and software included in this and any
other Informatica Ultra Messaging GitHub repository
Copyright (C) Informatica. All rights reserved.

Permission is granted to licensees to use
or alter this software for any purpose, including commercial applications,
according to the terms laid out in the Software License Agreement.

This source code example is provided by Informatica for educational
and evaluation purposes only.

THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF
NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR
PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE
UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR
INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE
TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF
THE LIKELIHOOD OF SUCH DAMAGES.

## REPOSITORY

See https://github.com/UltraMessaging/UMTools/DEBUG for code and documentation.

## INTRODUCTION

Ultra Messaging debug logs are primarily intended for Informatica engineers to
analyze.
The output is generally too internals-related without user-level
documentation.
However, sometimes users can make some sense out of the log files
(usually with help from Informatica Support).

The tools here are developed by Informatica Support, primarily to assist
Support engineers in their analysis.
But to the degree that these tools turn out to be useful for users,
go for it!

# dbg_timestamp.pl

Tool to read a debug file and convert the timestamp into human-readable form.

Note that this took about 15 minutes to process a 7 gigabyte debug file.

UM debug logs contain a "compressed" timestamp that includes seconds after the hour,
but not the hour or date.
Each time the debug run crosses an hour boundary, the seconds after the hour resets
to zero.
Therefore, this tool keeps track of the number of times the debug timestamp wraps to
zero, and increments the hour.

The tool has no way of knowing the hour that the run was begun.
It is the user's responsibility to remember that (perhaps name the
file with the time and date).
That starting hour can be provided with the "-H hour" option.

Finally, be aware that it is possible for some debug logs to be non-deterministic.
For example, let's say that the only debug flag used is API.
And maybe 61 minutes go by without any API calls.
In that case, the tool will think the 61-minute gap is only a 1-minute gap,
and won't increment the hour.
Note that this is highly unlikely in most real-world applications (for
example, sending and receiving messages count as API invocations).

## Example

````
$ head -1 raw_debug_log.txt
[23796:1616492480|0518.345396]:[lbm_version]

$ ./dbg_timestamp.pl -H 9 raw_debug_log.txt >processed_debug_log.txt
[23796:1616492480|09:08:38.345396]:[lbm_version]
````
