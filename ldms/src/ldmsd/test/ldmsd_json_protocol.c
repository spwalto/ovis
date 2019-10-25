#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <semaphore.h>
#include "json/json_util.h"
#include "ldms.h"
#include "../ldmsd_request.h"

sem_t done;

static const char *short_opts = "h:p:x:a:";

static struct option long_opts[] = {
		{ "host",	required_argument, 0, 'h' },
		{ "port",	required_argument, 0, 'p' },
		{ "xprt",	required_argument, 0, 'x' },
		{ "attr",	required_argument, 0, 'a' },
		{ 0,		0,		   0, 0 }
};

static void usage(int argc, char **argv)
{
	printf( "usage: %s -x <xprt> -h <host> -p <port> "
		"	-a <attr>=<value> -a <attr=value> ...\n",
			argv[0]);
	exit(1);
}

struct attr {
	char *name;
	char *value;
	LIST_ENTRY(attr) entry;
};
LIST_HEAD(attr_list, attr) attr_list;

struct attr *attr_new(const char *name, const char *value)
{
	struct attr *attr;
	attr = calloc(1, sizeof(*attr));
	if (!attr)
		return NULL;
	attr->name = strdup(name);
	attr->value = strdup(value);
	LIST_INSERT_HEAD(&attr_list, attr, entry);
	return attr;
}

void attr_free(struct attr *attr)
{
	if (attr->name)
		free(attr->name);
	if (attr->value)
		free(attr->value);
	free(attr);
}

static void handle_err(json_entity_t json)
{
	json_entity_t errcode, msg;
	errcode = json_value_find(json, "errcode");
	if (!errcode) {
		fprintf(stderr, "Missing the mandatory 'errcode'\n");
		assert(0);
	}
	msg = json_value_find(json, "msg");
	if (!msg) {
		fprintf(stderr, "Missing the mandatory 'msg'\n");
		assert(0);
	}
	fprintf(stderr, "Received an error from ldmsd. Error %" PRIu64 ": %s\n",
			json_value_int(errcode), json_value_str(msg)->str);
}

static void handle_info(json_entity_t obj)
{
	json_entity_t name, info, a;
	struct attr *attr;
	name = json_value_find(obj, "name");
	if (!name) {
		fprintf(stderr, "Missing the mandatory attribute 'name'\n");
		assert(0);
	}
	info = json_value_find(obj, "info");
	if (!info) {
		fprintf(stderr, "Missing the mandatory attribute 'info'\n");
		assert(0);
	}

	if (0 != strncmp("example", json_value_str(name)->str, 7)) {
		fprintf(stderr, "Expecting vs received : 'example' vs %s\n",
				json_value_str(name)->str);
		assert(0);
	}

	if (JSON_DICT_VALUE != json_entity_type(info)) {
		fprintf(stderr, "Unexpected json type.\n");
		assert(0);
	}

	attr = LIST_FIRST(&attr_list);
	while (attr) {
		a = json_value_find(info, attr->name);
		if (!a) {
			fprintf(stderr, "Missing attr '%s'\n", attr->name);
			assert(0);
		}
		if (0 != strcmp(json_value_str(a)->str, attr->value)) {
			fprintf(stderr, "Wrong attribute value of %s. "
					"Expected Vs received -- %s vs %s\n",
					attr->name, attr->value,
					json_value_str(a)->str);
			assert(0);
		}
		fprintf(stdout, "	%s=%s\n", attr->name, json_value_str(a)->str);
		LIST_REMOVE(attr, entry);
		attr = LIST_FIRST(&attr_list);
	}
}

static void handle_response(ldmsd_rec_hdr_t hdr)
{
	json_entity_t obj, type;
	json_parser_t parser;
	int rc;
	size_t data_len = hdr->rec_len - sizeof(*hdr);

	parser = json_parser_new(0);
	if (!parser) {
		fprintf(stderr, "Out of memory\n");
		assert(0);
	}
	rc = json_parse_buffer(parser, (char *)(hdr + 1), data_len, &obj);
	if (rc) {
		fprintf(stderr, "Error %d: Failed to parser the received JSON string\n"
				"   %s\n", rc, (char *)(hdr + 1));
		assert(0);
	}
	type = json_value_find(obj, "type");
	if (!type) {
		fprintf(stderr, "Missing the mandatory attribute 'type'\n");
		assert(0);
	}
	if (0 == strncmp("info", json_value_str(type)->str, 4)) {
		handle_info(obj);
	} else if (0 == strncmp("err", json_value_str(type)->str, 3)) {
		handle_err(obj);
	} else {
		fprintf(stderr, "Received and unexpected response type '%s'\n",
						json_value_str(type)->str);
		assert(0);
	}

	sem_post(&done);
}

static void recv_msg(ldms_t x, char *data, size_t data_len)
{
	ldmsd_rec_hdr_t hdr = (ldmsd_rec_hdr_t)data;
	ldmsd_ntoh_rec_hdr(hdr);

	if (hdr->rec_len > ldms_xprt_msg_max(x)) {
		fprintf(stderr, "A record size is larger than the max transport length\n");
		assert(0);
	}

	switch (hdr->type) {
	case LDMSD_MSG_TYPE_RESP:
		/*
		 * Assume that the whole message is in one record.
		 */
		handle_response(hdr);
		break;
	case LDMSD_MSG_TYPE_REQ:
		fprintf(stderr, "unexpectedly received a request\n");
	default:
		assert(0);
	}
}

static int check_truncated(ldmsd_req_buf_t buf, size_t cnt)
{
	if (cnt >= buf->len - buf->off) {
		buf = ldmsd_req_buf_realloc(buf, buf->len * 2);
		if (!buf)
			return ENOMEM;
		return 1;
	}
	return 0;
}

static void send_msg(ldms_t x)
{
	ldmsd_rec_hdr_t hdr;
	ldmsd_req_buf_t buf;
	int num = 0;
	struct attr *attr;
	size_t cnt;
	int rc;

	buf = ldmsd_req_buf_alloc(1024);
	if (!buf) {
		fprintf(stderr, "Out of memory\n");
		exit(ENOMEM);
	}

	hdr = (ldmsd_rec_hdr_t)buf->buf;
	hdr->type = LDMSD_MSG_TYPE_REQ;
	hdr->key.msg_no = 1;
	hdr->key.conn_id = (uint64_t)(long unsigned)x;
	hdr->flags = LDMSD_REC_SOM_F;
	buf->off = sizeof(*hdr);

hdr:
	cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off,
			"{\"type\":\"cmd_obj\","
			" \"cmd\" :\"example\","
			" \"spec\":{");
	if ((rc = check_truncated(buf, cnt))) {
		if (ENOMEM == rc)
			goto oom;
		else
			goto hdr;
	}
	buf->off += cnt;

	LIST_FOREACH(attr, &attr_list, entry) {
		if (num > 0) {
		comma:
			cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off, ",");
			if ((rc = check_truncated(buf, cnt))) {
				if (ENOMEM == rc)
					goto oom;
				else
					goto comma;
			}
			buf->off += cnt;
		}
	attr:
		cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off,
				"\"%s\":\"%s\"",
				attr->name, attr->value);
		if ((rc = check_truncated(buf, cnt))) {
			if (ENOMEM == rc)
				goto oom;
			else
				goto attr;
		}
		buf->off += cnt;
		num++;
	}
close:
	cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off, "}}");
	if ((rc = check_truncated(buf, cnt))) {
		if (ENOMEM == rc)
			goto oom;
		else
			goto close;
	}
	buf->off += cnt;
	hdr->rec_len = buf->off;
	ldmsd_hton_rec_hdr(hdr);
	rc = ldms_xprt_send(x, (char *)hdr, buf->off);
	if (rc) {
		fprintf(stderr, "failed to send the request\n");
		assert(0);
	}

	return;
oom:
	fprintf(stderr, "Out of memory\n");
	exit(ENOMEM);

}

static void event_cb(ldms_t x, ldms_xprt_event_t e, void *cb_arg)
{
	switch (e->type) {
	case LDMS_XPRT_EVENT_CONNECTED:
		fprintf(stdout, "connected\n");
		send_msg(x);
		break;
	case LDMS_XPRT_EVENT_REJECTED:
		fprintf(stdout, "rejected\n");
		break;
	case LDMS_XPRT_EVENT_DISCONNECTED:
		fprintf(stdout, "disconnected\n");
		exit(0);
	case LDMS_XPRT_EVENT_ERROR:
		fprintf(stderr, "connection error\n");
		exit(0);
	case LDMS_XPRT_EVENT_RECV:
		recv_msg(x, e->data, e->data_len);
		break;
	default:
		assert(0);
	}
}

static int connect_ldmsd(const char *host, const char *port, const char *xprt)
{
	ldms_t ldms;
	int rc;
	struct addrinfo *ai;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	rc = getaddrinfo(host, port, &hints, &ai);
	if (rc)
		return EHOSTUNREACH;

	ldms = ldms_xprt_new(xprt, NULL);
	if (!ldms) {
		fprintf(stderr, "Failed to create ldms endpoint\n");
		return ENOMEM;
	}

	rc = ldms_xprt_connect(ldms,ai->ai_addr, ai->ai_addrlen, event_cb, NULL);
	if (rc) {
		ldms_xprt_put(ldms);
		return rc;
	}
	return 0;
}

int main(int argc, char **argv) {
	char *host = "localhost";
	char *port = NULL;
	char *xprt = "sock";
	char *s;
	struct attr *attr = NULL;
	int rc;

	int opt, opt_idx;

	while ((opt = getopt_long(argc, argv,
				short_opts, long_opts,
				&opt_idx)) > 0) {
		switch (opt) {
		case 'h':
			host = strdup(optarg);
			break;
		case 'x':
			xprt = strdup(optarg);
			break;
		case 'p':
			port = strdup(optarg);
			break;
		case 'a':
			s = strchr(optarg, '=');
			s[0] = '\0';
			s++;
			attr = attr_new(optarg, s);
			if (!attr)
				return ENOMEM;
			break;
		default:
			usage(argc, argv);
		}
	}

	if (!host || !port)
		usage(argc, argv);

	sem_init(&done, 0, 0);

	rc = connect_ldmsd(host, port, xprt);
	if (rc) {
		fprintf(stderr, "Failed to connect to ldmsd\n");
		return rc;
	}

	sem_wait(&done);
	fprintf(stdout, "PASSED\n");
	return 0;
}


