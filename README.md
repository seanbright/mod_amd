Asterisk app_amd for FreeSWITCH
===============================

This is an implementation of Asterisk's answering machine detection (voice
activity detection) for FreeSWITCH.

Currently, in limited testing, we are about to get satisfactory results in
determining what is a human and what is a machine, but there is much more to
do:

* Make this function as a dialplan application.
* Set appropriate channel variables when a human/machine is detected.
* Make sure that we are unlocking and cleaning up where necessary.
