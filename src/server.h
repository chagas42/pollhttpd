/* The server module interface: one exported symbol; the rest is static. */
#pragma once

/* Starts the server on `port` (e.g. "8080") and serves connections until
 * the process ends.
 *
 * Returns -1 if the listening socket could not be opened; the cause has
 * already been logged. Otherwise it does not return. */
int server_run(const char *port);
