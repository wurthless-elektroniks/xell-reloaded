
#ifndef HTTPD_HWCONFIG_H_
#define HTTPD_HWCONFIG_H_

#include "httpd/httpd.h"

int response_hwconfig_process_request(struct http_state *http, const char *method, const char *url);
int response_hwconfig_do_header(struct http_state *http);
int response_hwconfig_do_data(struct http_state *http);
void response_hwconfig_finish(struct http_state *http);

#endif /* HTTPD_HWCONFIG_H_ */

