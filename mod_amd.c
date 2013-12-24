#include <switch.h>

#define AMD_PARAMS (2)
#define AMD_SYNTAX "<uuid> <command>"

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amd_shutdown);
SWITCH_STANDARD_API(amd_api_main);

SWITCH_MODULE_LOAD_FUNCTION(mod_amd_load);

SWITCH_MODULE_DEFINITION(mod_amd, mod_amd_load, mod_amd_shutdown, NULL);
SWITCH_STANDARD_APP(amd_start_function);

static struct {
	int initial_silence;
	int greeting;
	int after_greeting_silence;
	int total_analysis_time;
	int minimum_word_length;
	int between_silence_words;
	int maximum_number_of_words;
	int silence_threshold;
	int maximum_word_length;
} globals;

static switch_xml_config_item_t instructions[] = {
	SWITCH_CONFIG_ITEM(
		"initial_silence",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.initial_silence,
		(void *) 2500,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"greeting",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.greeting,
		(void *) 1500,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"after_greeting_silence",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.after_greeting_silence,
		(void *) 800,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"total_analysis_time",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.total_analysis_time,
		(void *) 5000,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"min_word_length",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.minimum_word_length,
		(void *) 100,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"between_words_silence",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.between_silence_words,
		(void *) 50,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"maximum_number_of_words",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.maximum_number_of_words,
		(void *) 3,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"maximum_word_length",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.maximum_word_length,
		(void *)5000,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM(
		"silence_threshold",
		SWITCH_CONFIG_INT,
		CONFIG_RELOADABLE,
		&globals.silence_threshold,
		(void *) 256,
		NULL, NULL, NULL),

	SWITCH_CONFIG_ITEM_END()
};

typedef enum {
	IN_WORD,
	IN_SILENCE
} amd_state_t;

typedef struct amd_codec_info {
    int rate;
    int channels;
} amd_codec_info_t;

typedef struct amd_session_info {
	switch_core_session_t *session;
	amd_codec_info_t codec;
	amd_state_t state;
	uint32_t analysis_time;
	uint32_t total_silence;
	uint32_t total_voice;
	uint32_t in_greeting:1;
	uint32_t in_initial_silence:1;
	uint32_t word_count;
} amd_session_info_t;

static switch_bool_t is_silence_frame(switch_frame_t *frame, amd_session_info_t *amd_info)
{
	int16_t *fdata = (int16_t *) frame->data;
	uint32_t samples = frame->datalen / sizeof(*fdata);
	switch_bool_t is_silence = SWITCH_TRUE;
	uint32_t channel_num = 0;

	int divisor = 0;
	if (!(divisor = amd_info->codec.rate / 8000)) {
		divisor = 1;
	}

	for (channel_num = 0; channel_num < amd_info->codec.channels && is_silence; channel_num++) {
		uint32_t count = 0, j = channel_num;
		double energy = 0;
		for (count = 0; count < samples; count++) {
			energy += abs(fdata[j]);
			j += amd_info->codec.channels;
		}
		is_silence &= (uint32_t) ((energy / (samples / divisor)) < globals.silence_threshold);
	}

	return is_silence;
}

static switch_bool_t process_frame(amd_session_info_t *amd_info, switch_frame_t *frame)
{
	amd_info->analysis_time += 20;

	if (is_silence_frame(frame, amd_info)) {
		amd_info->total_silence += 20;
		amd_info->total_voice = 0;
	} else {
		amd_info->total_silence = 0;
		amd_info->total_voice += 20;
	}

	if (amd_info->total_silence > 0) {
		if (amd_info->total_silence >= globals.between_silence_words) {
			if (amd_info->state != IN_SILENCE) {
				switch_log_printf(
					SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
					SWITCH_LOG_WARNING,
					"AMD: Changed state to STATE_IN_SILENCE\n");
			}

			if (amd_info->total_voice < globals.minimum_word_length && amd_info->total_voice > 0) {
				switch_log_printf(
					SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
					SWITCH_LOG_WARNING,
					"AMD: Short Word Duration: %d\n",
					amd_info->total_voice);
			}

			amd_info->state = IN_SILENCE;
			amd_info->total_voice = 0;
		}

		if (amd_info->in_initial_silence && amd_info->total_silence >= globals.initial_silence) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: ANSWERING MACHINE: silenceDuration: %d / initialSilence: %d\n",
				amd_info->total_silence,
				globals.initial_silence);
			return SWITCH_FALSE;
		}

		if (amd_info->total_silence >= globals.after_greeting_silence && amd_info->in_greeting) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: HUMAN: silenceDuration: %d / afterGreetingSilence: %d\n",
				amd_info->total_silence,
				globals.after_greeting_silence);
			return SWITCH_FALSE;
		}
	} else {
		if (amd_info->total_voice >= globals.minimum_word_length && amd_info->state == IN_SILENCE) {
			amd_info->word_count++;
			amd_info->state = IN_WORD;

			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: Word detected. iWordsCount: %d\n",
				amd_info->word_count);
		}

		if (amd_info->total_voice >= globals.maximum_word_length) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: Maximum Word Length detected. [%d] (MACHINE)\n",
				amd_info->total_voice);
			return SWITCH_FALSE;
		}

		if (amd_info->word_count >= globals.maximum_number_of_words) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: ANSWERING MACHINE: iWordsCount: %d\n",
				amd_info->word_count);
			return SWITCH_FALSE;
		}

		if (amd_info->in_greeting && amd_info->total_voice >= globals.greeting) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: ANSWERING MACHINE: voiceDuration: %d greeting: %d\n",
				amd_info->total_voice,
				globals.greeting);
			return SWITCH_FALSE;
		}

		if (amd_info->total_voice >= globals.minimum_word_length) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: Detected Talk, previous silence duration: %d\n",
				amd_info->total_silence);
			amd_info->total_silence = 0;
		}

		if (amd_info->total_voice >= globals.minimum_word_length && !amd_info->in_greeting) {
			switch_log_printf(
				SWITCH_CHANNEL_SESSION_LOG(amd_info->session),
				SWITCH_LOG_WARNING,
				"AMD: Before Greeting Time:  silenceDuration: %d voiceDuration: %d\n",
				amd_info->total_silence,
				amd_info->total_voice);
			amd_info->in_initial_silence = 0;
			amd_info->in_greeting = 1;
		}
	}

	if (amd_info->analysis_time >= globals.total_analysis_time) {
		return SWITCH_FALSE;
	}

	return SWITCH_TRUE;
}

static switch_bool_t amd_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	amd_session_info_t *amd_info;
	switch_codec_t *codec;

	amd_info = (amd_session_info_t *) user_data;
	if (!amd_info) {
		return SWITCH_FALSE;
	}

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		codec = switch_core_session_get_read_codec(amd_info->session);
		amd_info->codec.rate = codec->implementation->samples_per_second;
		amd_info->codec.channels = codec->implementation->number_of_channels;
		break;

    case SWITCH_ABC_TYPE_READ_PING:
    case SWITCH_ABC_TYPE_CLOSE:
    case SWITCH_ABC_TYPE_READ:
    case SWITCH_ABC_TYPE_WRITE:
    case SWITCH_ABC_TYPE_TAP_NATIVE_READ:
    case SWITCH_ABC_TYPE_TAP_NATIVE_WRITE:
		break;

	case SWITCH_ABC_TYPE_READ_REPLACE:
		return process_frame(amd_info, switch_core_media_bug_get_read_replace_frame(bug));

	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t do_config(switch_bool_t reload)
{
	memset(&globals, 0, sizeof(globals));

	if (switch_xml_config_parse_module_settings("amd.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_amd_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	do_config(SWITCH_FALSE);

	SWITCH_ADD_APP(
		app_interface,
		"amd",
		"Voice activity detection",
		"Asterisk's AMD",
		amd_start_function,
		"[start] [stop]",
		SAF_NONE);

	SWITCH_ADD_API(api_interface, "amd", "Detect voice activity", amd_api_main, AMD_SYNTAX);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_amd_shutdown)
{
	switch_xml_config_cleanup(instructions);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(amd_start_function)
{
	switch_media_bug_t *bug;
	amd_session_info_t *amd_info;
	switch_channel_t *channel;
	switch_status_t status;

	if (!session) {
		return;
	}

	channel = switch_core_session_get_channel(session);

	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_amd_");

	if (bug) {
		if (strncasecmp(data, "stop", sizeof("stop") - 1) == 0) {
			switch_channel_set_private(channel, "_amd_", NULL);
			switch_core_media_bug_remove(session, &bug);
			return;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Already running on channel.\n");
		return;
	}

	amd_info = (amd_session_info_t *) switch_core_session_alloc(session, sizeof(amd_session_info_t));
	memset(amd_info, 0, sizeof(amd_session_info_t));
	amd_info->session = session;
	amd_info->in_initial_silence = 1;
	amd_info->state = IN_WORD;

	status = switch_core_media_bug_add(session, "amd", NULL, amd_callback, amd_info, 0, SMBF_READ_REPLACE, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure bugging stream\n");
		return;
	}

	switch_channel_set_private(channel, "_amd_", bug);
}

SWITCH_STANDARD_API(amd_api_main)
{
	switch_core_session_t *amd_session = NULL;
	switch_media_bug_t *bug;
	amd_session_info_t *amd_info;
	switch_channel_t *channel;
	switch_status_t status;

	int argc;
	char *argv[AMD_PARAMS];
	char *cmd_rw, *uuid, *command;

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", AMD_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	cmd_rw = strdup(cmd);

	/* Split the arguments */
	argc = switch_separate_string(cmd_rw, ' ', argv, AMD_PARAMS);

	if (argc != AMD_PARAMS) {
		stream->write_function(stream, "-USAGE: %s\n", AMD_SYNTAX);
		goto end;
	}

	uuid = argv[0];
	command = argv[1];

	amd_session = switch_core_session_locate(uuid);

	if (!amd_session) {
		/* This is stupid, the syntax is right but the session is gone... whatever */
		stream->write_function(stream, "-USAGE: %s\n", AMD_SYNTAX);
		goto end;
	}

	channel = switch_core_session_get_channel(amd_session);

	bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_amd_");

	if (bug) {
		if (strncasecmp(command, "stop", sizeof("stop") - 1) == 0) {
			switch_channel_set_private(channel, "_amd_", NULL);
			switch_core_media_bug_remove(amd_session, &bug);
			stream->write_function(stream, "+OK\n");
			goto end;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Already running on channel.\n");
		goto end;
	}

	if (strncasecmp(command, "start", sizeof("start") - 1) != 0) {
		stream->write_function(stream, "-USAGE: %s\n", AMD_SYNTAX);
		goto end;
	}

	amd_info = (amd_session_info_t *) switch_core_session_alloc(amd_session, sizeof(amd_session_info_t));
	memset(amd_info, 0, sizeof(amd_session_info_t));
	amd_info->session = amd_session;
	amd_info->in_initial_silence = 1;
	amd_info->state = IN_WORD;

	status = switch_core_media_bug_add(amd_session, "amd", NULL, amd_callback, amd_info, 0, SMBF_READ_REPLACE, &bug);

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure bugging stream\n");
		goto end;
	}

	switch_channel_set_private(channel, "_amd_", bug);

	stream->write_function(stream, "+OK\n");

  end:

	if (amd_session) {
		switch_core_session_rwunlock(amd_session);
	}

	switch_safe_free(cmd_rw);

	return SWITCH_STATUS_SUCCESS;
}
