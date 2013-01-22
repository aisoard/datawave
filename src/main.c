#include "common.h"

static void signal_handler(int signal)
{
	jack_client_close(client);
	errx(0, "signal received, exiting ...\n");
}

int main(int argc, char * argv[])
{
	const char * client_name = NULL;

	/* Retrieve client name */
	char * begin = strrchr(argv[0], '/');
	if(begin != NULL)
		client_name = begin+1;
	else
		client_name = argv[0];

	/* Register client */
	jack_status_t status;
	client = jack_client_open(client_name, JackNullOption, &status, NULL);
	if(client == NULL) {
		if(status & JackServerFailed)
			warnx("Unable to connect to JACK server");
		errx(1, "jack_client_open() failed, status = 0x%2.0x", status);
	}

	if(status & JackServerStarted)
		warnx("JACK server started");

	if(status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		warnx("name already assigned, client renamed to `%s'", client_name);
	}

	/* Register callbacks */
	init(NULL); // JACK does not provides any init callback
	jack_set_process_callback(client, exec, NULL);
	jack_on_shutdown(client, fini, NULL);

	/* Display server caracteristics */
	printf("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate(client));

	/* Register input port */
	input_port = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if(input_port == NULL)
		errx(1, "no free JACK input ports available");

	/* Register output ports */
	output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if(output_port == NULL)
		errx(1, "no free JACK output ports available");

	/* Activate client */
	if(jack_activate(client))
		errx(1, "cannot activate client");

	/* Search for physical ports */
	const char ** ports = NULL;

	/* Connect input ports */
	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
	if(ports == NULL)
		errx(1, "no physical capture ports");
	for(int i = 0; ports[i] != NULL; i++)
		if(jack_connect(client, ports[i], jack_port_name(input_port)))
			warnx("cannot connect input port to physical capture port %d", i);
	jack_free(ports);

	/* Connect output ports */
	ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if(ports == NULL)
		errx(1, "no physical playback ports");
	for(int i = 0; ports[i] != NULL; i++)
		if(jack_connect(client, jack_port_name(output_port), ports[i]))
			warnx("cannot connect output port to physical playback port %d", i);
	jack_free(ports);

	/* Signal capture */
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);

	/* Standby */
	pthread_join(jack_client_thread_id(client), NULL);

	return 0;
}