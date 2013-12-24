Asterisk app_amd for FreeSWITCH
===============================

This is an implementation of Asterisk's answering machine detection (voice
activity detection) for FreeSWITCH.

Currently, in limited testing, we are about to get satisfactory results in
determining what is a human and what is a machine, but there is much more to
do:

* Emit events when a decision is made (Not sure, Machine, or Human).
* Make sure that we are unlocking and cleaning up where necessary.

Building
--------

To build this module, all you need to do is type `make`, but because it relies
on pkg-config and FreeSWITCH, you need to point pkg-config to where FreeSWITCH
is installed:

```$ export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig/

$ make
```
