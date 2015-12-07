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
on `pkg-config` and FreeSWITCH, you need to point `pkg-config` to where
FreeSWITCH is installed before building:

```
host$ export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig/
host$ make
```

Sample Configuration
--------------------

Just put a file like this in your freeswitch installation, in **conf/autoload_configs/amd.conf.xml**
```xml
<configuration name="amd.conf" description="mod_amd Configuration">
  <settings>
    <param name="silence_threshold" value="256"/>
    <param name="maximum_word_length" value="5000"/>
    <param name="maximum_number_of_words" value="3"/>
    <param name="between_words_silence" value="50"/>
    <param name="min_word_length" value="100"/>
    <param name="total_analysis_time" value="5000"/>
    <param name="after_greeting_silence" value="800"/>
    <param name="greeting" value="1500"/>
    <param name="initial_silence" value="2500"/>
  </settings>
</configuration>
```

Variables
---------

After the AMD execution, the variable `amd_result` and `amd_cause` will be set.

The variable `amd_result` will return one of the following results:

- NOTSURE: take this value if total_analysis_time is over and decision could not be made
- HUMAN: if a human is detected
- MACHINE: if a human is detected


The variable `amd_cause` will return one of the following results:

- INITIALSILENCE (MACHINE)
- SILENCEAFTERGREETING (HUMAN)
- MAXWORDLENGTH (MACHINE)
- MAXWORDS (MACHINE)
- LONGGREETING (MACHINE)
- TOOLONG (NOTSURE)


Usage
-----

Set a Dialplan as follow:

```xml
    <extension name="amd_ext" continue="false">
      <condition field="destination_number" expression="^5555$">
        <action application="answer"/>
          <action application="amd"/>
          <action application="playback" data="/usr/local/freeswitch/sounds/en/us/callie/voicemail/8000/vm-hello.wav"/>
          <action application="info"/>
          <action application="hangup"/>
      </condition>
    </extension>
```

The originate a call that will bridge to the `amd_ext` dialplan:

    originate {origination_caller_id_number='808111222',ignore_early_media=true,originate_timeout=45}sofia/gateway/mygateway/0044888888888 5555
