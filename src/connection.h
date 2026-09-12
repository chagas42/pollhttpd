#pragma once

#include <time.h>

#include "config.h"

// two clocks inside: idle_since slides with activity and only counts
// between messages; deadline is absolute and armed whenever a request or a
// response is in flight, so a trickling client cannot renew it.
typedef struct connection connection;

void  connection_open(connection *conn, int fd,
                      const server_config *cfg, time_t now);
void  connection_close(connection *conn);

int   connection_fd(const connection *conn);

// never both: POLLOUT with nothing to send makes poll() spin
short connection_interest(const connection *conn);

// both return 0 to keep the connection, -1 to drop it
int   connection_on_ready(connection *conn, short revents, time_t now);
int   connection_on_clock(connection *conn, time_t now);
