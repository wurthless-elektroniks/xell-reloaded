//
// hwconfig dumper for XeLL
// parts of this can probably be integrated to libxenon
//

#include <stdio.h>
#include <string.h>

#include <lwip/tcp.h>
#include <xenon_soc/xenon_io.h>

#include <network/network.h>
#include <xb360/xb360.h>


#include "httpd_hwconfig.h"

struct response_mem_priv_s
{
	void *base;
	int len;
	int ptr, hdr_state;
	int togo;
	char *filename;

    void* data;
};

#define M_GpuReg(reg) (0xE4000000 | (reg << 2))

static const uint32_t REGISTERS_TO_DUMP[] =
{
    // memory controller registers - just SDRAM delay timings for now,
    // more can be added in the future.
    // these are listed in the order that hwinit typically initializes them.
	M_GpuReg(0x0827), // MC0_RD_STR_DLY_0
	M_GpuReg(0x0828), // MC0_RD_STR_DLY_1
	M_GpuReg(0x0867), // MC1_RD_STR_DLY_0
	M_GpuReg(0x0868), // MC1_RD_STR_DLY_1

	M_GpuReg(0x0820), // MC0_WR_STR_DLL_0
	M_GpuReg(0x0821), // MC0_WR_STR_DLL_1
	M_GpuReg(0x0860), // MC1_WR_STR_DLL_0
	M_GpuReg(0x0861), // MC1_WR_STR_DLL_1
};

int response_hwconfig_process_request(struct http_state *http, const char *method, const char *url)
{
	if (strcmp(method, "GET")) return 0;
    if (strcmp(url, "/HWCONFIG") != 0) return 0;

    http->response_priv = mem_malloc(sizeof(struct response_mem_priv_s));
	if (!http->response_priv)
		return 0;
	
    struct response_mem_priv_s *priv = http->response_priv;

    priv->data = mem_malloc(sizeof(REGISTERS_TO_DUMP) * 2);

    void* membase = 0x8000020000000000ull;    
    uint32_t* data_as_ints = (uint32_t*)priv->data;
    for (int i = 0; i < (sizeof(REGISTERS_TO_DUMP) / sizeof(uint32_t)); i++)
    {
        data_as_ints[(i*2)]   = REGISTERS_TO_DUMP[i];
        data_as_ints[(i*2)+1] =  *( (uint32_t*)(membase + REGISTERS_TO_DUMP[i]) );
    }

	priv->hdr_state = 0;
	priv->ptr = 0;
	http->code = 200;

    return 1;
}

int response_hwconfig_do_header(struct http_state *http) {
	struct response_mem_priv_s *priv = http->response_priv;

	const char *t=0, *o=0;
	char buf[80];
	switch (priv->hdr_state)
	{
	case 0:
		t = "Content-Type";
		o = "application/octet-stream";
		break;

	case 1:
		t = "Content-Length";
        sprintf(buf, "%d", sizeof(REGISTERS_TO_DUMP) * 2); 
		o = buf;
		break;
        
	case 2:
		t = "Content-Disposition";
		sprintf(buf, "attachment; filename=hwconfig.bin");
		o = buf;
		break;
	case 3:
		return httpd_do_std_header(http);
	}

	int av = httpd_available_sendbuffer(http);
	if (av < (strlen(t) + strlen(o) + 4))
		return 1;

	httpd_put_sendbuffer_string(http, t);
	httpd_put_sendbuffer_string(http, ": ");
	httpd_put_sendbuffer_string(http, o);
	httpd_put_sendbuffer_string(http, "\r\n");

	++priv->hdr_state;
	return 2;
}

int response_hwconfig_do_data(struct http_state *http)
{
	struct response_mem_priv_s *priv = http->response_priv;


	int av = httpd_available_sendbuffer(http);

	if (!av)
	{
		printf("no httpd sendbuffer space\n");
		return 1;
	}

	if (av > (priv->len - priv->ptr))
		av = priv->len - priv->ptr;

	while (av)
	{
		int maxread = sizeof(REGISTERS_TO_DUMP) * 2;
		if (maxread > av)
			maxread = av;

		httpd_put_sendbuffer(http, (void*)(priv->data + priv->ptr), maxread);

		priv->ptr += maxread;
		av -= maxread;
		priv->togo-=maxread;

		if (priv->togo <= 0){
			return 0;
		}
	}

    return 1;
}


void response_hwconfig_finish(struct http_state *http)
{
	struct response_fuse_priv_s *priv = http->response_priv;
	mem_free(priv->data);
	mem_free(priv);
}
