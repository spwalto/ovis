/* -*- c-basic-offset: 8 -*-
 * Copyright (c) 2015-2018 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS). Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 * Copyright (c) 2015-2018 Open Grid Computing, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of Sandia nor the names of any contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *      Neither the name of Open Grid Computing nor the names of any
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *      Modified source versions must be plainly marked as such, and
 *      must not be misrepresented as being the original software.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <coll/rbt.h>
#include <pthread.h>
#include <ovis_util/util.h>
#include <json/json_util.h>
#include "ldms.h"
#include "ldmsd.h"
#include "ldmsd_plugin.h"
#include "ldmsd_sampler.h"
#include "ldmsd_store.h"
#include "ldmsd_request.h"
//#include "ldmsd_stream.h"
#include "ldms_xprt.h"

/*
 * This file implements an LDMSD control protocol. The protocol is
 * message oriented and has message boundary markers.
 *
 * Every message has a unique msg_no identifier. Every record that is
 * part of the same message has the same msg_no value. The flags field
 * is a bit field as follows:
 *
 * 1 - Start of Message
 * 2 - End of Message
 *
 * The rec_len field is the size of the record including the header.
 * It is assumed that when reading from the socket that the next
 * message starts at cur_ptr + rec_len when cur_ptr starts at 0 and is
 * incremented by the read length for each socket operation.
 *
 * When processing protocol records, the header is stripped off and
 * all reqresp strings that share the same msg_no are concatenated
 * together until the record in which flags | End of Message is True
 * is received and then delivered to the ULP as a single message
 *
 */

int ldmsd_req_debug = 0; /* turn on / off using gdb or edit src to
                                 * see request/response debugging messages */

/*
 * TODO: Uncomment this when refactoring the quit handler
 */
//static int cleanup_requested = 0;

void __ldmsd_log(enum ldmsd_loglevel level, const char *fmt, va_list ap);

__attribute__((format(printf, 1, 2)))
static inline
void __dlog(const char *fmt, ...)
{
	if (!ldmsd_req_debug)
		return;
	va_list ap;
	va_start(ap, fmt);
	__ldmsd_log(LDMSD_LALL, fmt, ap);
	va_end(ap);
}

static int msg_comparator(void *a, const void *b)
{
	ldmsd_msg_key_t ak = (ldmsd_msg_key_t)a;
	ldmsd_msg_key_t bk = (ldmsd_msg_key_t)b;
	int rc;

	rc = ak->conn_id - bk->conn_id;
	if (rc)
		return rc;
	return ak->msg_no - bk->msg_no;
}
struct rbt req_msg_tree = RBT_INITIALIZER(msg_comparator);
struct rbt rsp_msg_tree = RBT_INITIALIZER(msg_comparator);
pthread_mutex_t req_msg_tree_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rsp_msg_tree_lock = PTHREAD_MUTEX_INITIALIZER;

static
void ldmsd_req_ctxt_sec_get(ldmsd_req_ctxt_t rctxt, ldmsd_sec_ctxt_t sctxt)
{
	switch (rctxt->xprt->type) {
	case LDMSD_CFG_XPRT_CONFIG_FILE:
		ldmsd_sec_ctxt_get(sctxt);
		break;
	case LDMSD_CFG_XPRT_LDMS:
		ldms_xprt_cred_get(rctxt->xprt->xprt, NULL, &sctxt->crd);
		break;
	}
}

typedef int (*ldmsd_obj_handler_t)(ldmsd_req_ctxt_t reqc);
struct request_handler_entry {
	int req_id;
	ldmsd_obj_handler_t handler;
	int flag; /* Lower 12 bit (mask 0777) for request permisson.
		   * The rest is reserved for ldmsd_request use. */
};

struct obj_handler_entry {
	const char *name;
	ldmsd_obj_handler_t handler;
	int flag; /* Lower 12 bit (mask 0777) for request permission.
	   * The rest is reserved for ldmsd_request use. */
};

/*
 * Command object handlers
 */
static int example_handler(ldmsd_req_ctxt_t req_ctxt);
static int greeting_handler(ldmsd_req_ctxt_t req_ctxt);
static int include_handler(ldmsd_req_ctxt_t req_ctxt);
static int plugin_status_handler(ldmsd_req_ctxt_t reqc);
static int set_route_handler(ldmsd_req_ctxt_t req_ctxt);
static int smplr_status_handler(ldmsd_req_ctxt_t reqc);

/*
 * Configuration object handlers
 */
static int auth_handler(ldmsd_req_ctxt_t reqc);
static int daemon_handler(ldmsd_req_ctxt_t reqc);
static int listen_handler(ldmsd_req_ctxt_t reqc);
static int plugin_instance_handler(ldmsd_req_ctxt_t reqc);
static int smplr_handler(ldmsd_req_ctxt_t req_ctxt);

//static int smplr_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int smplr_start_handler(ldmsd_req_ctxt_t req_ctxt);
//static int smplr_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_start_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_start_regex_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_stop_regex_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_status_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_set_status_handler(ldmsd_req_ctxt_t req_ctxt);
//static int prdcr_subscribe_regex_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_start_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_prdcr_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_prdcr_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_metric_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_metric_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int strgp_status_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_prdcr_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_prdcr_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_match_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_match_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_start_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//static int updtr_status_handler(ldmsd_req_ctxt_t req_ctxt);
//static int plugn_list_handler(ldmsd_req_ctxt_t req_ctxt);
//static int plugn_sets_handler(ldmsd_req_ctxt_t req_ctxt);
//static int plugn_usage_handler(ldmsd_req_ctxt_t req_ctxt);
//static int plugn_query_handler(ldmsd_req_ctxt_t req_ctxt);
//static int set_udata_handler(ldmsd_req_ctxt_t req_ctxt);
//static int set_udata_regex_handler(ldmsd_req_ctxt_t req_ctxt);
//static int verbosity_change_handler(ldmsd_req_ctxt_t reqc);
//static int daemon_status_handler(ldmsd_req_ctxt_t reqc);
//static int version_handler(ldmsd_req_ctxt_t reqc);
//static int env_handler(ldmsd_req_ctxt_t req_ctxt);
//static int oneshot_handler(ldmsd_req_ctxt_t req_ctxt);
//static int logrotate_handler(ldmsd_req_ctxt_t req_ctxt);
//static int exit_daemon_handler(ldmsd_req_ctxt_t req_ctxt);
//static int unimplemented_handler(ldmsd_req_ctxt_t req_ctxt);
//static int eperm_handler(ldmsd_req_ctxt_t req_ctxt);
//static int ebusy_handler(ldmsd_req_ctxt_t reqc);

//static int export_config_handler(ldmsd_req_ctxt_t reqc);
//
///* these are implemented in ldmsd_failover.c */
//int failover_config_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_peercfg_start_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_peercfg_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_mod_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_status_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_pair_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_reset_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_cfgprdcr_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_cfgupdtr_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_cfgstrgp_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_ping_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_peercfg_handler(ldmsd_req_ctxt_t req);
//
//int failover_start_handler(ldmsd_req_ctxt_t req_ctxt);
//int failover_stop_handler(ldmsd_req_ctxt_t req_ctxt);
//
//static int setgroup_add_handler(ldmsd_req_ctxt_t req_ctxt);
//static int setgroup_mod_handler(ldmsd_req_ctxt_t req_ctxt);
//static int setgroup_del_handler(ldmsd_req_ctxt_t req_ctxt);
//static int setgroup_ins_handler(ldmsd_req_ctxt_t req_ctxt);
//static int setgroup_rm_handler(ldmsd_req_ctxt_t req_ctxt);
//
//static int stream_publish_handler(ldmsd_req_ctxt_t req_ctxt);
//static int stream_subscribe_handler(ldmsd_req_ctxt_t reqc);
//
//static int auth_del_handler(ldmsd_req_ctxt_t reqc);

/* executable for all */
#define XALL 0111
/* executable for user, and group */
#define XUG 0110

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static int handler_entry_comp(const void *a, const void *b)
{
	struct obj_handler_entry *_a = (struct obj_handler_entry *)a;
	struct obj_handler_entry *_b = (struct obj_handler_entry *)b;
	return strcmp(_a->name, _b->name);
}

/*
 * TODO: Fill in the flag field of all obj_handler_entry in all tables.
 */
static struct obj_handler_entry cfg_obj_handler_tbl[] = {
		{ "auth",		auth_handler,			XUG },
//		{ "env",	env_handler    },
		{ "daemon",		daemon_handler,			XUG },
		{ "listen",		listen_handler,			XUG },
		{ "plugin_instance",	plugin_instance_handler,	XUG },
		{ "smplr",		smplr_handler,			XUG },
//		{ "prdcr",	prdcr_add_handler },
//		{ "updtr",	updtr_add_handler },
//		{ "strgp",	strgp_add_handler },
//		{ "setgroup",	setgroup_add_handler },
//		{ "failover",	failover_config_handler }
};

static struct obj_handler_entry cmd_obj_handler_tbl[] = {
		{ "example", 	example_handler, 	XALL },
		{ "greeting",	greeting_handler,	XALL },
		{ "include",	include_handler,	XUG },
		{ "plugin_status",	plugin_status_handler,	XALL },
		{ "set_route",	set_route_handler, 	XUG },
		{ "smplr_status",	smplr_status_handler,	XALL },
};

static struct obj_handler_entry act_obj_handler_tbl[] = {
};

/*
 * The process request function takes records and collects
 * them into messages. These messages are then delivered to the req_id
 * specific handlers.
 *
 * The assumptions are the following:
 * 1. msg_no is unique on the socket
 * 2. There may be multiple messages outstanding on the same socket
 */
static ldmsd_req_ctxt_t find_req_ctxt(struct ldmsd_msg_key *key, int type)
{
	ldmsd_req_ctxt_t rm = NULL;
	struct rbt *tree;
	struct rbn *rbn;

	if (LDMSD_REQ_CTXT_RSP == type)
		tree = &rsp_msg_tree;
	else
		tree = &req_msg_tree;
	rbn = rbt_find(tree, key);
	if (rbn)
		rm = container_of(rbn, struct ldmsd_req_ctxt, rbn);
	return rm;
}

void ldmsd_req_ctxt_tree_lock(int type)
{
	if (LDMSD_REQ_CTXT_REQ == type)
		pthread_mutex_lock(&req_msg_tree_lock);
	else
		pthread_mutex_lock(&rsp_msg_tree_lock);
}

void ldmsd_req_ctxt_tree_unlock(int type)
{
	if (LDMSD_REQ_CTXT_REQ == type)
		pthread_mutex_unlock(&req_msg_tree_lock);
	else
		pthread_mutex_unlock(&rsp_msg_tree_lock);
}

/*
 * Caller must hold the tree lock
 */
ldmsd_req_ctxt_t ldmsd_req_ctxt_first(int type)
{
	ldmsd_req_ctxt_t reqc;
	struct rbn *rbn;
	struct rbt *tree = (type == LDMSD_REQ_CTXT_REQ)?&req_msg_tree:&rsp_msg_tree;
	rbn = rbt_min(tree);
	if (!rbn)
		return NULL;
	reqc = container_of(rbn, struct ldmsd_req_ctxt, rbn);
	return reqc;
}

/*
 * Caller must hold the tree lock
 */
ldmsd_req_ctxt_t ldmsd_req_ctxt_next(ldmsd_req_ctxt_t reqc)
{
	ldmsd_req_ctxt_t next;
	struct rbn *rbn;
	rbn = rbn_succ(&reqc->rbn);
	if (!rbn)
		return NULL;
	next = container_of(rbn, struct ldmsd_req_ctxt, rbn);
	return next;
}

/* The caller must _not_ hold the msg_tree lock. */
void __req_ctxt_del(ldmsd_req_ctxt_t reqc)
{
	ldmsd_cfg_xprt_ref_put(reqc->xprt, "req_ctxt");
	if (reqc->recv_buf)
		ldmsd_req_buf_free(reqc->recv_buf);
	if (reqc->send_buf)
		ldmsd_req_buf_free(reqc->send_buf);
	free(reqc);
}

int cfg_msg_ctxt_free_actor(ev_worker_t src, ev_worker_t dst, ev_status_t status, ev_t ev)
{
	ldmsd_req_ctxt_t reqc;
	reqc = EV_DATA(ev, struct msg_ctxt_free_data)->reqc;
	ldmsd_req_ctxt_ref_put(reqc, "create");
	return 0;
}

void __msg_key_get(ldmsd_cfg_xprt_t xprt, uint32_t msg_no,
						ldmsd_msg_key_t key_)
{
	key_->msg_no = msg_no;
	if (xprt->type == LDMSD_CFG_XPRT_LDMS) {
		/*
		 * Don't use the cfg_xprt directly because
		 * a new cfg_xprt get allocated
		 * every time LDMSD receives a record.
		 */
		key_->conn_id = (uint64_t)(unsigned long)xprt->ldms.ldms;
	} else {
		key_->conn_id = (uint64_t)(unsigned long)xprt;
	}
}

/*
 * max_msg_len must be a positive number.
 *
 * The caller must hold the msg_tree lock.
 */
ldmsd_req_ctxt_t
__req_ctxt_alloc(ldmsd_msg_key_t key, ldmsd_cfg_xprt_t xprt, int type)
{
	ldmsd_req_ctxt_t reqc;

	reqc = calloc(1, sizeof *reqc);
	if (!reqc)
		return NULL;

	reqc->recv_buf = ldmsd_req_buf_alloc(xprt->max_msg);
	if (!reqc->recv_buf)
		goto err;

	reqc->send_buf = ldmsd_req_buf_alloc(xprt->max_msg);
	if (!reqc->send_buf)
		goto err;

	ldmsd_cfg_xprt_ref_get(xprt, "req_ctxt");
	reqc->xprt = xprt;

	reqc->free_ev = ev_new(cfg_msg_ctxt_free_type);
	EV_DATA(reqc->free_ev, struct msg_ctxt_free_data)->reqc = reqc;

	ref_init(&reqc->ref, "create", (ref_free_fn_t)__req_ctxt_del, reqc);
	reqc->key = *key;
	rbn_init(&reqc->rbn, &reqc->key);
	reqc->type = type;
	if (LDMSD_REQ_CTXT_RSP == type) {
		rbt_ins(&rsp_msg_tree, &reqc->rbn);
	} else {
		rbt_ins(&req_msg_tree, &reqc->rbn);
	}
	return reqc;
 err:
 	__req_ctxt_del(reqc);
	return NULL;
}

/**
 * Allocate a request message context.
 */
ldmsd_req_ctxt_t
ldmsd_req_ctxt_alloc(struct ldmsd_msg_key *key, ldmsd_cfg_xprt_t xprt)
{
	return __req_ctxt_alloc(key, xprt, LDMSD_REQ_CTXT_RSP);
}

int ldmsd_req_ctxt_free(ldmsd_req_ctxt_t reqc)
{
	ldmsd_req_ctxt_tree_lock(reqc->type);
	if (LDMSD_REQ_CTXT_REQ == reqc->type)
		rbt_del(&req_msg_tree, &reqc->rbn);
	else
		rbt_del(&rsp_msg_tree, &reqc->rbn);
	ldmsd_req_ctxt_tree_unlock(reqc->type);
	return ev_post(cfg, cfg, reqc->free_ev, NULL);
}

int __ldmsd_append_buffer(ldmsd_cfg_xprt_t xprt, struct ldmsd_msg_key *key,
			ldmsd_req_buf_t buf,
			int msg_flags, int msg_type,
			const char *data, size_t data_len)
{
	return ldmsd_append_msg_buffer(xprt, xprt->max_msg, key,
				(ldmsd_msg_send_fn_t)xprt->send_fn,
				buf, msg_flags, msg_type, data, data_len);
}

int __ldmsd_append_buffer_va(ldmsd_cfg_xprt_t xprt, struct ldmsd_msg_key *key,
				ldmsd_req_buf_t buf, int msg_flags,
				int msg_type, const char *fmt, va_list _ap)
{
	char *str = NULL;
	va_list ap;
	size_t cnt;

	va_copy(ap, _ap);
	cnt = vsnprintf(str, 0, fmt, ap);
	va_end(ap);
	str = malloc(cnt + 1);
	if (!str)
		return ENOMEM;
	va_copy(ap, _ap);
	cnt = vsnprintf(str, cnt + 1, fmt, ap);
	va_end(ap);
	return ldmsd_append_msg_buffer(xprt, xprt->max_msg, key,
					(ldmsd_msg_send_fn_t)xprt->send_fn,
					buf, msg_flags, msg_type, str, cnt);
}

int ldmsd_append_response_va(ldmsd_req_ctxt_t reqc, int msg_flags, const char *fmt, ...)
{
	int rc;
	va_list ap;
	va_start(ap, fmt);
	rc = __ldmsd_append_buffer_va(reqc->xprt, &reqc->key, reqc->send_buf,
					msg_flags, LDMSD_MSG_TYPE_RESP, fmt, ap);
	va_end(ap);
	return rc;
}

int ldmsd_append_response(ldmsd_req_ctxt_t reqc, int msg_flags,
				const char *data, size_t data_len)
{
	return __ldmsd_append_buffer(reqc->xprt, &reqc->key, reqc->send_buf,
			msg_flags, LDMSD_MSG_TYPE_RESP, data, data_len);
}

int ldmsd_append_request_va(ldmsd_req_ctxt_t reqc, int msg_flags, const char *fmt, ...)
{
	int rc;
	va_list ap;
	va_start(ap, fmt);
	rc = __ldmsd_append_buffer_va(reqc->xprt, &reqc->key, reqc->send_buf,
			msg_flags, LDMSD_MSG_TYPE_REQ, fmt, ap);
	va_end(ap);
	return rc;
}

int ldmsd_append_request(ldmsd_req_ctxt_t reqc, int msg_flags,
				const char *data, size_t data_len)
{
	return __ldmsd_append_buffer(reqc->xprt, &reqc->key, reqc->send_buf,
			msg_flags, LDMSD_MSG_TYPE_REQ, data, data_len);
}

int __send_error(ldmsd_cfg_xprt_t xprt, struct ldmsd_msg_key *key,
				ldmsd_req_buf_t buf, uint32_t errcode,
				const char *errmsg_fmt, va_list errmsg_ap)
{
	int rc;
	char errcode_s[16];
	char *str;

	str = "{\"type\":\"err_obj\",\"errcode\":";
	rc = __ldmsd_append_buffer(xprt, key, buf,
				LDMSD_REC_SOM_F, LDMSD_MSG_TYPE_RESP,
				str, strlen(str));
	if (rc)
		return rc;
	snprintf(errcode_s, 16, "%d", errcode);
	rc = __ldmsd_append_buffer(xprt, key, buf, 0, LDMSD_MSG_TYPE_RESP,
			errcode_s, strlen(errcode_s));
	if (rc)
		return rc;
	if (0 == errcode) {
		rc = __ldmsd_append_buffer(xprt, key, buf, LDMSD_REC_EOM_F,
				LDMSD_MSG_TYPE_RESP, "}", 1);
	} else {
		str = ",\"msg\":\"";
		rc = __ldmsd_append_buffer(xprt, key, buf, 0,
				LDMSD_MSG_TYPE_RESP, str, strlen(str));
		if (rc)
			return rc;
		rc = __ldmsd_append_buffer_va(xprt, key, buf, 0,
				LDMSD_MSG_TYPE_RESP, errmsg_fmt, errmsg_ap);
		if (rc)
			return rc;
		rc = __ldmsd_append_buffer(xprt, key, buf, LDMSD_REC_EOM_F,
				LDMSD_MSG_TYPE_RESP, "\"}", 2);
	}
	return rc;
}

/*
 * Any errors occur in any handler function must call \c ldmsd_send_error instead.
 *
 * if \c buf is NULL, the function will create its own buffer.
 *
 * Call the function only once to construct and send an error.
 */
int __ldmsd_send_error(ldmsd_cfg_xprt_t xprt, uint32_t msg_no,
				ldmsd_req_buf_t _buf, uint32_t errcode,
				char *errmsg_fmt, ...)
{
	va_list errmsg_ap;
	ldmsd_req_buf_t buf;
	struct ldmsd_msg_key key;
	int rc = 0;

	__msg_key_get(xprt, msg_no, &key);
	if (_buf) {
		buf = _buf;
	} else {
		buf = ldmsd_req_buf_alloc(xprt->max_msg);
		if (!buf)
			return ENOMEM;
	}

	va_start(errmsg_ap, errmsg_fmt);
	rc = __send_error(xprt, &key, buf, errcode, errmsg_fmt, errmsg_ap);
	if (!_buf)
		ldmsd_req_buf_free(buf);
	va_end(errmsg_ap);
	return rc;
}

/*
 * A convenient function that constructs and sends the error JSON object string.
 *
 * { "type": "error",
 *   "errcode": \c errcode,
 *   "msg": <string the same as the string printed by printf(errmsg_fmt, ...)>
 * }
 */

int ldmsd_send_error(ldmsd_req_ctxt_t reqc, uint32_t errcode, char *errmsg_fmt, ...)
{
	va_list errmsg_ap;
	int rc = 0;

	/*
	 * Clear any existing content in the send buffer.
	 */
	ldmsd_req_buf_reset(reqc->send_buf);
	va_start(errmsg_ap, errmsg_fmt);
	rc = __send_error(reqc->xprt, &reqc->key, reqc->send_buf, errcode,
					errmsg_fmt, errmsg_ap);
	va_end(errmsg_ap);
	return rc;
}

/*
 * A convenient function that constructs and sends the error response
 * caused by a required attribute is missing.
 */
int ldmsd_send_missing_attr_err(ldmsd_req_ctxt_t reqc,
					const char *obj_name,
					const char *missing_attr)
{
	int rc;

	rc = ldmsd_send_error(reqc, EINVAL, "'%s' is missing from the '%s' request.",
			missing_attr, obj_name);
	return rc;
}

int ldmsd_send_type_error(ldmsd_req_ctxt_t reqc, const char *obj_name,
							const char *type)
{
	return ldmsd_send_error(reqc, EINVAL, "Wrong JSON value type. %s must be %s.", obj_name, type);
}

int __ldmsd_send_missing_mandatory_attr(ldmsd_req_ctxt_t reqc,
					const char *obj_type,
					const char *missing)
{
	int rc;
	ldmsd_log(LDMSD_LERROR, "'%s' (%s) is missing from "
			"the message number %d:%" PRIu64,
			missing, obj_type,
			reqc->key.msg_no, reqc->key.conn_id);
	rc = ldmsd_send_error(reqc, EINVAL,
			"'%s' (%s) is missing from "
			"the message number %d:%" PRIu64,
			missing, obj_type,
			reqc->key.msg_no, reqc->key.conn_id);
	return rc;
}

int
ldmsd_send_err_rec_adv(ldmsd_cfg_xprt_t xprt, uint32_t msg_no, uint32_t rec_len)
{
	size_t cnt, hdr_sz, len;
	int rc;
	char *buf;
	ldmsd_rec_hdr_t hdr;

	hdr_sz = sizeof(struct ldmsd_rec_hdr_s);
	len = hdr_sz + 1024;

	buf = calloc(1, len);
	if (!buf)
		return ENOMEM;
	hdr = (ldmsd_rec_hdr_t)buf;
	cnt = snprintf(&buf[hdr_sz], len - hdr_sz, "{\"rsp\":\"rec_adv\","
						   " \"spec\": {\"errcode\":%d,"
						   "\"rec_len\":%d}}",
						   E2BIG, rec_len);
	if (cnt >= len - hdr_sz) {
		free(buf);
		buf = malloc(len + cnt);
		if (!buf)
			return ENOMEM;
	}
	cnt = snprintf(&buf[hdr_sz], len - hdr_sz, "{\"rsp\":\"rec_adv\","
						   " \"spec\": {\"errcode\":%d,"
						   "\"rec_len\":%d}}",
						   E2BIG, rec_len);
	hdr->msg_no = msg_no;
	hdr->rec_len = cnt + hdr_sz;
	hdr->type = LDMSD_MSG_TYPE_RESP;
	hdr->flags = LDMSD_REC_SOM_F | LDMSD_REC_EOM_F;
	ldmsd_hton_rec_hdr(hdr);
	rc = xprt->send_fn(xprt, (char *)hdr, cnt + hdr_sz);
	if (rc) {
		/* The content in reqc->rep_buf hasn't been sent. */
		ldmsd_log(LDMSD_LERROR, "failed to send the record length advice "
				"to the ldms xprt 0x%p\n", xprt->ldms.ldms);
		goto out;
	}
out:
	if (buf)
		free(buf);
	return rc;
}

int ldmsd_process_msg_request(ldmsd_rec_hdr_t req, ldmsd_cfg_xprt_t xprt)
{
	ldmsd_req_ctxt_t reqc = NULL;
	char *oom_errstr = "ldmsd out of memory";
	char *repl_str;
	int rc = 0;
	json_parser_t parser;
	size_t repl_str_len;
	int req_ctxt_type = LDMSD_REQ_CTXT_REQ;
	struct ldmsd_msg_key key;
	errno = 0;

	__msg_key_get(xprt, req->msg_no, &key);
	ldmsd_req_ctxt_tree_lock(req_ctxt_type);
	/*
	 * Copy the data from this record to the tail of the buffer
	 */
	if (req->flags & LDMSD_REC_SOM_F) {
		reqc = find_req_ctxt(&key, LDMSD_REQ_CTXT_REQ);
		if (reqc) {
			rc = ldmsd_send_error(reqc, EADDRINUSE,
				"Duplicate message number %" PRIu32,
				req->msg_no);
			if (rc == ENOMEM)
				goto oom;
			else
				goto err;
		}
		reqc = __req_ctxt_alloc(&key, xprt, LDMSD_REQ_CTXT_REQ);
		if (!reqc)
			goto oom;
	} else {
		reqc = find_req_ctxt(&key, LDMSD_REQ_CTXT_REQ);
		if (!reqc) {
			rc = __ldmsd_send_error(xprt, req->msg_no, NULL, ENOENT,
					"The message number %" PRIu32
					" was not found.", req->msg_no);
			ldmsd_log(LDMSD_LERROR, "The request ID %" PRIu32 ":%" PRIu64
					" was not found.\n",
					req->msg_no, key.conn_id);
			goto err;
		}
	}
	repl_str = str_repl_env_vars((const char *)(req + 1));
	if (!repl_str) {
		reqc = NULL;
		goto oom;
	}
	repl_str_len = strlen(repl_str);
	if (reqc->recv_buf->len - reqc->recv_buf->off < repl_str_len) {
		reqc->recv_buf = ldmsd_req_buf_realloc(reqc->recv_buf,
					2 * (reqc->recv_buf->off + repl_str_len));
		if (!reqc->recv_buf) {
			reqc = NULL;
			goto oom;
		}
	}
	memcpy(&reqc->recv_buf->buf[reqc->recv_buf->off], repl_str, repl_str_len);
	free(repl_str);
	reqc->recv_buf->off += repl_str_len;

	ldmsd_req_ctxt_tree_unlock(req_ctxt_type);

	if (!(req->flags & LDMSD_REC_EOM_F)) {
		/*
		 * LDMSD hasn't received the whole message.
		 */
		goto out;
	}

	parser = json_parser_new(0);
	if (!parser)
		goto oom;

	rc = json_parse_buffer(parser, reqc->recv_buf->buf,
			reqc->recv_buf->off, &reqc->json);

	if (rc) {
		ldmsd_log(LDMSD_LCRITICAL, "Failed to parse a JSON object string\n");
		__ldmsd_send_error(reqc->xprt, req->msg_no, reqc->send_buf, rc,
				"Failed to parse a JSON object string");
		goto err;
	}

	rc = ldmsd_process_json_obj(reqc);
out:
	return rc;

oom:
	rc = ENOMEM;
	ldmsd_log(LDMSD_LCRITICAL, "%s\n", oom_errstr);
	__ldmsd_send_error(xprt, req->msg_no, NULL, rc, "%s", oom_errstr);
err:
	ldmsd_req_ctxt_tree_unlock(req_ctxt_type);
	if (reqc)
		ldmsd_req_ctxt_free(reqc);
	return rc;
}

int ldmsd_process_msg_response(ldmsd_rec_hdr_t req, ldmsd_cfg_xprt_t xprt)
{
	ldmsd_req_ctxt_t reqc = NULL;
	char *oom_errstr = "ldmsd out of memory";
	char *repl_str;
	int rc = 0;
	json_parser_t parser;
	int req_ctxt_type = LDMSD_REQ_CTXT_RSP;
	size_t repl_str_len;
	struct ldmsd_msg_key key;

	__msg_key_get(xprt, req->msg_no, &key);
	errno = 0;
	ldmsd_req_ctxt_tree_lock(req_ctxt_type);
	/*
	 * Use the key sent by the peer because this is a response
	 * to a REQUEST message sent by this LDMSD.
	 */
	reqc = find_req_ctxt(&key, LDMSD_REQ_CTXT_RSP);
	if (!reqc) {
		ldmsd_log(LDMSD_LERROR, "Cannot find the original request of "
				"a response number %d:%" PRIu64 "\n",
				key.msg_no, key.conn_id);
		rc = __ldmsd_send_error(xprt, req->msg_no, NULL, ENOENT,
			"Cannot find the original request of "
			"a response number %d:%" PRIu64,
			key.msg_no, key.conn_id);
		if (rc == ENOMEM)
			goto oom;
		else
			goto err;
	}
	ldmsd_req_ctxt_ref_get(reqc, "resp:lookup");
	/*
	 * Copy the data from this record to the tail of the receive buffer
	 */
	repl_str = str_repl_env_vars((const char *)(req + 1));
	if (!repl_str) {
		reqc = NULL;
		goto oom;
	}
	repl_str_len = strlen(repl_str);
	if (reqc->recv_buf->len - reqc->recv_buf->off < repl_str_len) {
		reqc->recv_buf = ldmsd_req_buf_realloc(reqc->recv_buf,
					2 * (reqc->recv_buf->off + repl_str_len));
		if (!reqc->recv_buf)
			goto oom;
	}
	memcpy(&reqc->recv_buf->buf[reqc->recv_buf->off], repl_str, repl_str_len);
	free(repl_str);
	reqc->recv_buf->off += repl_str_len;

	ldmsd_req_ctxt_tree_unlock(req_ctxt_type);

	if (!(req->flags & LDMSD_REC_EOM_F)) {
		/*
		 * LDMSD hasn't received the whole message.
		 */
		goto out;
	}

	parser = json_parser_new(0);
	if (!parser)
		goto oom;

	rc = json_parse_buffer(parser, reqc->recv_buf->buf,
			reqc->recv_buf->off, &reqc->json);

	if (rc) {
		ldmsd_log(LDMSD_LCRITICAL, "Failed to parse a JSON object string\n");
		__ldmsd_send_error(reqc->xprt, req->msg_no, reqc->send_buf, rc,
				"Failed to parse a JSON object string");
		goto err;
	}

	if (reqc->resp_handler) {
		rc = reqc->resp_handler(reqc);
	} else {
		rc = ldmsd_process_json_obj(reqc);
	}

	ldmsd_req_ctxt_ref_put(reqc, "resp:lookup");
out:
	return rc;

oom:
	rc = ENOMEM;
	ldmsd_log(LDMSD_LCRITICAL, "%s\n", oom_errstr);
err:
	ldmsd_req_ctxt_tree_unlock(req_ctxt_type);
	if (reqc)
		ldmsd_req_ctxt_ref_put(reqc, "resp:lookup");
	return rc;
}

int ldmsd_process_err_obj(ldmsd_req_ctxt_t reqc)
{
	json_entity_t errcode, msg;
	char *s;

	errcode = json_value_find(reqc->json, "errcode");
	if (!errcode) {
		ldmsd_log(LDMSD_LERROR, "Msg ID %d:%" PRIu64 ": "
				"The response err_obj has no errcode.\n",
				reqc->key.msg_no, reqc->key.conn_id);
		return 0;
	}
	if (json_value_int(errcode) != 0) {
		msg = json_value_find(reqc->json, "msg");
		if (msg)
			s = json_value_str(msg)->str;
		else
			s = NULL;
		ldmsd_log(LDMSD_LERROR, "Msg ID %d:%" PRIu64 ": received "
				"an error response '%" PRIu64 "': '%s'.\n",
				reqc->key.msg_no, reqc->key.conn_id,
				json_value_int(errcode), s);
	}
	ldmsd_req_ctxt_free(reqc);
	return 0;
}

int process_unexpected_info_obj(ldmsd_req_ctxt_t reqc)
{
	json_entity_t name;

	name = json_value_find(reqc->json, "name");
	if (!name || (JSON_STRING_VALUE != json_entity_type(name))) {
		ldmsd_log(LDMSD_LERROR, "Received an unexpected info object without a name.\n");
	} else {
		ldmsd_log(LDMSD_LERROR, "Received an unexpected info object '%s'\n",
							json_value_str(name)->str);
	}
	__dlog("%s\n", reqc->recv_buf->buf);
	ldmsd_req_ctxt_free(reqc);
	return 0;
}

int ldmsd_process_cfg_obj(ldmsd_req_ctxt_t reqc)
{
	int rc;
	json_entity_t cfg_obj, spec, op;
	struct obj_handler_entry *handler;
	char *cfg_obj_s, *op_s;

	cfg_obj = json_value_find(reqc->json, "cfg_obj");
	if (!cfg_obj) {
		rc = __ldmsd_send_missing_mandatory_attr(reqc, "cfg_obj", "cfg_obj");
		return rc;
	}

	if (JSON_STRING_VALUE != json_entity_type(cfg_obj)) {
		rc = ldmsd_send_error(reqc, EINVAL, "cfg_obj:cfg_obj must be a JSON string.");
		return rc;
	}

	cfg_obj_s = json_value_str(cfg_obj)->str;
	spec = json_value_find(reqc->json, "spec");
	if (!spec) {
		rc = __ldmsd_send_missing_mandatory_attr(reqc, "cfg_obj", "spec");
		return rc;
	}
	op = json_value_find(reqc->json, "op");
	if (op) {
		if (JSON_STRING_VALUE != json_entity_type(op)) {
			rc = ldmsd_send_error(reqc, EINVAL, "cfg_obj:op must be a string.");
			return rc;
		}
		op_s = json_value_str(op)->str;
		if ((0 != strncmp("create", op_s, 6)) && (0 != strncmp("update", op_s, 6))) {
			rc = ldmsd_send_error(reqc, EINVAL, "cfg_obj:op invalid value (%s)", op_s);
			return rc;
		}
	}

	handler = bsearch(&cfg_obj_s, cfg_obj_handler_tbl,
			ARRAY_SIZE(cfg_obj_handler_tbl),
			sizeof(*handler), handler_entry_comp);
	if (!handler) {
		ldmsd_log(LDMSD_LERROR, "Received an unrecognized "
				"configuration object '%s'\n", cfg_obj_s);
		rc = ldmsd_send_error(reqc, ENOTSUP, "Received an unrecognized "
				"configuration object '%s'\n", cfg_obj_s);
		return rc;
	}

	rc = handler->handler(reqc);
	ldmsd_req_ctxt_free(reqc);
	return rc;
}

int ldmsd_process_cmd_obj(ldmsd_req_ctxt_t reqc)
{
	int rc;
	json_entity_t cmd, spec;
	struct obj_handler_entry *handler;
	char *cmd_s;

	cmd = json_value_find(reqc->json, "cmd");
	if (!cmd) {
		rc = __ldmsd_send_missing_mandatory_attr(reqc, "cmd_obj", "cmd");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(cmd)) {
		rc = ldmsd_send_error(reqc, EINVAL, "cmd_obj:cmd must be a JSON string.");
		return rc;
	}
	cmd_s = json_value_str(cmd)->str;
	spec = json_value_find(reqc->json, "spec");
	if (spec && (JSON_DICT_VALUE != json_entity_type(spec))) {
		rc = ldmsd_send_type_error(reqc, "cmd_obj:spec", "a dictionary");
		return rc;
	}

	handler = bsearch(&cmd_s, cmd_obj_handler_tbl,
			ARRAY_SIZE(cmd_obj_handler_tbl),
			sizeof(*handler), handler_entry_comp);
	if (!handler) {
		ldmsd_log(LDMSD_LERROR, "Received an unrecognized "
				"command object '%s'\n", cmd_s);
		rc = ldmsd_send_error(reqc, ENOTSUP, "Received an unrecognized "
				"command object '%s'\n", cmd_s);
		return rc;
	}

	rc = handler->handler(reqc);
	ldmsd_req_ctxt_free(reqc);
	return rc;
}

int ldmsd_process_act_obj(ldmsd_req_ctxt_t reqc)
{
	int rc;
	json_entity_t value, names, regex;
	char *action_s, *cfgobj_s;
	struct obj_handler_entry *handler;
	size_t len;

	/* cfg_obj */
	value = json_value_find(reqc->json, "cfg_obj");
	if (!value) {
		rc = __ldmsd_send_missing_mandatory_attr(reqc,
				"act_obj", "cfg_obj");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(value)) {
		rc = ldmsd_send_type_error(reqc,
				"act_obj:cfg_obj", "a string");
		return rc;
	}
	cfgobj_s = json_value_str(value)->str;

	/* names */
	names = json_value_find(reqc->json, "names");
	if (names && (JSON_LIST_VALUE != json_entity_type(names))) {
		rc = ldmsd_send_type_error(reqc, "act_obj:names", "a list");
		return rc;
	}

	/* regex */
	regex = json_value_find(reqc->json, "regex");
	if (regex && (JSON_LIST_VALUE != json_entity_type(regex))) {
		rc = ldmsd_send_type_error(reqc, "act_obj:regex", "a list");
		return rc;
	}
	if (!names && !regex) {
		rc = ldmsd_send_error(reqc, EINVAL,
			"act_obj: Either 'names' or 'regex' must be given.");
		return rc;
	}

	/* action */
	value = json_value_find(reqc->json, "action");
	if (!value) {
		rc = __ldmsd_send_missing_mandatory_attr(reqc, "act_obj", "action");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(value)) {
		rc = ldmsd_send_type_error(reqc, "act_obj:action", "a string");
		return rc;
	}
	action_s = json_value_str(value)->str;
	len = strlen(action_s);

	if ((0 != strncmp(action_s, "start", len)) &&
			(0 != strncmp(action_s, "stop", len)) &&
			(0 != strncmp(action_s, "delete", len))) {
		rc = ldmsd_send_error(reqc, EINVAL,
				"Unrecognized act_obj:action '%s'", action_s);
		return rc;
	}

	handler = bsearch(&cfgobj_s, act_obj_handler_tbl,
			ARRAY_SIZE(act_obj_handler_tbl),
			sizeof(*handler), handler_entry_comp);
	if (!handler) {
		ldmsd_log(LDMSD_LERROR, "Received an unsupported "
					"configuration object "
					"by action objects'%s'\n",
					cfgobj_s);
		rc = ldmsd_send_error(reqc, ENOTSUP,
				"Received an unsupported "
				"configuration object "
				"by action objects'%s'\n",
				cfgobj_s);
		return rc;
	}

	rc = handler->handler(reqc);
	ldmsd_req_ctxt_free(reqc);
	return rc;

}

int ldmsd_process_json_obj(ldmsd_req_ctxt_t reqc)
{
	json_entity_t type;
	char *type_s;
	int rc;

	type = json_value_find(reqc->json, "type");
	if (!type) {
		ldmsd_log(LDMSD_LERROR, "The 'type' attribute is missing from "
				"message number %d:%" PRIu64 "\n",
				reqc->key.msg_no, reqc->key.conn_id);
		rc = ldmsd_send_error(reqc, EINVAL,
				"The 'type' attribute is missing from "
				"message number %d:%" PRIu64 "\n",
				reqc->key.msg_no, reqc->key.conn_id);
		goto out;
	}
	if (JSON_STRING_VALUE != json_entity_type(type)) {
		ldmsd_log(LDMSD_LERROR, "message number %d:%" PRIu64
				": The 'type' attribute is not a string.\n",
				reqc->key.msg_no, reqc->key.conn_id);
		rc = ldmsd_send_error(reqc, EINVAL, "message number %d:%" PRIu64
				": The 'type' attribute is not a string.",
				reqc->key.msg_no, reqc->key.conn_id);
		goto out;;
	}
	type_s = json_value_str(type)->str;
	if (0 == strncmp("cfg_obj", json_value_str(type)->str, 7)) {
		rc = ldmsd_process_cfg_obj(reqc);
	} else if (0 == strncmp("act_obj", json_value_str(type)->str, 7)) {
		if (!ldmsd_is_initialized()) {
			/*
			 * Do not process any action objects
			 * before LDMSD is initialized.
			 */
			goto out;
		}
		rc = ldmsd_process_act_obj(reqc);
	} else if (0 == strncmp("cmd_obj", json_value_str(type)->str, 7)) {
		rc = ldmsd_process_cmd_obj(reqc);
	} else if (0 == strncmp("err_obj", json_value_str(type)->str, 7)) {
		rc = ldmsd_process_err_obj(reqc);
	} else if (0 == strncmp("info_obj", json_value_str(type)->str, 7)) {
		rc = process_unexpected_info_obj(reqc);
	} else {
		ldmsd_log(LDMSD_LERROR, "Message number %d:%" PRIu64
				"has an unrecognized object type '%s'\n",
				reqc->key.msg_no, reqc->key.conn_id, type_s);
		rc = ldmsd_send_error(reqc, ENOTSUP, "message number %d:%" PRIu64
				" has ba unrecognized object type '%s'.",
				reqc->key.msg_no, reqc->key.conn_id, type_s);
	}
out:
	return rc;
}

int ldmsd_append_info_obj_hdr(ldmsd_req_ctxt_t reqc, const char *info_name)
{
	return ldmsd_append_response_va(reqc, LDMSD_REC_SOM_F,
					"{\"type\":\"info\","
					" \"name\":\"%s\","
					" \"info\":", info_name);
}

int __find_perm(ldmsd_req_ctxt_t reqc, json_entity_t spec, int *perm_value)
{
	int rc;
	json_entity_t perm;
	perm = json_value_find(spec, "perm");
	if (perm) {
		if (JSON_STRING_VALUE != json_entity_type(perm)) {
			rc = ldmsd_send_type_error(reqc, "smplr:perm", "a string");
			return rc;
		}
		*perm_value = strtol(json_value_str(perm)->str, NULL, 0);
	}
	return 0;
}

/*
 * This handler is an example of how to get the received attributes through
 * the 'spec' dictionary from reqc->json. It also an example of how to
 * send a rsp_obj back to the sender by constructing a rsp_obj JSON-formatted string.
 *
 * What the handler does, for example,
 *   the received 'spec' dictionary is
 *
 *   { "attr1": "A",
 *     "attr2": "B"
 *   }
 *
 *   It constructs the rsp_obj JSON-formatted string and sends it back
 *   by calling the convenient function \c ldmsd_append_rsp_obj.
 *
 *   { "rsp":"example","spec":{"attr1":"A","attr2":"B"}}
 *
 */
static int example_handler(ldmsd_req_ctxt_t reqc)
{
	int rc;
	char *str, attr_str[1024];
	size_t cnt;
	json_entity_t recv_attrs, attr;
	json_str_t name, value;
	int attr_num = 0;

	recv_attrs = json_value_find(reqc->json, "spec");

	rc = ldmsd_append_info_obj_hdr(reqc, "example");
	if (rc)
		return rc;
	rc = ldmsd_append_response(reqc, 0, "{", 1);
	if (rc)
		goto out;
	for (attr = json_attr_first(recv_attrs); attr; attr = json_attr_next(attr)) {

		if (attr_num > 0) {
			/* Append the comma that separates two attributes in the dictionary */
			rc = ldmsd_append_response(reqc, 0, ",", 1);
			if (rc)
				goto out;
		}

		name = json_attr_name(attr);
		value = json_value_str(json_attr_value(attr));

		/*
		 * Construct the string
		 * "attr_name":"attr_value"
		 */
		cnt = snprintf(attr_str, 1024, "\"%s\":\"%s\"", name->str, value->str);

		/* Append to the rsp obj */
		rc = ldmsd_append_response(reqc, 0, attr_str, cnt);
		if (rc)
			goto out;
		attr_num++;
	}

	str = "}}"; /* The close parentheses of the 'spec' dict and the rsp_obj dict */
	rc = ldmsd_append_response(reqc, LDMSD_REC_EOM_F, str, strlen(str));
out:
	return rc;
}

//static int prdcr_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	ldmsd_prdcr_t prdcr;
//	char *name, *host, *xprt, *attr_name, *type_s, *port_s, *interval_s;
//	char *auth, *auth_args;
//	name = host = xprt = type_s = port_s = interval_s = auth = auth_args = NULL;
//	enum ldmsd_prdcr_type type = -1;
//	unsigned short port_no = 0;
//	int interval_us = -1;
//	uid_t uid;
//	gid_t gid;
//	int perm;
//	char *perm_s = NULL;
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "type";
//	type_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_TYPE);
//	if (!type_s) {
//		goto einval;
//	} else {
//		type = ldmsd_prdcr_str2type(type_s);
//		if ((int)type < 0) {
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//					"The attribute type '%s' is invalid.",
//					type_s);
//			reqc->errcode = EINVAL;
//			goto send_reply;
//		}
//		if (type == LDMSD_PRDCR_TYPE_LOCAL) {
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//					"Producer with type 'local' is "
//					"not supported.");
//			reqc->errcode = EINVAL;
//			goto send_reply;
//		}
//	}
//
//	attr_name = "xprt";
//	xprt = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_XPRT);
//	if (!xprt)
//		goto einval;
//
//	attr_name = "host";
//	host = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_HOST);
//	if (!host)
//		goto einval;
//
//	attr_name = "port";
//	port_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PORT);
//	if (!port_s) {
//		goto einval;
//	} else {
//		long ptmp = 0;
//		ptmp = strtol(port_s, NULL, 0);
//		if (ptmp < 1 || ptmp > USHRT_MAX) {
//			goto einval;
//		}
//		port_no = (unsigned)ptmp;
//	}
//
//	attr_name = "interval";
//	interval_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	if (!interval_s) {
//		goto einval;
//	} else {
//		 interval_us = strtol(interval_s, NULL, 0);
//	}
//
//	auth = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_AUTH);
//	if (auth)
//		auth_args = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_STRING);
//
//	struct ldmsd_sec_ctxt sctxt;
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	uid = sctxt.crd.uid;
//	gid = sctxt.crd.gid;
//
//	perm = 0770;
//	perm_s = ldmsd_req_attr_str_value_get_by_name(reqc, "perm");
//	if (perm_s)
//		perm = strtol(perm_s, NULL, 0);
//
//	prdcr = ldmsd_prdcr_new_with_auth(name, xprt, host, port_no, type,
//					  interval_us, auth, auth_args,
//					  uid, gid, perm);
//	if (!prdcr) {
//		if (errno == EEXIST)
//			goto eexist;
//		else if (errno == EAFNOSUPPORT)
//			goto eafnosupport;
//		else if (errno == EINVAL)
//			goto auth_args_inval;
//		else
//			goto enomem;
//	}
//
//	goto send_reply;
//auth_args_inval:
//	reqc->errcode = EINVAL;
//	snprintf(reqc->recv_buf, reqc->recv_len,
//			"Invalid auth options");
//	goto send_reply;
//enomem:
//	reqc->errcode = ENOMEM;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Memory allocation failed.");
//	goto send_reply;
//eexist:
//	reqc->errcode = EEXIST;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The prdcr %s already exists.", name);
//	goto send_reply;
//eafnosupport:
//	reqc->errcode = EAFNOSUPPORT;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Error resolving hostname '%s'\n", host);
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required.", attr_name);
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (type_s)
//		free(type_s);
//	if (port_s)
//		free(port_s);
//	if (interval_s)
//		free(interval_s);
//	if (host)
//		free(host);
//	if (xprt)
//		free(xprt);
//	if (perm_s)
//		free(perm_s);
//	return 0;
//}
//
//static int prdcr_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name = NULL, *attr_name;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute '%s' is required by prdcr_del.",
//				attr_name);
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_prdcr_del(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer is in use.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Error: %d %s",
//				reqc->errcode, ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int prdcr_start_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *interval_str;
//	name = interval_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//	int flags = 0;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'name' is required by prdcr_start.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	interval_str = ldmsd_req_attr_str_value_get_by_id(reqc,
//							LDMSD_ATTR_INTERVAL);
//	if (reqc->flags & LDMSD_REQ_DEFER_FLAG)
//		flags = LDMSD_PERM_DSTART;
//	reqc->errcode = ldmsd_prdcr_start(name, interval_str, &sctxt, flags);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer is already running.");
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer specified does not exist.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Error: %d %s",
//				reqc->errcode, ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (interval_str)
//		free(interval_str);
//	return 0;
//}
//
//static int prdcr_stop_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'name' is required by prdcr_stop.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_prdcr_stop(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer is already stopped.");
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The producer specified does not exist.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Error: %d %s",
//				reqc->errcode, ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int prdcr_start_regex_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *prdcr_regex, *interval_str;
//	prdcr_regex = interval_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//	int flags = 0;
//	reqc->errcode = 0;
//
//	prdcr_regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!prdcr_regex) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'regex' is required by prdcr_start_regex.");
//		goto send_reply;
//	}
//
//	interval_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	flags = (reqc->flags & LDMSD_REQ_DEFER_FLAG)?(LDMSD_PERM_DSTART):0;
//	reqc->errcode = ldmsd_prdcr_start_regex(prdcr_regex, interval_str,
//					reqc->recv_buf, reqc->recv_len,
//					&sctxt, flags);
//	/* on error, reqc->line_buf will be filled */
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (prdcr_regex)
//		free(prdcr_regex);
//	if (interval_str)
//		free(interval_str);
//	return 0;
//}
//
//static int prdcr_stop_regex_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *prdcr_regex = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	prdcr_regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!prdcr_regex) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'regex' is required by prdcr_stop_regex.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_prdcr_stop_regex(prdcr_regex,
//				reqc->recv_buf, reqc->recv_len, &sctxt);
//	/* on error, reqc->line_buf will be filled */
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (prdcr_regex)
//		free(prdcr_regex);
//	return 0;
//}
//
//static int prdcr_subscribe_regex_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *prdcr_regex;
//	char *stream_name;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	prdcr_regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!prdcr_regex) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'regex' is required by prdcr_stop_regex.");
//		goto send_reply;
//	}
//
//	stream_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_STREAM);
//	if (!stream_name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'stream' is required by prdcr_subscribe_regex.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_prdcr_subscribe_regex(prdcr_regex,
//						    stream_name,
//						    reqc->recv_buf,
//						    reqc->recv_len, &sctxt);
//	/* on error, reqc->line_buf will be filled */
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (prdcr_regex)
//		free(prdcr_regex);
//	return 0;
//}
//
//int __prdcr_status_json_obj(ldmsd_req_ctxt_t reqc, ldmsd_prdcr_t prdcr, int prdcr_cnt)
//{
//	extern const char *prdcr_state_str(enum ldmsd_prdcr_state state);
//	ldmsd_prdcr_set_t prv_set;
//	int set_count = 0;
//	int rc = 0;
//
//	/* Append the string to line_buf */
//	if (prdcr_cnt) {
//		rc = linebuf_printf(reqc, ",\n");
//		if (rc)
//			return rc;
//	}
//
//	ldmsd_prdcr_lock(prdcr);
//	rc = linebuf_printf(reqc,
//			"{ \"name\":\"%s\","
//			"\"type\":\"%s\","
//			"\"host\":\"%s\","
//			"\"port\":%hu,"
//			"\"transport\":\"%s\","
//			"\"reconnect_us\":\"%ld\","
//			"\"state\":\"%s\","
//			"\"sets\": [",
//			prdcr->obj.name, ldmsd_prdcr_type2str(prdcr->type),
//			prdcr->host_name, prdcr->port_no, prdcr->xprt_name,
//			prdcr->conn_intrvl_us,
//			prdcr_state_str(prdcr->conn_state));
//	if (rc)
//		goto out;
//
//	set_count = 0;
//	for (prv_set = ldmsd_prdcr_set_first(prdcr); prv_set;
//	     prv_set = ldmsd_prdcr_set_next(prv_set)) {
//		if (set_count) {
//			rc = linebuf_printf(reqc, ",\n");
//			if (rc)
//				goto out;
//		}
//
//		rc = linebuf_printf(reqc,
//			"{ \"inst_name\":\"%s\","
//			"\"schema_name\":\"%s\","
//			"\"state\":\"%s\"}",
//			prv_set->inst_name,
//			(prv_set->schema_name ? prv_set->schema_name : ""),
//			ldmsd_prdcr_set_state_str(prv_set->state));
//		if (rc)
//			goto out;
//		set_count++;
//	}
//	rc = linebuf_printf(reqc, "]}");
//out:
//	ldmsd_prdcr_unlock(prdcr);
//	return rc;
//}
//
//static int prdcr_status_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	size_t cnt = 0;
//	struct ldmsd_req_attr_s attr;
//	ldmsd_prdcr_t prdcr = NULL;
//	char *name;
//	int count;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (name) {
//		prdcr = ldmsd_prdcr_find(name);
//		if (!prdcr) {
//			/* Do not report any status */
//			cnt = snprintf(reqc->recv_buf, reqc->recv_len,
//					"prdcr '%s' doesn't exist.", name);
//			reqc->errcode = ENOENT;
//			ldmsd_send_req_response(reqc, reqc->recv_buf);
//			return 0;
//		}
//	}
//
//	/* Construct the json object of the producer(s) */
//	if (prdcr) {
//		rc = __prdcr_status_json_obj(reqc, prdcr, 0);
//		if (rc)
//			goto out;
//	} else {
//		count = 0;
//		ldmsd_cfg_lock(LDMSD_CFGOBJ_PRDCR);
//		for (prdcr = ldmsd_prdcr_first(); prdcr;
//				prdcr = ldmsd_prdcr_next(prdcr)) {
//			rc = __prdcr_status_json_obj(reqc, prdcr, count);
//			if (rc) {
//				ldmsd_cfg_unlock(LDMSD_CFGOBJ_PRDCR);
//				goto out;
//			}
//			count++;
//		}
//		ldmsd_cfg_unlock(LDMSD_CFGOBJ_PRDCR);
//	}
//	cnt = reqc->recv_off + 2; /* +2 for '[' and ']' */
//
//	/* Send the json attribute header */
//	attr.discrim = 1;
//	attr.attr_len = cnt;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		goto out;
//
//	/* Send the json object */
//	rc = ldmsd_append_reply(reqc, "[", 1, 0);
//	if (rc)
//		goto out;
//	if (reqc->recv_off) {
//		rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//		if (rc)
//			goto out;
//	}
//	rc = ldmsd_append_reply(reqc, "]", 1, 0);
//	if (rc) {
//		goto out;
//	}
//
//	/* Send the terminating attribute */
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim,
//			sizeof(uint32_t), LDMSD_REC_EOM_F);
//out:
//	if (name)
//		free(name);
//	if (prdcr)
//		ldmsd_prdcr_put(prdcr);
//	return rc;
//}
//
//size_t __prdcr_set_status(ldmsd_req_ctxt_t reqc, ldmsd_prdcr_set_t prd_set)
//{
//	struct ldms_timestamp ts = { 0, 0 }, dur = { 0, 0 };
//	const char *producer_name = "";
//	char intrvl_hint[32];
//	char offset_hint[32];
//	if (prd_set->set) {
//		ts = ldms_transaction_timestamp_get(prd_set->set);
//		dur = ldms_transaction_duration_get(prd_set->set);
//		producer_name = ldms_set_producer_name_get(prd_set->set);
//	}
//	if (prd_set->updt_hint.intrvl_us) {
//		snprintf(intrvl_hint, sizeof(intrvl_hint), "%ld",
//			 prd_set->updt_hint.intrvl_us);
//	} else {
//		snprintf(intrvl_hint, sizeof(intrvl_hint), "none");
//	}
//	if (prd_set->updt_hint.offset_us != LDMSD_UPDT_HINT_OFFSET_NONE) {
//		snprintf(offset_hint, sizeof(offset_hint), "%ld",
//			 prd_set->updt_hint.offset_us);
//	} else {
//		snprintf(offset_hint, sizeof(offset_hint), "none");
//	}
//	return linebuf_printf(reqc,
//		"{ "
//		"\"inst_name\":\"%s\","
//		"\"schema_name\":\"%s\","
//		"\"state\":\"%s\","
//		"\"origin\":\"%s\","
//		"\"producer\":\"%s\","
//		"\"hint.sec\":\"%s\","
//		"\"hint.usec\":\"%s\","
//		"\"timestamp.sec\":\"%d\","
//		"\"timestamp.usec\":\"%d\","
//		"\"duration.sec\":\"%d\","
//		"\"duration.usec\":\"%d\""
//		"}",
//		prd_set->inst_name, prd_set->schema_name,
//		ldmsd_prdcr_set_state_str(prd_set->state),
//		producer_name,
//		prd_set->prdcr->obj.name,
//		intrvl_hint, offset_hint,
//		ts.sec, ts.usec,
//		dur.sec, dur.usec);
//}
//
///* This function must be called with producer lock held */
//int __prdcr_set_status_handler(ldmsd_req_ctxt_t reqc, ldmsd_prdcr_t prdcr,
//			int *count, const char *setname, const char *schema)
//{
//	int rc = 0;
//	ldmsd_prdcr_set_t prd_set;
//
//	if (setname) {
//		prd_set = ldmsd_prdcr_set_find(prdcr, setname);
//		if (!prd_set)
//			return 0;
//		if (schema && (0 != strcmp(prd_set->schema_name, schema)))
//			return 0;
//		if (*count) {
//			rc = linebuf_printf(reqc, ",\n");
//			if (rc)
//				return rc;
//		}
//		rc = __prdcr_set_status(reqc, prd_set);
//		if (rc)
//			return rc;
//		(*count)++;
//	} else {
//		for (prd_set = ldmsd_prdcr_set_first(prdcr); prd_set;
//			prd_set = ldmsd_prdcr_set_next(prd_set)) {
//			if (schema && (0 != strcmp(prd_set->schema_name, schema)))
//				continue;
//
//			if (*count) {
//				rc = linebuf_printf(reqc, ",\n");
//				if (rc)
//					return rc;
//			}
//			rc = __prdcr_set_status(reqc, prd_set);
//			if (rc)
//				return rc;
//			(*count)++;
//		}
//	}
//	return rc;
//}
//
//int __prdcr_set_status_json_obj(ldmsd_req_ctxt_t reqc)
//{
//	char *prdcr_name, *setname, *schema;
//	prdcr_name = setname = schema = NULL;
//	ldmsd_prdcr_t prdcr = NULL;
//	int rc, count = 0;
//	reqc->errcode = 0;
//
//	prdcr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PRODUCER);
//	setname = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INSTANCE);
//	schema = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_SCHEMA);
//
//	rc = linebuf_printf(reqc, "[");
//	if (rc)
//		return rc;
//	if (prdcr_name) {
//		prdcr = ldmsd_prdcr_find(prdcr_name);
//		if (!prdcr)
//			goto out;
//	}
//
//	if (prdcr) {
//		ldmsd_prdcr_lock(prdcr);
//		rc = __prdcr_set_status_handler(reqc, prdcr, &count,
//						setname, schema);
//		ldmsd_prdcr_unlock(prdcr);
//	} else {
//		ldmsd_cfg_lock(LDMSD_CFGOBJ_PRDCR);
//		for (prdcr = ldmsd_prdcr_first(); prdcr;
//				prdcr = ldmsd_prdcr_next(prdcr)) {
//			ldmsd_prdcr_lock(prdcr);
//			rc = __prdcr_set_status_handler(reqc, prdcr, &count,
//							setname, schema);
//			ldmsd_prdcr_unlock(prdcr);
//			if (rc) {
//				ldmsd_cfg_unlock(LDMSD_CFGOBJ_PRDCR);
//				goto out;
//			}
//		}
//		ldmsd_cfg_unlock(LDMSD_CFGOBJ_PRDCR);
//	}
//
//out:
//	rc = linebuf_printf(reqc, "]");
//	if (prdcr_name)
//		free(prdcr_name);
//	if (setname)
//		free(setname);
//	if (schema)
//		free(schema);
//	if (prdcr) /* ref from find(), first(), or next() */
//		ldmsd_prdcr_put(prdcr);
//	return rc;
//}
//
//static int prdcr_set_status_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	struct ldmsd_req_attr_s attr;
//
//	rc = __prdcr_set_status_json_obj(reqc);
//	if (rc)
//		return rc;
//	attr.discrim = 1;
//	attr.attr_len = reqc->recv_off;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr,
//				sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		return rc;
//
//	rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//	if (rc)
//		return rc;
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim,
//			sizeof(uint32_t), LDMSD_REC_EOM_F);
//	return rc;
//}
//
//static int strgp_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *attr_name, *name, *container, *schema;
//	name = container = schema = NULL;
//	uid_t uid;
//	gid_t gid;
//	int perm;
//	char *perm_s = NULL;
//	ldmsd_plugin_inst_t inst = NULL;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "container";
//	container = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_CONTAINER);
//	if (!container)
//		goto einval;
//
//	inst = ldmsd_plugin_inst_find(container);
//	if (!inst)
//		goto enoent;
//
//	attr_name = "schema";
//	schema = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_SCHEMA);
//	if (!schema)
//		goto einval;
//
//	struct ldmsd_sec_ctxt sctxt;
//	ldmsd_sec_ctxt_get(&sctxt);
//	uid = sctxt.crd.uid;
//	gid = sctxt.crd.gid;
//
//	perm = 0770;
//	perm_s = ldmsd_req_attr_str_value_get_by_name(reqc, "perm");
//	if (perm_s)
//		perm = strtol(perm_s, NULL, 0);
//
//	ldmsd_strgp_t strgp = ldmsd_strgp_new_with_auth(name, uid, gid, perm);
//	if (!strgp) {
//		if (errno == EEXIST)
//			goto eexist;
//		else
//			goto enomem;
//	}
//	ldmsd_plugin_inst_get(inst); /* for attaching inst to strgp,
//				      * this is put down on strgp delete. */
//	strgp->inst = inst;
//	strgp->schema = strdup(schema);
//	if (!strgp->schema)
//		goto enomem_1;
//
//	goto send_reply;
//
//enomem_1:
//	ldmsd_strgp_del(name, &sctxt);
//enomem:
//	reqc->errcode = ENOMEM;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Memory allocation failed.");
//	goto send_reply;
//eexist:
//	reqc->errcode = EEXIST;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The prdcr %s already exists.", name);
//	goto send_reply;
//enoent:
//	reqc->errcode = ENOENT;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Store instance (container '%s') not found.",
//				container);
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//		       "The attribute '%s' is required by strgp_add.",
//		       attr_name);
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (container)
//		free(container);
//	if (schema)
//		free(schema);
//	if (perm_s)
//		free(perm_s);
//	if (inst)
//		ldmsd_plugin_inst_put(inst); /* put down ref from `find` */
//	return 0;
//}
//
//static int strgp_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode= EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'name' is required"
//				"by strgp_del.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_strgp_del(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy is in use.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//

static int __smplr_handler(ldmsd_req_ctxt_t reqc, json_entity_t d,
				unsigned long int_v, unsigned long offset_v,
				int perm_value, uid_t uid, gid_t gid)
{
	int rc;
	json_entity_t name, pi_inst;
	char *name_s, *inst_s;
	char obj_name[128];

	name = json_value_find(d, "name");
	if (!name) {
		rc = ldmsd_send_missing_attr_err(reqc, "cfg_obj:smplr", "name");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(name)) {
		rc = ldmsd_send_type_error(reqc, "smplr:name", "a string");
		return rc;
	}
	name_s = json_value_str(name)->str;
	snprintf(obj_name, 128, "smplr(%s)", name_s);
	pi_inst = json_value_find(d, "plugin_instance");
	if (!pi_inst) {
		rc = ldmsd_send_missing_attr_err(reqc,
				obj_name, "plugin_instance");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(pi_inst)) {
		rc = ldmsd_send_type_error(reqc, obj_name, "a string");
		return rc;
	}
	inst_s = json_value_str(pi_inst)->str;

	ldmsd_plugin_inst_t pi = ldmsd_plugin_inst_find(inst_s);
	if (!pi) {
		rc = ldmsd_send_error(reqc, ENOENT,
				"%s: Plugin instance `%s` not found\n",
				obj_name, inst_s);
		return rc;
	}
	if (!LDMSD_INST_IS_SAMPLER(pi)) {
		rc = ldmsd_send_error(reqc, EINVAL,
				"%s: The plugin instance `%s` is not a sampler.\n",
				obj_name, inst_s);
		return rc;
	}

	ldmsd_smplr_t smplr = ldmsd_smplr_new_with_auth(name_s, pi,
							int_v, offset_v,
							uid, gid, perm_value);
	if (!smplr) {
		if (errno == EEXIST) {
			rc = ldmsd_send_error(reqc, EEXIST,
					"%s: The smplr %s already exists.",
					obj_name, name_s);
			return rc;
		} else {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
			return ENOMEM;
		}
	}
	return 0;
}

static int smplr_handler(ldmsd_req_ctxt_t reqc)
{
	unsigned long int_v, offset_v;
	uid_t uid;
	gid_t gid;
	int perm_value;
	int rc;
	json_entity_t spec, instances, interval, offset, item;
	struct ldmsd_sec_ctxt sctxt;

	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
	uid = sctxt.crd.uid;
	gid = sctxt.crd.gid;

	spec = json_value_find(reqc->json, "spec");

	/* sample interval */
	interval = json_value_find(spec, "interval");
	if (!interval) {
		rc = ldmsd_send_missing_attr_err(reqc, "cfg_obj:smplr", "interval");
		return rc;
	}
	if (JSON_STRING_VALUE == json_entity_type(interval)) {
		int_v = ldmsd_time_str2us(json_value_str(interval)->str);
	} else if (JSON_INT_VALUE == json_entity_type(interval)) {
		int_v = json_value_int(interval);
	} else {
		rc = ldmsd_send_type_error(reqc, "smplr:interval",
				"a string or an integer");
		return rc;
	}

	offset = json_value_find(spec, "offset");
	if (offset) {
		if (JSON_STRING_VALUE == json_entity_type(offset)) {
			offset_v = ldmsd_time_str2us(json_value_str(offset)->str);
		} else if (JSON_INT_VALUE == json_entity_type(offset)) {
			offset_v = json_value_int(offset);
		} else {
			rc = ldmsd_send_type_error(reqc, "smplr:offset",
					"a string or an integer");
			return rc;
		}
	} else {
		offset_v = LDMSD_UPDT_HINT_OFFSET_NONE;
	}

	/* Permission */
	perm_value = 0770;
	rc = __find_perm(reqc, spec, &perm_value);
	if (rc)
		return rc;

	instances = json_value_find(reqc->json, "instances");
	if (!instances) {
		rc = __smplr_handler(reqc, spec, int_v, offset_v,
						perm_value, uid, gid);
		if (rc)
			return rc;
	} else {
		for (item = json_item_first(instances); item;
				item = json_item_next(item)) {
			if (JSON_DICT_VALUE != json_entity_type(item)) {
				rc = ldmsd_send_type_error(reqc,
							"smplr:instances[]",
							"a dictionary");
				return rc;
			}
			rc = __smplr_handler(reqc, item, int_v, offset_v,
						perm_value, uid, gid);
			if (rc)
				return rc;
		}
	}

	rc = ldmsd_send_error(reqc, 0, "");
	return rc;
}
//
//static int smplr_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode= EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'name' is required"
//				"by strgp_del.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_smplr_del(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The sampler policy specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The sampler policy is in use.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int strgp_prdcr_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *regex_str, *attr_name;
//	name = regex_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "regex";
//	regex_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!regex_str)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_strgp_prdcr_add(name, regex_str,
//				reqc->recv_buf, reqc->recv_len, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy specified "
//				"does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be made "
//				"while the storage policy is running.");
//		break;
//	case ENOMEM:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//					"Out of memory");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//					"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"strgp_prdcr_add");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (regex_str)
//		free(regex_str);
//	return 0;
//}
//
//static int strgp_prdcr_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *regex_str, *attr_name;
//	name = regex_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "regex";
//	regex_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!regex_str)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_strgp_prdcr_del(name, regex_str, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy specified "
//				"does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Configuration changes cannot be made "
//			"while the storage policy is running.");
//		break;
//	case EEXIST:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The specified regex does not match "
//				"any condition.");
//		reqc->errcode = ENOENT;
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"strgp_prdcr_del");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (regex_str)
//		free(regex_str);
//	return 0;
//}
//
//static int strgp_metric_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *metric_name, *attr_name;
//	name = metric_name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "metric";
//	metric_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_METRIC);
//	if (!metric_name)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_strgp_metric_add(name, metric_name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy specified "
//				"does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be made "
//				"while the storage policy is running.");
//		break;
//	case EEXIST:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The specified metric is already present.");
//		break;
//	case ENOMEM:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Memory allocation failure.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"strgp_metric_add");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (metric_name)
//		free(metric_name);
//	return 0;
//}
//
//static int strgp_metric_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *metric_name, *attr_name;
//	name = metric_name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	attr_name = "metric";
//	metric_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_METRIC);
//	if (!metric_name)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_strgp_metric_del(name, metric_name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be made "
//				"while the storage policy is running.");
//		break;
//	case EEXIST:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The specified metric was not found.");
//		reqc->errcode = ENOENT;
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"strgp_metric_del");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (metric_name)
//		free(metric_name);
//	return 0;
//}
//
//static int strgp_start_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *attr_name;
//	name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//	int flags = 0;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"%dThe attribute '%s' is required by %s.",
//				EINVAL, attr_name, "strgp_start");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	if (reqc->flags & LDMSD_REQ_DEFER_FLAG)
//		flags = LDMSD_PERM_DSTART;
//	reqc->errcode = ldmsd_strgp_start(name, &sctxt, flags);
//	switch (reqc->errcode) {
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The storage policy does not exist.");
//		goto send_reply;
//	case EPERM:
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Permission denied.");
//		goto send_reply;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The storage policy is already running.");
//		goto send_reply;
//	case 0:
//		break;
//	default:
//		break;
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int strgp_stop_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *attr_name;
//	name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute '%s' is required by %s.",
//				attr_name, "strgp_stop");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_strgp_stop(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The storage policy is not running.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//int __strgp_status_json_obj(ldmsd_req_ctxt_t reqc, ldmsd_strgp_t strgp,
//							int strgp_cnt)
//{
//	int rc;
//	int match_count, metric_count;
//	ldmsd_name_match_t match;
//	ldmsd_strgp_metric_t metric;
//
//	if (strgp_cnt) {
//		rc = linebuf_printf(reqc, ",\n");
//		if (rc)
//			goto out;
//	}
//
//	ldmsd_strgp_lock(strgp);
//	rc = linebuf_printf(reqc,
//		       "{\"name\":\"%s\","
//		       "\"container\":\"%s\","
//		       "\"plugin\":\"%s\","
//		       "\"schema\":\"%s\","
//		       "\"state\":\"%s\","
//		       "\"producers\":[",
//		       strgp->obj.name,
//		       strgp->inst->inst_name,
//		       strgp->inst->plugin_name,
//		       strgp->schema,
//		       ldmsd_strgp_state_str(strgp->state));
//	if (rc)
//		goto out;
//
//	match_count = 0;
//	for (match = ldmsd_strgp_prdcr_first(strgp); match;
//	     match = ldmsd_strgp_prdcr_next(match)) {
//		if (match_count) {
//			rc = linebuf_printf(reqc, ",");
//			if (rc)
//				goto out;
//		}
//		match_count++;
//		rc = linebuf_printf(reqc, "\"%s\"", match->regex_str);
//		if (rc)
//			goto out;
//	}
//	rc = linebuf_printf(reqc, "],\"metrics\":[");
//	if (rc)
//		goto out;
//
//	metric_count = 0;
//	for (metric = ldmsd_strgp_metric_first(strgp); metric;
//	     metric = ldmsd_strgp_metric_next(metric)) {
//		if (metric_count) {
//			rc = linebuf_printf(reqc, ",");
//			if (rc)
//				goto out;
//		}
//		metric_count++;
//		rc = linebuf_printf(reqc, "\"%s\"", metric->name);
//		if (rc)
//			goto out;
//	}
//	rc = linebuf_printf(reqc, "]}");
//out:
//	ldmsd_strgp_unlock(strgp);
//	return rc;
//}
//
//static int strgp_status_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	size_t cnt = 0;
//	struct ldmsd_req_attr_s attr;
//	char *name;
//	ldmsd_strgp_t strgp = NULL;
//	int strgp_cnt;
//
//	reqc->errcode = 0;
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (name) {
//		strgp = ldmsd_strgp_find(name);
//		if (!strgp) {
//			/* Not report any status */
//			cnt = snprintf(reqc->recv_buf, reqc->recv_len,
//				"strgp '%s' doesn't exist.", name);
//			reqc->errcode = ENOENT;
//			ldmsd_send_req_response(reqc, reqc->recv_buf);
//			return 0;
//		}
//	}
//
//	/* Construct the json object of the strgp(s) */
//	if (strgp) {
//		rc = __strgp_status_json_obj(reqc, strgp, 0);
//	} else {
//		strgp_cnt = 0;
//		ldmsd_cfg_lock(LDMSD_CFGOBJ_STRGP);
//		for (strgp = ldmsd_strgp_first(); strgp;
//			strgp = ldmsd_strgp_next(strgp)) {
//			rc = __strgp_status_json_obj(reqc, strgp, strgp_cnt);
//			if (rc) {
//				ldmsd_cfg_unlock(LDMSD_CFGOBJ_STRGP);
//				goto out;
//			}
//			strgp_cnt++;
//		}
//		ldmsd_cfg_unlock(LDMSD_CFGOBJ_STRGP);
//	}
//	cnt = reqc->recv_off + 2; /* +2 for '[' and ']' */
//
//	/* Send the json attribute header */
//	attr.discrim = 1;
//	attr.attr_len = cnt;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		goto out;
//
//	/* Send the json object */
//	rc = ldmsd_append_reply(reqc, "[", 1, 0);
//	if (rc)
//		goto out;
//	if (reqc->recv_off) {
//		rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//		if (rc)
//			goto out;
//	}
//	rc = ldmsd_append_reply(reqc, "]", 1, 0);
//	if (rc)
//		goto out;
//
//	/* Send the terminating attribute */
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim, sizeof(uint32_t),
//								LDMSD_REC_EOM_F);
//out:
//	if (name)
//		free(name);
//	if (strgp)
//		ldmsd_strgp_put(strgp);
//	return rc;
//}
//
//static int updtr_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *offset_str, *interval_str, *push, *auto_interval;
//	name = offset_str = interval_str = push = auto_interval = NULL;
//	uid_t uid;
//	gid_t gid;
//	int perm;
//	char *perm_s = NULL;
//	int push_flags, is_auto_task;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "The attribute 'name' is required.");
//		goto send_reply;
//	}
//	if (0 == strncmp(LDMSD_FAILOVER_NAME_PREFIX, name,
//			 sizeof(LDMSD_FAILOVER_NAME_PREFIX)-1)) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "%s is an invalid updtr name",
//			       name);
//		goto send_reply;
//	}
//
//	interval_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	if (!interval_str) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "The 'interval' attribute is required.");
//		goto send_reply;
//	}
//
//	offset_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_OFFSET);
//	push = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PUSH);
//	auto_interval = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_AUTO_INTERVAL);
//
//	struct ldmsd_sec_ctxt sctxt;
//	ldmsd_sec_ctxt_get(&sctxt);
//	uid = sctxt.crd.uid;
//	gid = sctxt.crd.gid;
//
//	perm = 0770;
//	perm_s = ldmsd_req_attr_str_value_get_by_name(reqc, "perm");
//	if (perm_s)
//		perm = strtoul(perm_s, NULL, 0);
//
//	if (auto_interval) {
//		if (0 == strcasecmp(auto_interval, "true")) {
//			if (push) {
//				reqc->errcode = EINVAL;
//				Snprintf(&reqc->recv_buf, &reqc->recv_len,
//						"auto_interval and push are "
//						"incompatible options");
//				goto send_reply;
//			}
//			is_auto_task = 1;
//		} else if (0 == strcasecmp(auto_interval, "false")) {
//			is_auto_task = 0;
//		} else {
//			reqc->errcode = EINVAL;
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "The auto_interval option requires "
//				       "either 'true', or 'false'\n");
//			goto send_reply;
//		}
//	} else {
//		is_auto_task = 0;
//	}
//	push_flags = 0;
//	if (push) {
//		if (0 == strcasecmp(push, "onchange")) {
//			push_flags = LDMSD_UPDTR_F_PUSH | LDMSD_UPDTR_F_PUSH_CHANGE;
//		} else if (0 == strcasecmp(push, "true") || 0 == strcasecmp(push, "yes")) {
//			push_flags = LDMSD_UPDTR_F_PUSH;
//		} else {
//			reqc->errcode = EINVAL;
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "The valud push options are \"onchange\", \"true\" "
//				       "or \"yes\"\n");
//			goto send_reply;
//		}
//		is_auto_task = 0;
//	}
//	ldmsd_updtr_t updtr = ldmsd_updtr_new_with_auth(name, interval_str,
//							offset_str ? offset_str : "0",
//							push_flags,
//							is_auto_task,
//							uid, gid, perm);
//	if (!updtr) {
//		reqc->errcode = errno;
//		if (errno == EEXIST) {
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "The updtr %s already exists.", name);
//		} else if (errno == ENOMEM) {
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "Out of memory");
//		} else {
//			if (!reqc->errcode)
//				reqc->errcode = EINVAL;
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "The updtr could not be created.");
//		}
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (interval_str)
//		free(interval_str);
//	if (auto_interval)
//		free(auto_interval);
//	if (offset_str)
//		free(offset_str);
//	if (push)
//		free(push);
//	if (perm_s)
//		free(perm_s);
//	return 0;
//}
//
//static int updtr_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_del(name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater is in use.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute 'name' is required by updtr_del.");
//	goto send_reply;
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int updtr_prdcr_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name, *prdcr_regex, *attr_name;
//	updtr_name = prdcr_regex = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name)
//		goto einval;
//
//	if (0 == strncmp(LDMSD_FAILOVER_NAME_PREFIX, updtr_name,
//			 sizeof(LDMSD_FAILOVER_NAME_PREFIX) - 1)) {
//		goto ename;
//	}
//
//	attr_name = "regex";
//	prdcr_regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!prdcr_regex)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_prdcr_add(updtr_name, prdcr_regex,
//				reqc->recv_buf, reqc->recv_len, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be "
//				"made while the updater is running.");
//		break;
//	case ENOMEM:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Memory allocation failure.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//
//ename:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Bad prdcr name");
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"updtr_prdcr_add");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	if (prdcr_regex)
//		free(prdcr_regex);
//	return 0;
//}
//
//static int updtr_prdcr_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name, *prdcr_regex, *attr_name;
//	updtr_name = prdcr_regex = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name)
//		goto einval;
//
//	attr_name = "regex";
//	prdcr_regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!prdcr_regex)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_prdcr_del(updtr_name, prdcr_regex,
//			reqc->recv_buf, reqc->recv_len, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	case ENOMEM:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be "
//				"made while the updater is running,");
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"updtr_prdcr_del");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	if (prdcr_regex)
//		free(prdcr_regex);
//	return 0;
//}
//
//static int updtr_match_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name, *regex_str, *match_str, *attr_name;
//	updtr_name = regex_str = match_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name)
//		goto einval;
//	attr_name = "regex";
//	regex_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!regex_str)
//		goto einval;
//
//	match_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_MATCH);
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_match_add(updtr_name, regex_str, match_str,
//			reqc->recv_buf, reqc->recv_len, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be made "
//				"while the updater is running.");
//		break;
//	case ENOMEM:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Out of memory.");
//		break;
//	case EINVAL:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The value '%s' for match= is invalid.",
//				match_str);
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"updtr_match_add");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	if (regex_str)
//		free(regex_str);
//	if (match_str)
//		free(match_str);
//	return 0;
//}
//
//static int updtr_match_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name, *regex_str, *match_str, *attr_name;
//	updtr_name = regex_str = match_str = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "name";
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name)
//		goto einval;
//	attr_name = "regex";
//	regex_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!regex_str)
//		goto einval;
//
//	match_str  = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_MATCH);
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_match_del(updtr_name, regex_str, match_str,
//					      &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Configuration changes cannot be made "
//				"while the updater is running.");
//		break;
//	case -ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The specified regex does not match any condition.");
//		reqc->errcode = -reqc->errcode;
//		break;
//	case EINVAL:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Unrecognized match type '%s'", match_str);
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by %s.", attr_name,
//			"updtr_match_del");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	if (regex_str)
//		free(regex_str);
//	if (match_str)
//		free(match_str);
//	return 0;
//}
//
//static int updtr_start_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name, *interval_str, *offset_str, *auto_interval;
//	updtr_name = interval_str = offset_str = auto_interval = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//	int flags;
//	reqc->errcode = 0;
//
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater name must be specified.");
//		goto send_reply;
//	}
//	interval_str = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	offset_str  = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_OFFSET);
//	auto_interval = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_AUTO_INTERVAL);
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	flags = (reqc->flags & LDMSD_REQ_DEFER_FLAG)?(LDMSD_PERM_DSTART):0;
//	reqc->errcode = ldmsd_updtr_start(updtr_name, interval_str, offset_str,
//					  auto_interval, &sctxt, flags);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater is already running.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	if (interval_str)
//		free(interval_str);
//	if (offset_str)
//		free(offset_str);
//	return 0;
//}
//
//static int updtr_stop_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *updtr_name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	updtr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!updtr_name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater name must be specified.");
//		goto send_reply;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_updtr_stop(updtr_name, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater specified does not exist.");
//		break;
//	case EBUSY:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The updater is already stopped.");
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (updtr_name)
//		free(updtr_name);
//	return 0;
//}
//
//static const char *update_mode(int push_flags)
//{
//	if (!push_flags)
//		return "Pull";
//	if (push_flags & LDMSD_UPDTR_F_PUSH_CHANGE)
//		return "Push on Change";
//	return "Push on Request";
//}
//
//int __updtr_status_json_obj(ldmsd_req_ctxt_t reqc, ldmsd_updtr_t updtr,
//							int updtr_cnt)
//{
//	int rc;
//	ldmsd_prdcr_ref_t ref;
//	ldmsd_prdcr_t prdcr;
//	int prdcr_count;
//	const char *prdcr_state_str(enum ldmsd_prdcr_state state);
//
//	if (updtr_cnt) {
//		rc = linebuf_printf(reqc, ",\n");
//		if (rc)
//			return rc;
//	}
//
//	ldmsd_updtr_lock(updtr);
//	rc = linebuf_printf(reqc,
//		"{\"name\":\"%s\","
//		"\"interval\":\"%ld\","
//		"\"offset\":\"%ld\","
//	        "\"offset_incr\":\"%ld\","
//		"\"auto\":\"%s\","
//		"\"mode\":\"%s\","
//		"\"state\":\"%s\","
//		"\"producers\":[",
//		updtr->obj.name,
//		updtr->sched.intrvl_us,
//		updtr->sched.offset_us,
//		updtr->sched.offset_skew,
//		updtr->is_auto_task ? "true" : "false",
//		update_mode(updtr->push_flags),
//		ldmsd_updtr_state_str(updtr->state));
//	if (rc)
//		goto out;
//
//	prdcr_count = 0;
//	for (ref = ldmsd_updtr_prdcr_first(updtr); ref;
//	     ref = ldmsd_updtr_prdcr_next(ref)) {
//		if (prdcr_count) {
//			rc = linebuf_printf(reqc, ",\n");
//			if (rc)
//				goto out;
//		}
//		prdcr_count++;
//		prdcr = ref->prdcr;
//		rc = linebuf_printf(reqc,
//			       "{\"name\":\"%s\","
//			       "\"host\":\"%s\","
//			       "\"port\":%hu,"
//			       "\"transport\":\"%s\","
//			       "\"state\":\"%s\"}",
//			       prdcr->obj.name,
//			       prdcr->host_name,
//			       prdcr->port_no,
//			       prdcr->xprt_name,
//			       prdcr_state_str(prdcr->conn_state));
//		if (rc)
//			goto out;
//	}
//	rc = linebuf_printf(reqc, "]}");
//out:
//	ldmsd_updtr_unlock(updtr);
//	return rc;
//}
//
//static int updtr_status_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	size_t cnt = 0;
//	struct ldmsd_req_attr_s attr;
//	char *name;
//	int updtr_cnt;
//	ldmsd_updtr_t updtr = NULL;
//
//	reqc->errcode = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (name) {
//		updtr = ldmsd_updtr_find(name);
//		if (!updtr) {
//			/* Don't report any status */
//			cnt = snprintf(reqc->recv_buf, reqc->recv_len,
//				"updtr '%s' doesn't exist.", name);
//			reqc->errcode = ENOENT;
//			ldmsd_send_req_response(reqc, reqc->recv_buf);
//			return 0;
//		}
//	}
//
//	/* Construct the json object of the updater(s) */
//	if (updtr) {
//		rc = __updtr_status_json_obj(reqc, updtr, 0);
//		if (rc)
//			goto out;
//	} else {
//		updtr_cnt = 0;
//		ldmsd_cfg_lock(LDMSD_CFGOBJ_UPDTR);
//		for (updtr = ldmsd_updtr_first(); updtr;
//				updtr = ldmsd_updtr_next(updtr)) {
//			rc = __updtr_status_json_obj(reqc, updtr, updtr_cnt);
//			if (rc) {
//				ldmsd_cfg_unlock(LDMSD_CFGOBJ_UPDTR);
//				goto out;
//			}
//			updtr_cnt++;
//		}
//		ldmsd_cfg_unlock(LDMSD_CFGOBJ_UPDTR);
//	}
//	cnt = reqc->recv_off + 2; /* +2 for '[' and ']' */
//
//	/* Send the json attribute header */
//	attr.discrim = 1;
//	attr.attr_len = cnt;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		goto out;
//
//	/* send the json object */
//	rc = ldmsd_append_reply(reqc, "[", 1, 0);
//	if (rc)
//		goto out;
//	if (reqc->recv_off) {
//		rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//		if (rc)
//			goto out;
//	}
//	rc = ldmsd_append_reply(reqc, "]", 1, 0);
//	if (rc)
//		goto out;
//
//	/* Send the terminating attribute */
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim, sizeof(uint32_t),
//								LDMSD_REC_EOM_F);
//out:
//	if (name)
//		free(name);
//	if (updtr)
//		ldmsd_updtr_put(updtr);
//	return rc;
//}
//
//static int setgroup_add_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	char *name = NULL;
//	char *producer = NULL;
//	char *interval = NULL; /* for update hint */
//	char *offset = NULL; /* for update hint */
//	char *perm_s = NULL;
//	ldmsd_setgrp_t grp = NULL;
//	long interval_us, offset_us = LDMSD_UPDT_HINT_OFFSET_NONE;
//	struct ldmsd_sec_ctxt sctxt;
//	mode_t perm;
//	int flags = 0;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		linebuf_printf(reqc, "missing `name` attribute");
//		rc = EINVAL;
//		goto out;
//	}
//
//	producer = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PRODUCER);
//	interval = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	offset = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_OFFSET);
//	if (interval) {
//		/*
//		 * The interval and offset values are used for
//		 * the auto-interval update in the next aggregation.
//		 */
//		interval_us = strtol(interval, NULL, 0);
//		if (offset) {
//			offset_us = strtol(offset, NULL, 0);
//		}
//	} else {
//		interval_us = 0;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	perm = 0777;
//	perm_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PERM);
//	if (perm_s)
//		perm = strtol(perm_s, NULL, 0);
//
//	if (reqc->flags & LDMSD_REQ_DEFER_FLAG)
//		flags |= LDMSD_PERM_DSTART;
//
//	grp = ldmsd_setgrp_new_with_auth(name, producer, interval_us, offset_us,
//					sctxt.crd.uid, sctxt.crd.gid, perm, flags);
//	if (!grp) {
//		rc = errno;
//		if (errno == EEXIST) {
//			linebuf_printf(reqc,
//				"A set or a group existed with the given name.");
//		} else {
//			linebuf_printf(reqc, "Group creation error: %d", rc);
//		}
//	}
//
//out:
//	reqc->errcode = rc;
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (producer)
//		free(producer);
//	if (interval)
//		free(interval);
//	if (offset)
//		free(offset);
//	if (perm_s)
//		free(perm_s);
//	return 0;
//}
//
//static int setgroup_mod_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	char *name = NULL;
//	char *interval = NULL; /* for update hint */
//	char *offset = NULL; /* for update hint */
//	long interval_us = 0, offset_us = LDMSD_UPDT_HINT_OFFSET_NONE;
//	ldmsd_setgrp_t grp = NULL;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		linebuf_printf(reqc, "missing `name` attribute");
//		rc = EINVAL;
//		goto send_reply;
//	}
//
//	interval = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	offset = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_OFFSET);
//	if (interval) {
//		interval_us = strtol(interval, NULL, 0);
//		if (offset) {
//			offset_us = strtol(offset, NULL, 0);
//		}
//	}
//
//	grp = ldmsd_setgrp_find(name);
//	if (!grp) {
//		rc = ENOENT;
//		linebuf_printf(reqc, "Group '%s' not found.", name);
//		goto send_reply;
//	}
//	ldmsd_setgrp_lock(grp);
//	rc = ldmsd_set_update_hint_set(grp->set, interval_us, offset_us);
//	if (rc)
//		linebuf_printf(reqc, "Update hint update error: %d", rc);
//	/* rc is 0 */
//	ldmsd_setgrp_unlock(grp);
//send_reply:
//	reqc->errcode = rc;
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (interval)
//		free(interval);
//	if (offset)
//		free(offset);
//	if (grp)
//		ldmsd_setgrp_put(grp); /* `fine` reference */
//	return rc;
//}
//
//static int setgroup_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		linebuf_printf(reqc, "missing `name` attribute");
//		rc = EINVAL;
//		goto out;
//	}
//	rc = ldmsd_setgrp_del(name, &sctxt);
//	if (rc == ENOENT) {
//		linebuf_printf(reqc, "Setgroup '%s' not found.", name);
//	} else if (rc == EACCES) {
//		linebuf_printf(reqc, "Permission denied");
//	} else {
//		linebuf_printf(reqc, "Failed to delete setgroup '%s'", name);
//	}
//
//out:
//	reqc->errcode = rc;
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	return 0;
//}
//
//static int setgroup_ins_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	const char *delim = ",";
//	char *name = NULL;
//	char *instance = NULL;
//	char *sname;
//	char *p;
//	char *attr_name;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		attr_name = "name";
//		goto einval;
//	}
//	instance = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INSTANCE);
//	if (!instance) {
//		attr_name = "instance";
//		goto einval;
//	}
//
//	sname = strtok_r(instance, delim, &p);
//	while (sname) {
//		rc = ldmsd_setgrp_ins(name, sname);
//		if (rc) {
//			if (rc == ENOENT) {
//				linebuf_printf(reqc,
//					"Either setgroup '%s' or member '%s' not exist",
//					name, sname);
//			} else {
//				linebuf_printf(reqc, "Error %d: Failed to add "
//						"member '%s' to setgroup '%s'",
//						rc, sname, name);
//			}
//			goto send_reply;
//		}
//		sname = strtok_r(NULL, delim, &p);
//	}
//	/* rc is 0 */
//	goto send_reply;
//einval:
//	linebuf_printf(reqc, "The attribute '%s' is missing.", attr_name);
//	rc = EINVAL;
//send_reply:
//	reqc->errcode = rc;
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (instance)
//		free(instance);
//	return rc;
//}
//
//int __ldmsd_setgrp_rm(ldmsd_setgrp_t grp, const char *instance);
//
//static int setgroup_rm_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	const char *delim = ",";
//	char *name = NULL;
//	char *instance = NULL;
//	char *sname;
//	char *p;
//	char *attr_name;
//	ldmsd_setgrp_t grp = NULL;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		attr_name = "name";
//		goto einval;
//	}
//
//	instance = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INSTANCE);
//	if (!instance) {
//		attr_name = "instance";
//		goto einval;
//	}
//
//	grp = ldmsd_setgrp_find(name);
//	if (!grp) {
//		rc = ENOENT;;
//		linebuf_printf(reqc, "Setgroup '%s' not found.", name);
//		goto send_reply;
//	}
//
//	ldmsd_setgrp_lock(grp);
//	sname = strtok_r(instance, delim, &p);
//	while (sname) {
//		rc = __ldmsd_setgrp_rm(grp, sname);
//		if (rc) {
//			if (rc == ENOENT) {
//				linebuf_printf(reqc,
//					"Either setgroup '%s' or member '%s' not exist",
//					name, sname);
//			} else {
//				linebuf_printf(reqc, "Error %d: Failed to remove "
//						"member '%s' from setgroup '%s'",
//						rc, sname, name);
//			}
//			ldmsd_setgrp_unlock(grp);
//			goto send_reply;
//		}
//		sname = strtok_r(NULL, delim, &p);
//	}
//	ldmsd_setgrp_unlock(grp);
//	/* rc is 0 */
//	goto send_reply;
//einval:
//	linebuf_printf(reqc, "The attribute '%s' is missing.", attr_name);
//	rc = EINVAL;
//send_reply:
//	reqc->errcode = rc;
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (instance)
//		free(instance);
//	if (grp)
//		ldmsd_setgrp_put(grp); /* `find` reference */
//	return rc;
//}
//
//extern int ldmsd_load_plugin(const char *inst_name,
//			     const char *plugin_name,
//			     char *errstr, size_t errlen);
//extern int ldmsd_term_plugin(const char *plugin_name);
//
//
//static int smplr_start_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *smplr_name, *interval_us, *offset, *attr_name;
//	int flags = 0;
//	struct ldmsd_sec_ctxt sctxt;
//	smplr_name = interval_us = offset = NULL;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	attr_name = "name";
//	smplr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!smplr_name)
//		goto einval;
//	interval_us = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INTERVAL);
//	offset = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_OFFSET);
//
//	flags = (reqc->flags & LDMSD_REQ_DEFER_FLAG)?(LDMSD_PERM_DSTART):0;
//	reqc->errcode = ldmsd_smplr_start(smplr_name, interval_us,
//					offset, 0, flags, &sctxt);
//	if (reqc->errcode == 0) {
//		goto send_reply;
//	} else if (reqc->errcode == EINVAL) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"interval '%s' invalid", interval_us);
//	} else if (reqc->errcode == -EINVAL) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The specified plugin is not a sampler.");
//	} else if (reqc->errcode == ENOENT) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Sampler '%s' not found.", smplr_name);
//	} else if (reqc->errcode == EBUSY) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Sampler '%s' is already running.", smplr_name);
//	} else if (reqc->errcode == EDOM) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Sampler parameters interval and offset are "
//				"incompatible.");
//	} else {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Failed to start the sampler '%s'.", smplr_name);
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by start.", attr_name);
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (smplr_name)
//		free(smplr_name);
//	if (interval_us)
//		free(interval_us);
//	if (offset)
//		free(offset);
//	return 0;
//}
//
//static int smplr_stop_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *smplr_name = NULL;
//	char *attr_name;
//	struct ldmsd_sec_ctxt sctxt;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	attr_name = "name";
//	smplr_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!smplr_name)
//		goto einval;
//
//	reqc->errcode = ldmsd_smplr_stop(smplr_name, &sctxt);
//	if (reqc->errcode == 0) {
//		goto send_reply;
//	} else if (reqc->errcode == ENOENT) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Sampler '%s' not found.", smplr_name);
//	} else if (reqc->errcode == EINVAL) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The plugin '%s' is not a sampler.",
//				smplr_name);
//	} else if (reqc->errcode == EBUSY) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The sampler '%s' is not running.", smplr_name);
//	} else {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Failed to stop sampler '%s'", smplr_name);
//	}
//	goto send_reply;
//
//einval:
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by stop.", attr_name);
//	reqc->errcode = EINVAL;
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (smplr_name)
//		free(smplr_name);
//	return 0;
//}
//
/* smplr_status */

int __smplr_status_json_obj(ldmsd_req_ctxt_t reqc, ldmsd_smplr_t smplr)
{
	extern const char *smplr_state_str(enum ldmsd_smplr_state state);
	int rc;
	ldmsd_set_entry_t sent;
	ldmsd_sampler_type_t samp;
	int first = 1;

	ldmsd_smplr_lock(smplr);
	rc = ldmsd_append_response_va(reqc, 0,
		       "{ \"name\":\"%s\","
		       "\"plugin\":\"%s\","
		       "\"instance\":\"%s\","
		       "\"interval_us\":\"%ld\","
		       "\"offset_us\":\"%ld\","
		       "\"synchronous\":\"%d\","
		       "\"state\":\"%s\","
		       "\"sets\": [ ",
		       smplr->obj.name,
		       smplr->pi->plugin_name,
		       smplr->pi->inst_name,
		       smplr->interval_us,
		       smplr->offset_us,
		       smplr->synchronous,
		       smplr_state_str(smplr->state)
		       );
	if (rc)
		goto out;
	samp = LDMSD_SAMPLER(smplr->pi);
	first = 1;
	LIST_FOREACH(sent, &samp->set_list, entry) {
		const char *name = ldms_set_instance_name_get(sent->set);
		rc = ldmsd_append_response_va(reqc, 0, "%s\"%s\"", first?"":",", name);
		if (rc)
			goto out;
		first = 0;
	}
	rc = ldmsd_append_response_va(reqc, 0, "]}");
 out:
	ldmsd_smplr_unlock(smplr);
	return rc;
}

static int smplr_status_handler(ldmsd_req_ctxt_t reqc)
{
	int rc;
	ldmsd_smplr_t smplr = NULL;
	char *name_s;
	int count;
	json_entity_t spec, name;

	spec = json_value_find(reqc->json, "spec");
	if (spec) {
		name = json_value_find(spec, "name");
		if (JSON_STRING_VALUE != json_entity_type(name)) {
			rc = ldmsd_send_type_error(reqc, "smplr_status:name", "a string");
			return rc;
		}
		name_s = json_value_str(name)->str;
		smplr = ldmsd_smplr_find(name_s);
		if (!smplr) {
			/* Do not report any status */
			rc = ldmsd_send_error(reqc, ENOENT,
					"smplr '%s' doesn't exist.", name_s);
			return rc;
		}
	}
	rc = ldmsd_append_info_obj_hdr(reqc, "smplr_status");
	if (rc)
		goto out;
	rc = ldmsd_append_response(reqc, 0, "[", 1);
	if (rc)
		goto out;
	if (smplr) {
		rc = __smplr_status_json_obj(reqc, smplr);
		if (rc)
			goto out;
		ldmsd_smplr_put(smplr);
	} else {
		ldmsd_cfg_lock(LDMSD_CFGOBJ_SMPLR);
		/* Determine the attribute value length */
		count = 0;
		for (smplr = ldmsd_smplr_first(); smplr;
				smplr = ldmsd_smplr_next(smplr)) {
			if (count) {
				rc = ldmsd_append_response(reqc, 0, ",", 1);
				if (rc)
					goto unlock_out;
			}
			rc = __smplr_status_json_obj(reqc, smplr);
			if (rc)
				goto unlock_out;
			count++;
		}
		ldmsd_cfg_unlock(LDMSD_CFGOBJ_SMPLR);
	}


	return ldmsd_append_response(reqc, LDMSD_REC_EOM_F, "]}", 2);

unlock_out:
	ldmsd_cfg_unlock(LDMSD_CFGOBJ_SMPLR);
out:
	if (smplr)
		ldmsd_smplr_put(smplr);
	return rc;
}

static int plugin_status_handler(ldmsd_req_ctxt_t reqc)
{
	int rc, count;
	ldmsd_plugin_inst_t inst;
	json_entity_t qr = NULL;
	json_entity_t val;
	jbuf_t jb = NULL;

	rc = ldmsd_append_response(reqc, LDMSD_REC_SOM_F, "[", 1);
	if (rc)
		goto out;
	count = 0;
	LDMSD_PLUGIN_INST_FOREACH(inst) {
		if (count) {
			rc = ldmsd_append_response(reqc, 0, ",\n", 1);
			if (rc)
				goto out;
		}
		count++;
		qr = inst->base->query(inst, "status");
		if (!qr) {
			rc = ENOMEM;
			goto out;
		}
		val = json_value_find(qr, "rc");
		if (!val) {
			ldmsd_log(LDMSD_LERROR, "The status JSON dict of "
					"plugin instance '%s' does not contain "
					"the attribute 'rc'.\n", inst->inst_name);
			rc = ldmsd_send_error(reqc, EINTR,
					"Failed to get the status of '%s'",
					inst->inst_name);
			goto out;
		}
		rc = json_value_int(val);
		if (rc) {
			rc = ldmsd_send_error(reqc, rc,
					"Failed to get the status of '%s'",
					inst->inst_name);
			goto out;
		}

		jb = json_entity_dump(NULL, json_value_find(qr, "status"));
		if (!jb) {
			rc = ENOMEM;
			goto out;
		}
		rc = ldmsd_append_response_va(reqc, 0, "%s", jb->buf);
		if (rc)
			goto out;
	}
	rc = ldmsd_append_response(reqc, LDMSD_REC_EOM_F, "]", 1);
out:
	if (qr)
		json_entity_free(qr);
	if (jb)
		jbuf_free(jb);
	return rc;
}
//
//
//static int __query_inst(ldmsd_req_ctxt_t reqc, const char *query,
//			ldmsd_plugin_inst_t inst)
//{
//	int rc = 0;
//	json_entity_t qr = NULL;
//	json_entity_t ra;
//	jbuf_t jb = NULL;
//	qr = inst->base->query(inst, query);
//	if (!qr) {
//		rc = ENOMEM;
//		goto out;
//	}
//	rc = json_value_int(json_attr_value(json_attr_find(qr, "rc")));
//	if (rc)
//		goto out;
//	ra = json_attr_find(qr, (char *)query);
//	if (!ra) {
//		/*
//		 * No query result
//		 */
//		rc = linebuf_printf(reqc, "%s", "");
//	} else {
//		jb = json_entity_dump(NULL, json_attr_value(ra));
//		if (!jb) {
//			rc = ENOMEM;
//			goto out;
//		}
//		rc = linebuf_printf(reqc, "%s", jb->buf);
//	}
//
//out:
//	if (qr)
//		json_entity_free(qr);
//	if (jb)
//		jbuf_free(jb);
//	reqc->errcode = rc;
//	return rc;
//}
//
//static int plugn_query_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	char *query;
//	char *name;
//	int count;
//	ldmsd_plugin_inst_t inst;
//
//	query = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_QUERY);
//	if (!query) {
//		rc = reqc->errcode = EINVAL;
//		snprintf(reqc->recv_buf, reqc->recv_len,
//			 "Missing `query` attribute");
//		ldmsd_send_req_response(reqc, reqc->recv_buf);
//		goto out;
//	}
//
//	/* optional */
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//
//	rc = linebuf_printf(reqc, "[");
//	if (rc)
//		goto err;
//	count = 0;
//
//	if (name) {
//		inst = ldmsd_plugin_inst_find(name);
//		if (!inst) {
//			rc = reqc->errcode = ENOENT;
//			(void) linebuf_printf(reqc,
//				"Plugin instance '%s' not found", name);
//			ldmsd_send_req_response(reqc, reqc->recv_buf);
//			goto out;
//		}
//		rc = __query_inst(reqc, query, inst);
//		if (rc)
//			goto err;
//	} else {
//		/*  query all instances */
//		LDMSD_PLUGIN_INST_FOREACH(inst) {
//			if (count) {
//				rc = linebuf_printf(reqc, ",\n");
//				if (rc)
//					goto err;
//			}
//			count++;
//			rc = __query_inst(reqc, query, inst);
//			if (rc)
//				goto err;
//		}
//	}
//
//	rc = linebuf_printf(reqc, "]");
//	if (rc) {
//		reqc->errcode = rc;
//		snprintf(reqc->recv_buf, reqc->recv_len,
//			 "Internal error: %d", rc);
//		ldmsd_send_req_response(reqc, reqc->recv_buf);
//		goto out;
//	}
//
//	struct ldmsd_req_attr_s attr;
//	attr.discrim = 1;
//	attr.attr_len = reqc->recv_off;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		goto out;
//	rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//	if (rc)
//		goto out;
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim,
//				sizeof(uint32_t), LDMSD_REC_EOM_F);
//	if (rc)
//		goto out;
//	goto out;
//err:
//	reqc->errcode = rc;
//	snprintf(reqc->recv_buf, reqc->recv_len, "query error: %d", rc);
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//out:
//	return rc;
//}
//

static int plugin_instance_handler(ldmsd_req_ctxt_t reqc)
{
	int rc = 0;
	json_entity_t spec, plugin, name, config, item;
	ldmsd_plugin_inst_t pi;
	char *plugin_s, *name_s;

	spec = json_value_find(reqc->json, "spec");
	name = json_value_find(spec, "name");
	if (!name) {
		rc = ldmsd_send_missing_attr_err(reqc, "cfg_obj:plugin_instance", "name");
		return rc;
	}
	if (JSON_STRING_VALUE != json_entity_type(name)) {
		rc = ldmsd_send_type_error(reqc, "plugin_instance:name", "a string");
		return rc;
	}
	name_s = json_value_str(name)->str;


	plugin = json_value_find(spec, "plugin");
	if (!plugin) {
		plugin_s = name_s;
	} else if (JSON_STRING_VALUE != json_entity_type(plugin)) {
		rc = ldmsd_send_type_error(reqc, "plugin_instance:plugin", "a string");
		return rc;
	} else {
		plugin_s = json_value_str(plugin)->str;
	}

	config = json_value_find(spec, "config");
	if (!config) {
		rc = ldmsd_send_missing_attr_err(reqc,
				"cfg_obj:plugin_instance", "config");
		return rc;
	}
	if ((JSON_DICT_VALUE != json_entity_type(config)) &&
			(JSON_LIST_VALUE != json_entity_type(config))) {
		/*
		 * The JSON_LIST_VALUE is supported because some plugins
		 * need to be configured multiple times in some setup, e.g.,
		 * test_sampler. These plugins MUST be redesigned on how they
		 * should be configured.
		 */
		rc = ldmsd_send_type_error(reqc, "plugin_instance:config",
				"a dictionary or a list of dictionaries");
		return rc;
	}
	/* reuse the receive buffer for an underlying error message */
	ldmsd_req_buf_reset(reqc->recv_buf);

	pi = ldmsd_plugin_inst_load(name_s, plugin_s,
			reqc->recv_buf->buf, reqc->recv_buf->len);
	if (!pi) {
		rc = ldmsd_send_error(reqc, errno, "%s", reqc->recv_buf->buf);
		return rc;
	}

	/*
	 * TODO: process 'perm' attribute
	 */


	/* Add the config dictionary */
	/*
	 * TODO: resolve this.
	 *
	 * Is it possible that a plugin instance will need multiple config object?
	 * I hope not. Any plugin that is required to be configured with multiple
	 * config lines should be re-designed so that all information will be
	 * contained in the plugin instance cfgobj.
	 *
	 * Before this change, the config json object of a plugin instance
	 * is designed to be a list of dictionary. Each dictionary represents
	 * a config line.
	 *
	 * TODO: change the list of dictionaries design to be just a dictionary.
	 *
	 */
	if (JSON_LIST_VALUE == json_entity_type(config)) {
		for (item = json_item_first(config); item; item = json_item_next(item)) {
			if (JSON_DICT_VALUE != json_entity_type(item)) {
				return ldmsd_send_type_error(reqc,
					"cfg_obj:plugin_instance:config",
					"a dictionary or a list of dictionaries.");
			}
		}
	}

	/*
	 * TODO: I hate this. I need to defer configuring plugin instances
	 * because sampler plugins create LDMS sets at config time.
	 *
	 * I strongly believe that the sampler plugin interface should
	 * include 'start()' which will create LDMS sets as configured and schedule
	 * the next sampling.
	 *
	 * The 'start()' will be called when the corresponding sampler policy
	 * is started.
	 */
	if (!ldmsd_is_initialized()) {
		ldmsd_deferred_pi_config_t cfg;
		cfg = ldmsd_deferred_pi_config_new(name_s, config,
							reqc->rem_key.msg_no,
							reqc->xprt->file.filename);
		if (!cfg) {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
			return ENOMEM;
		}
	} else {
		if (JSON_LIST_VALUE == json_entity_type(config)) {
			for (item = json_item_first(config); item;
					item = json_item_next(item)) {
				rc = ldmsd_plugin_inst_config(pi, item,
							reqc->recv_buf->buf,
							reqc->recv_buf->len);
				/*
				 * Send the config error to the requester.
				 */
				if (rc) {
					rc = ldmsd_send_error(reqc, rc, "%s",
							reqc->recv_buf->buf);
					return rc;
				}
			}
		} else {
			rc = ldmsd_plugin_inst_config(pi, config,
					reqc->recv_buf->buf, reqc->recv_buf->len);
			if (rc) {
				rc = ldmsd_send_error(reqc, rc, "%s",
						reqc->recv_buf->buf);
				return rc;
			}
		}

	}
	rc = ldmsd_send_error(reqc, 0, "");
	return rc;
}

//static int plugn_term_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *plugin_name, *attr_name;
//	plugin_name = NULL;
//
//	attr_name = "name";
//	plugin_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!plugin_name)
//		goto einval;
//
//	reqc->errcode = ldmsd_term_plugin(plugin_name);
//	if (reqc->errcode == 0) {
//		goto send_reply;
//	} else if (reqc->errcode == ENOENT) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"plugin '%s' not found.", plugin_name);
//	} else if (reqc->errcode == EINVAL) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The specified plugin '%s' has "
//				"active users and cannot be terminated.",
//				plugin_name);
//	} else {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Failed to terminate the plugin '%s'.",
//				plugin_name);
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by term.", attr_name);
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (plugin_name)
//		free(plugin_name);
//	return 0;
//}
//
//static int plugn_config_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *config_attr, *attr_name;
//	name = config_attr = NULL;
//	ldmsd_plugin_inst_t inst = NULL;
//	char *token, *next_token, *ptr;
//	json_entity_t d, a;
//	int rc = 0;
//
//	reqc->errcode = 0;
//	d = NULL;
//
//	attr_name = "name";
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name)
//		goto einval;
//	config_attr = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_STRING);
//
//	d = json_entity_new(JSON_DICT_VALUE);
//	if (!d)
//		goto enomem;
//
//	if (config_attr) {
//		for (token = strtok_r(config_attr, " \t\n", &ptr); token;) {
//			char *value = strchr(token, '=');
//			if (value) {
//				value[0] = '\0';
//				value++;
//			} else {
//				value = "";
//			}
//			a = json_entity_new(JSON_ATTR_VALUE,
//					json_entity_new(JSON_STRING_VALUE, token),
//					json_entity_new(JSON_STRING_VALUE, value));
//			if (!a)
//				goto enomem;
//			json_attr_add(d, a);
//			next_token = strtok_r(NULL, " \t\n", &ptr);
//			token = next_token;
//		}
//	}
//
//	inst = ldmsd_plugin_inst_find(name);
//	if (!inst) {
//		linebuf_printf(reqc, "Instance not found: %s", name);
//		reqc->errcode = ENOENT;
//		goto send_reply;
//	}
//
//	if (reqc->flags & LDMSD_REQ_DEFER_FLAG) {
//		ldmsd_deferred_pi_config_t cfg;
//		cfg = ldmsd_deferred_pi_config_new(name, d,
//							reqc->key.msg_no,
//							reqc->xprt->file.filename);
//		if (!cfg) {
//			ldmsd_log(LDMSD_LERROR, "Memory allocation failure\n");
//			goto enomem;
//		}
//	} else {
//		reqc->errcode = ldmsd_plugin_inst_config(inst, d,
//							 reqc->recv_buf,
//							 reqc->recv_len);
//		/* if there is an error, the plugin should already fill line_buf */
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	(void)snprintf(reqc->recv_buf, reqc->recv_len,
//		 "The attribute '%s' is required by config.", attr_name);
//	goto send_reply;
//enomem:
//	rc = reqc->errcode = ENOMEM;
//	(void)snprintf(reqc->recv_buf, reqc->recv_len, "Out of memory");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (config_attr)
//		free(config_attr);
//	if (d)
//		json_entity_free(d);
//	if (inst)
//		ldmsd_plugin_inst_put(inst); /* put ref from find */
//	return rc;
//}
//
//extern struct plugin_list plugin_list;
//int __plugn_list_string(ldmsd_req_ctxt_t reqc)
//{
//	char *plugin;
//	const char *desc;
//	int rc, count = 0;
//	ldmsd_plugin_inst_t inst;
//	rc = 0;
//
//	plugin = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PLUGIN);
//	LDMSD_PLUGIN_INST_FOREACH(inst) {
//		if (plugin && strcmp(plugin, inst->type_name)
//			   && strcmp(plugin, inst->plugin_name)) {
//			/* does not match type nor plugin name */
//			continue;
//		}
//		desc = ldmsd_plugin_inst_desc(inst);
//		if (!desc)
//			desc = inst->plugin_name;
//		rc = linebuf_printf(reqc, "%s - %s\n", inst->inst_name, desc);
//		if (rc)
//			goto out;
//		count++;
//	}
//
//	if (!count) {
//		rc = linebuf_printf(reqc, "-- No plugin instances --");
//		reqc->errcode = ENOENT;
//	}
//out:
//	if (plugin)
//		free(plugin);
//	return rc;
//}
//
//static int plugn_list_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	struct ldmsd_req_attr_s attr;
//
//	rc = __plugn_list_string(reqc);
//	if (rc)
//		return rc;
//
//	attr.discrim = 1;
//	attr.attr_len = reqc->recv_off;
//	attr.attr_id = LDMSD_ATTR_STRING;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr),
//				LDMSD_REC_SOM_F);
//	if (rc)
//		return rc;
//	rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//	if (rc)
//		return rc;
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim, sizeof(uint32_t),
//				LDMSD_REC_EOM_F);
//	return rc;
//}
//
//static int plugn_usage_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	char *name = NULL;
//	char *type = NULL;
//	const char *usage = NULL;
//	ldmsd_plugin_inst_t inst = NULL;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (name) {
//		inst = ldmsd_plugin_inst_find(name);
//		if (!inst) {
//			rc = reqc->errcode = ENOENT;
//			snprintf(reqc->recv_buf, reqc->recv_len,
//				 "Plugin instance `%s` not found.", name);
//			goto send_reply;
//		}
//		usage = ldmsd_plugin_inst_help(inst);
//		if (!usage) {
//			rc = reqc->errcode = ENOSYS;
//			snprintf(reqc->recv_buf, reqc->recv_len,
//				 "`%s` has no usage", name);
//			goto send_reply;
//		}
//		linebuf_printf(reqc, "%s\n", usage);
//	}
//
//	type = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_TYPE);
//	if (type) {
//		if (0 == strcmp(type, "true")) {
//			if (!name) {
//				reqc->errcode = EINVAL;
//				snprintf(reqc->recv_buf, reqc->recv_len,
//						"The 'name' must be given if type=true");
//				goto send_reply;
//			}
//			type = (char *)inst->type_name;
//		}
//		if (0 == strcmp(type, "sampler")) {
//			usage = ldmsd_sampler_help();
//			linebuf_printf(reqc, "\nCommon attributes of sampler plugin instances\n");
//			linebuf_printf(reqc, "%s", usage);
//		} else if (0 == strcmp(type, "store")) {
//			usage = ldmsd_store_help();
//			linebuf_printf(reqc, "\nCommon attributes of store plugin instance\n");
//			linebuf_printf(reqc, "%s", usage);
//		} else if (0 == strcmp(type, "all")) {
//			usage = ldmsd_sampler_help();
//			linebuf_printf(reqc, "\nCommon attributes of sampler plugin instances\n");
//			linebuf_printf(reqc, "%s", usage);
//
//			usage = ldmsd_store_help();
//			linebuf_printf(reqc, "\nCommon attributes of store plugin instance\n");
//			linebuf_printf(reqc, "%s", usage);
//		} else {
//			reqc->errcode = EINVAL;
//			snprintf(reqc->recv_buf, reqc->recv_len,
//					"Invalid type value '%s'", type);
//			goto send_reply;
//		}
//	}
//
//	if (!name && !type) {
//		reqc->errcode = EINVAL;
//		snprintf(reqc->recv_buf, reqc->recv_len, "Either 'name' or 'type' must be given.");
//	}
//
// send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (inst)
//		ldmsd_plugin_inst_put(inst); /* put ref from find */
//	return rc;
//}
//
///* Caller must hold the set tree lock. */
//int __plugn_sets_json_obj(ldmsd_req_ctxt_t reqc,
//				ldmsd_plugin_inst_t inst)
//{
//	ldmsd_set_entry_t ent;
//	ldmsd_sampler_type_t samp = (void*)inst->base;
//	int rc, set_count;
//	rc = linebuf_printf(reqc,
//			"{"
//			"\"plugin\":\"%s\","
//			"\"sets\":[",
//			inst->inst_name);
//	if (rc)
//		return rc;
//	set_count = 0;
//	LIST_FOREACH(ent, &samp->set_list, entry) {
//		if (set_count) {
//			linebuf_printf(reqc, ",\"%s\"",
//				       ldms_set_instance_name_get(ent->set));
//		} else {
//			linebuf_printf(reqc, "\"%s\"",
//				       ldms_set_instance_name_get(ent->set));
//		}
//		set_count++;
//		if (rc)
//			return rc;
//	}
//	rc = linebuf_printf(reqc, "]}");
//	return rc;
//}
//
//static int plugn_sets_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	struct ldmsd_req_attr_s attr;
//
//	char *name;
//	int plugn_count;
//	ldmsd_plugin_inst_t inst;
//
//	rc = linebuf_printf(reqc, "[");
//	if (rc)
//		goto err;
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (name) {
//		inst = ldmsd_plugin_inst_find(name);
//		if (!inst) {
//			snprintf(reqc->recv_buf, reqc->recv_len,
//					"Plugin instance '%s' not found",
//					name);
//			reqc->errcode = ENOENT;
//			goto err0;
//		}
//		rc = __plugn_sets_json_obj(reqc, inst);
//		ldmsd_plugin_inst_put(inst);
//		if (rc)
//			goto err;
//	} else {
//		plugn_count = 0;
//		LDMSD_PLUGIN_INST_FOREACH(inst) {
//			if (strcmp(inst->type_name, "sampler"))
//				continue; /* skip non-sampler instance */
//			if (plugn_count) {
//				rc = linebuf_printf(reqc, ",");
//				if (rc)
//					goto err;
//			}
//			rc = __plugn_sets_json_obj(reqc, inst);
//			if (rc)
//				goto err;
//			plugn_count += 1;
//		}
//	}
//	rc = linebuf_printf(reqc, "]");
//	if (rc)
//		goto err;
//
//	attr.discrim = 1;
//	attr.attr_len = reqc->recv_off;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr),
//				LDMSD_REC_SOM_F);
//	if (rc)
//		return rc;
//	rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//	if (rc)
//		return rc;
//	attr.discrim = 0;
//	rc = ldmsd_append_reply(reqc, (char *)&attr.discrim, sizeof(uint32_t),
//				LDMSD_REC_EOM_F);
//	return rc;
//
//err:
//	ldmsd_send_error_reply(reqc->xprt, reqc->rec_no, rc,
//						"internal error", 15);
//	goto out;
//err0:
//	if (name)
//		free(name);
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	goto out;
//out:
//	return rc;
//}
//
//extern int ldmsd_set_udata(const char *set_name, const char *metric_name,
//			   const char *udata_s, ldmsd_sec_ctxt_t sctxt);
//static int set_udata_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *set_name, *metric_name, *udata, *attr_name;
//	set_name = metric_name = udata = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "instance";
//	set_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INSTANCE);
//	if (!set_name)
//		goto einval;
//	attr_name = "metric";
//	metric_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_METRIC);
//	if (!metric_name)
//		goto einval;
//	attr_name = "udata";
//	udata = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_UDATA);
//	if (!udata)
//		goto einval;
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//
//	reqc->errcode = ldmsd_set_udata(set_name, metric_name, udata, &sctxt);
//	switch (reqc->errcode) {
//	case 0:
//		break;
//	case EACCES:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Permission denied.");
//		break;
//	case ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Set '%s' not found.", set_name);
//		break;
//	case -ENOENT:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Metric '%s' not found in Set '%s'.",
//				metric_name, set_name);
//		break;
//	case EINVAL:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"User data '%s' is invalid.", udata);
//		break;
//	default:
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "Error %d %s", reqc->errcode,
//			       ovis_errno_abbvr(reqc->errcode));
//	}
//	goto out;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required.", attr_name);
//out:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (set_name)
//		free(set_name);
//	if (metric_name)
//		free(metric_name);
//	if (udata)
//		free(udata);
//	return 0;
//}
//
//extern int ldmsd_set_udata_regex(char *set_name, char *regex_str,
//		char *base_s, char *inc_s, char *er_str, size_t errsz,
//		ldmsd_sec_ctxt_t sctxt);
//static int set_udata_regex_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *set_name, *regex, *base_s, *inc_s, *attr_name;
//	set_name = regex = base_s = inc_s = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	reqc->errcode = 0;
//
//	attr_name = "instance";
//	set_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INSTANCE);
//	if (!set_name)
//		goto einval;
//	attr_name = "regex";
//	regex = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_REGEX);
//	if (!regex)
//		goto einval;
//	attr_name = "base";
//	base_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_BASE);
//	if (!base_s)
//		goto einval;
//
//	inc_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_INCREMENT);
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_set_udata_regex(set_name, regex, base_s, inc_s,
//				reqc->recv_buf, reqc->recv_len, &sctxt);
//	goto out;
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required.", attr_name);
//out:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (set_name)
//		free(set_name);
//	if (base_s)
//		free(base_s);
//	if (regex)
//		free(regex);
//	if (inc_s)
//		free(inc_s);
//	return 0;
//}
//
//static int verbosity_change_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *level_s = NULL;
//	int is_test = 0;
//
//	level_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_LEVEL);
//	if (!level_s) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"The attribute 'level' is required.");
//		goto out;
//	}
//
//	int rc = ldmsd_loglevel_set(level_s);
//	if (rc < 0) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Invalid verbosity level, expecting DEBUG, "
//				"INFO, ERROR, CRITICAL and QUIET\n");
//		goto out;
//	}
//
//	if (ldmsd_req_attr_keyword_exist_by_id(reqc->req_buf, LDMSD_ATTR_TEST))
//		is_test = 1;
//
//	if (is_test) {
//		ldmsd_log(LDMSD_LDEBUG, "TEST DEBUG\n");
//		ldmsd_log(LDMSD_LINFO, "TEST INFO\n");
//		ldmsd_log(LDMSD_LWARNING, "TEST WARNING\n");
//		ldmsd_log(LDMSD_LERROR, "TEST ERROR\n");
//		ldmsd_log(LDMSD_LCRITICAL, "TEST CRITICAL\n");
//		ldmsd_log(LDMSD_LALL, "TEST ALWAYS\n");
//	}
//
//out:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (level_s)
//		free(level_s);
//	return 0;
//}
//
//int __daemon_status_json_obj(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//
//	extern pthread_t *ev_thread;
//	extern int *ev_count;
//	int i;
//
//	rc = linebuf_printf(reqc, "[");
//	if (rc)
//		return rc;
//	for (i = 0; i < ldmsd_ev_thread_count_get(); i++) {
//		if (i) {
//			rc = linebuf_printf(reqc, ",\n");
//			if (rc)
//				return rc;
//		}
//
//		rc = linebuf_printf(reqc,
//				"{ \"thread\":\"%p\","
//				"\"task_count\":\"%d\"}",
//				(void *)ev_thread[i], ev_count[i]);
//		if (rc)
//			return rc;
//	}
//	rc = linebuf_printf(reqc, "]");
//	return rc;
//}
//
//static int daemon_status_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	struct ldmsd_req_attr_s attr;
//
//	rc = __daemon_status_json_obj(reqc);
//	if (rc)
//		return rc;
//
//	attr.discrim = 1;
//	attr.attr_len = reqc->recv_off;
//	attr.attr_id = LDMSD_ATTR_JSON;
//	ldmsd_hton_req_attr(&attr);
//	rc = ldmsd_append_reply(reqc, (char *)&attr, sizeof(attr), LDMSD_REC_SOM_F);
//	if (rc)
//		return rc;
//	rc = ldmsd_append_reply(reqc, reqc->recv_buf, reqc->recv_off, 0);
//	if (rc)
//		return rc;
//	attr.discrim = 0;
//	ldmsd_append_reply(reqc, (char *)&attr.discrim, sizeof(uint32_t), LDMSD_REC_EOM_F);
//	return rc;
//}
//
//static int version_handler(ldmsd_req_ctxt_t reqc)
//{
//	struct ldms_version ldms_version;
//	struct ldmsd_version ldmsd_version;
//
//	ldms_version_get(&ldms_version);
//	size_t cnt = snprintf(reqc->recv_buf, reqc->recv_len,
//			"LDMS Version: %hhu.%hhu.%hhu.%hhu\n",
//			ldms_version.major, ldms_version.minor,
//			ldms_version.patch, ldms_version.flags);
//
//	ldmsd_version_get(&ldmsd_version);
//	cnt += snprintf(&reqc->recv_buf[cnt], reqc->recv_len-cnt,
//			"LDMSD Version: %hhu.%hhu.%hhu.%hhu",
//			ldmsd_version.major, ldmsd_version.minor,
//			ldmsd_version.patch, ldmsd_version.flags);
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//
//
//}
//
///*
// * The tree contains environment variables given in
// * configuration files or via ldmsd_controller/ldmsctl.
// */
//int env_cmp(void *a, const void *b)
//{
//	return strcmp(a, b);
//}
//struct rbt env_tree = RBT_INITIALIZER(env_cmp);
//pthread_mutex_t env_tree_lock  = PTHREAD_MUTEX_INITIALIZER;
//
//struct env_node {
//	char *name;
//	struct rbn rbn;
//};
//
//static int env_node_new(const char *name, struct rbt *tree, pthread_mutex_t *lock)
//{
//	struct env_node *env_node;
//	struct rbn *rbn;
//
//	rbn = rbt_find(tree, name);
//	if (rbn) {
//		/* The environment variable is already recorded.
//		 * Its value will be retrieved by calling getenv().
//		 */
//		return EEXIST;
//	}
//
//	env_node = malloc(sizeof(*env_node));
//	if (!env_node)
//		return ENOMEM;
//	env_node->name = strdup(name);
//	if (!env_node->name)
//		return ENOMEM;
//	rbn_init(&env_node->rbn, env_node->name);
//	if (lock) {
//		pthread_mutex_lock(lock);
//		rbt_ins(tree, &env_node->rbn);
//		pthread_mutex_unlock(lock);
//	} else {
//		rbt_ins(tree, &env_node->rbn);
//	}
//
//	return 0;
//}
//
//static void env_node_del(struct env_node *node)
//{
//	free(node->name);
//	free(node);
//}
//
///*
// * { "obj" : "env",
// *   "spec": { "name": <env name string>,
// *             "value": <env value string>
// *           }
// * }
// */
//
//static int env_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc = 0;
//	json_entity_t spec;
//	json_entity_t name;
//	json_entity_t value;
//	char *name_s, *val_s;
//	char *exp_val = NULL;
//
//	spec = json_value_find(reqc->json, "spec");
//	if (!spec) {
//		__missing_required_json_attr(reqc, "spec");
//		return EINVAL;
//	}
//
//	name = json_value_find(spec, "name");
//	if (!name) {
//		__missing_required_spec_attr(reqc, "env", "name");
//		return EINVAL;
//	}
//	name_s = json_value_str(name)->str;
//
//	value = json_value_find(spec, "value");
//	if (!value) {
//		__missing_required_spec_attr(reqc, "env", "value");
//		return EINVAL;
//	}
//	val_s = json_value_str(value)->str;
//
//	if (reqc->xprt->trust) { /* TODO: why we check this */
//		exp_val = str_repl_cmd(val_s);
//		if (!exp_val) {
//			rc = errno;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//				"Failed to replace the string '%s'",
//				json_value_str(value)->str);
//			goto out;
//		}
//
//		rc = setenv(name_s, exp_val, 1);
//		if (rc) {
//			rc = errno;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//				"Failed to set the env: %s=%s",
//				json_value_str(name)->str,
//				exp_val);
//			goto out;
//		}
//
//		/*
//		 * Record the set environment variable for exporting purpose
//		 */
//		rc = env_node_new(name_s, &env_tree, &env_tree_lock);
//		if (rc == ENOMEM) {
//			ldmsd_log(LDMSD_LERROR, "Out of memory. "
//					"Failed to record a given env: %s=%s\n",
//					name_s, exp_val);
//			/* Keep setting the remaining give environment variables */
//			goto out;
//		} else if (rc == EEXIST) {
//			ldmsd_log(LDMSD_LINFO, "Reset the env: %s=%s\n",
//					name_s, val_s);
//			rc = 0;
//		} else if (rc == 0) {
//			ldmsd_log(LDMSD_LINFO, "Set the env: %s=%s",
//					name_s, val_s);
//		}
//	}
//
//out:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (exp_val)
//		free(exp_val);
//	return rc;
//}
//
static int include_handler(ldmsd_req_ctxt_t reqc)
{
	json_entity_t path, spec;
	char *path_s = NULL;
	int rc = 0;

	spec = json_value_find(reqc->json, "spec");
	path = json_value_find(spec, "path");
	if (!path)
		return ldmsd_send_missing_attr_err(reqc, "cmd:include", "path");
	if (JSON_STRING_VALUE != json_entity_type(path))
		return ldmsd_send_type_error(reqc, "cmd:include:path", "a string");
	path_s = json_value_str(path)->str;
	rc = process_config_file(path_s, reqc->xprt->trust);
	if (rc) {
		/*
		 * The actual error is always reported to the log file
		 * with the line number of the config file path.
		 */
		if (LDMSD_CFG_XPRT_CONFIG_FILE != reqc->xprt->type) {
			rc = ldmsd_send_error(reqc, rc,
				"An error occurs when including the config file %s. "
				"Please see the detail in the log file.",
				path_s);
		} else {
			rc = ldmsd_send_error(reqc, rc, "Failed to include "
						"the config file '%s'", path_s);
		}
	}

	return rc;
}
//
//static int oneshot_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *name, *time_s, *attr_name;
//	name = time_s = NULL;
//	int rc = 0;
//	struct ldmsd_sec_ctxt sctxt;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		attr_name = "name";
//		goto einval;
//	}
//	time_s = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_TIME);
//	if (!time_s) {
//		attr_name = "time";
//		goto einval;
//	}
//
//	ldmsd_req_ctxt_sec_get(reqc, &sctxt);
//	reqc->errcode = ldmsd_smplr_oneshot(name, time_s, &sctxt);
//	if (reqc->errcode) {
//		if ((int)reqc->errcode < 0) {
//			reqc->errcode = -reqc->errcode;
//			linebuf_printf(reqc, "Failed to get the current time. "
//					"Error %d", reqc->errcode);
//		} else if (reqc->errcode == EINVAL) {
//			linebuf_printf(reqc, "The given timestamp is in the past.");
//		} else if (reqc->errcode == ENOENT) {
//			linebuf_printf(reqc, "The sampler policy '%s' not found",
//									name);
//		} else {
//			linebuf_printf(reqc, "Failed to do the oneshot. Error %d",
//								reqc->errcode);
//		}
//	}
//	goto send_reply;
//
//einval:
//	reqc->errcode = EINVAL;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The attribute '%s' is required by oneshot.",
//			attr_name);
//
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	if (name)
//		free(name);
//	if (time_s)
//		free(time_s);
//	return rc;
//}
//
//extern int ldmsd_logrotate();
//static int logrotate_handler(ldmsd_req_ctxt_t reqc)
//{
//	reqc->errcode = ldmsd_logrotate();
//	if (reqc->errcode) {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"Failed to rotate the log file. %s",
//				strerror(reqc->errcode));
//	}
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//
//static int exit_daemon_handler(ldmsd_req_ctxt_t reqc)
//{
//	cleanup_requested = 1;
//	ldmsd_log(LDMSD_LINFO, "User requested exit.\n");
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"exit daemon request received");
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}

extern size_t __get_remaining(size_t max_msg, ldmsd_req_buf_t buf);
static int
__greeting_rsp_multi_rec_handler(ldmsd_req_ctxt_t reqc, json_entity_t num_rec)
{
	int rc = 0;
	int curr = 0;
	uint64_t tot_num_rec;
	char *str;

	tot_num_rec = json_value_int(num_rec);
	if ((rc = ldmsd_append_info_obj_hdr(reqc, "greeting")))
		goto out;
	str = "{\"mode\":\"rsp_multi_rec\",\"records\":[";
	if ((rc = ldmsd_append_response(reqc, 0, str, strlen(str))))
		goto out;
	if ((rc = ldmsd_append_response_va(reqc, 0, "\"rec_%d", curr)))
		goto out;
	do {
		if (curr + 1 == tot_num_rec) {
			/* Pad to the end of the record */
			rc = ldmsd_append_response_va(reqc, LDMSD_REC_EOM_F, "%*s\"]}}",
				__get_remaining(reqc->xprt->max_msg, reqc->send_buf) - 4, "");
			goto out;
		}
		rc = ldmsd_append_response_va(reqc, 0, "%*s\"",
				__get_remaining(reqc->xprt->max_msg, reqc->send_buf) - 1, "");
		if (rc)
			goto out;
		curr++;
		/* \c *curr is assumed to be incremented by 1 */
		if ((rc = ldmsd_append_response_va(reqc, 0, ",\"rec_%d", curr)))
			goto out;
	} while (curr < tot_num_rec);

out:
	return rc;
}

static int
__greeting_echo_handler(ldmsd_req_ctxt_t reqc, json_entity_t v)
{
	int rc;
	int cnt = 0;
	char *str;
	json_entity_t item;
	jbuf_t jb = NULL;

	if ((rc = ldmsd_append_info_obj_hdr(reqc, "greeting")))
		goto out;
	str = "{\"mode\":\"echo\",\"list\":[";
	if ((rc = ldmsd_append_response(reqc, 0, str, strlen(str))))
		goto out;
	if (JSON_LIST_VALUE != json_entity_type(v)) {
		ldmsd_log(LDMSD_LINFO, "greeting handler: Received a wrong JSON type\n");
		rc = ldmsd_send_error(reqc, EINVAL, "The JSON object of "
				"the 'value' attribute has type '%s' "
				"instead of '%s'\n", json_type_str(json_entity_type(v)),
				json_type_str(JSON_LIST_VALUE));
		if (rc)
			goto out;
	}
	for (item = json_item_first(v); item; item = json_item_next(item)) {
		if (cnt) {
			if ((rc = ldmsd_append_response(reqc, 0, ",", 1)))
				goto out;
		}
		jb = json_entity_dump(jb, item);
		if (!jb) {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
			goto out;
		}
		if ((rc = ldmsd_append_response_va(reqc, 0, "%s", jb->buf)))
			goto out;
		jbuf_free(jb);
		jb = NULL;
		cnt++;
	}
	rc = ldmsd_append_response(reqc, LDMSD_REC_EOM_F, "]}}", 3);
out:
	return rc;
}

typedef struct greeting_ping_pong_ctxt *greeting_ping_pong_ctxt_t;
struct greeting_ping_pong_ctxt {
	ldmsd_req_ctxt_t org_req;
	ldmsd_req_ctxt_t req;
	json_str_t s;
	int round;
	greeting_ping_pong_ctxt_t prev;
};

static void __cleanup_greeting_ping_pong_ctxt(greeting_ping_pong_ctxt_t ctxt)
{
	greeting_ping_pong_ctxt_t prev;
	ldmsd_req_ctxt_ref_put(ctxt->org_req, "greeting_ctxt:ref");
	do {
		prev = ctxt->prev;
		free(ctxt);
		ctxt = prev;
	} while (ctxt);
}

static int __greeting_ping_pong_response(ldmsd_req_ctxt_t reqc);
static int __greeting_ping_pong_request(greeting_ping_pong_ctxt_t ctxt)
{
	ldmsd_req_ctxt_t reqc;
	struct ldmsd_msg_key key;
	char *s;
	int rc;

	ldmsd_msg_key_get(ctxt->org_req->xprt->ldms.ldms, &key);
	reqc = ldmsd_req_ctxt_alloc(&key, ctxt->org_req->xprt);
	if (!reqc){
		ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
		return ENOMEM;
	}
	ctxt->req = reqc;
	reqc->ctxt = ctxt;
	reqc->resp_handler = __greeting_ping_pong_response;

	s = "{\"type\":\"cmd_obj\",\"cmd\":\"ping_pong\",\"spec\":";
	rc = ldmsd_append_request_va(reqc, LDMSD_REC_SOM_F, "%s", s);
	if (rc)
		goto err;
	s = "{\"round\":";
	rc = ldmsd_append_request_va(reqc, 0, "%s", s);
	if (rc)
		goto err;
	rc = ldmsd_append_request_va(reqc, LDMSD_REC_EOM_F, "%d}}", ctxt->round);
	if (rc)
		goto err;
	return 0;
err:
	ldmsd_req_ctxt_free(reqc);
	return rc;
}

static int
__greeting_ping_pong_words(ldmsd_req_ctxt_t reqc, json_str_t last_word)
{
	int rc;
	char *hdr;
	greeting_ping_pong_ctxt_t ctxt;
	ldmsd_req_ctxt_t org_req;

	ctxt = (greeting_ping_pong_ctxt_t)reqc->ctxt;

	hdr = "{\"mode\":\"ping_pong\",";

	if (!ctxt) {
		org_req = reqc;
		/*
		 * Client doesn't request LDSMD to ask for words.
		 */
		rc = ldmsd_append_info_obj_hdr(org_req, "greeting");
		if (rc)
			goto out;
		rc = ldmsd_append_response_va(org_req, 0, "%s", hdr);
		if (rc)
			goto out;
		rc = ldmsd_append_response_va(org_req, LDMSD_REC_EOM_F,
					"\"round\":1,\"words\":[\"%s\"]}}",
							last_word->str);
	} else {
		/* Get the original request */
		org_req = ctxt->org_req;
		rc = ldmsd_append_info_obj_hdr(org_req, "greeting");
		if (rc)
			goto out;
		rc = ldmsd_append_response_va(org_req, 0, "%s", hdr);
		if (rc)
			goto out;
		rc = ldmsd_append_response_va(org_req, 0,
				"\"round\":%d,\"words\":[\"%s\"",
				ctxt->round, last_word->str);
		if (rc)
			goto out;
		do {
			rc = ldmsd_append_response_va(org_req, 0, ",\"\%s\"",
								ctxt->s->str);
			if (rc)
				goto out;

			ctxt = ctxt->prev;
		} while (ctxt);
		rc = ldmsd_append_response(org_req, LDMSD_REC_EOM_F, "]}}", 3);
	}

out:
	return rc;
}

static int __greeting_ping_pong_response(ldmsd_req_ctxt_t reqc)
{
	json_entity_t spec, mode, value;
	json_str_t mode_s;
	int rc = 0;
	json_entity_t word;
	json_entity_t more;
	greeting_ping_pong_ctxt_t ctxt, prev;

	ctxt = prev = (greeting_ping_pong_ctxt_t)reqc->ctxt;

	spec = json_value_find(reqc->json, "spec");
	mode = json_value_find(spec, "mode");
	if (!mode) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting", "mode");
		goto out;
	}
	mode_s = json_value_str(mode);

	if (0 != strncmp(mode_s->str, "ping_pong", mode_s->str_len)) {
		rc = ldmsd_send_error(reqc, ENOTSUP, "Received unexpected mode %s",
								mode_s->str);
		goto out;
	}

	value = json_value_find(spec, "value");
	if (!value) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting/ping_pong", "value");
		goto out;
	}

	if (JSON_DICT_VALUE != json_entity_type(value)) {
		ldmsd_log(LDMSD_LINFO, "greeting handler: Received a wrong JSON type\n");
		rc = ldmsd_send_error(reqc, EINVAL, "The JSON object of "
				"the 'value' attribute has type '%s' "
				"instead of '%s'\n",
				json_type_str(json_entity_type(value)),
				json_type_str(JSON_DICT_VALUE));
		goto out;
	}

	more = json_value_find(value, "more");
	word = json_value_find(value, "word");

	if (!word) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting:ping_pong", "word");
		goto out;
	}

	if (more && (json_value_bool(more))) {
		ctxt = malloc(sizeof(*ctxt));
		if (!ctxt) {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
			rc = ENOMEM;
			goto out;
		}
		ctxt->org_req = prev->org_req;
		ctxt->s = json_value_str(word);
		ctxt->prev = prev;
		ctxt->round = prev->round + 1;


		/* Create a request to send back to the requester for the second word.  */
		rc = __greeting_ping_pong_request(ctxt);
	} else {
		rc = __greeting_ping_pong_words(reqc, json_value_str(word));
	}
	if (prev->req) {
		/*
		 * We got the response of this request already,
		 * so the context should not be used anymore.
		 */
		ldmsd_req_ctxt_free(prev->req);
	}

out:
	if (!more || rc) {
		if (ctxt)
			__cleanup_greeting_ping_pong_ctxt(ctxt);
	}
	return rc;
}


static int
__greeting_ping_pong_handler(ldmsd_req_ctxt_t reqc, json_entity_t v)
{
	int rc;
	json_entity_t word;
	json_entity_t more;
	greeting_ping_pong_ctxt_t ctxt = NULL;

	if (JSON_DICT_VALUE != json_entity_type(v)) {
		ldmsd_log(LDMSD_LINFO, "greeting handler: Received a wrong JSON type\n");
		rc = ldmsd_send_error(reqc, EINVAL, "The JSON object of "
				"the 'value' attribute has type '%s' "
				"instead of '%s'\n", json_type_str(json_entity_type(v)),
				json_type_str(JSON_DICT_VALUE));
		goto out;
	}

	more = json_value_find(v, "more");
	word = json_value_find(v, "word");

	if (!word) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting:ping_pong", "word");
		goto out;
	}

	if (more && (json_value_bool(more))) {
		ctxt = malloc(sizeof(*ctxt));
		if (!ctxt) {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
			rc = ENOMEM;
			goto out;
		}
		ldmsd_req_ctxt_ref_get(reqc, "greeting_ctxt:ref");
		ctxt->org_req = reqc;
		ctxt->s = json_value_str(word);
		ctxt->prev = NULL;
		ctxt->round = 1;

		/* Create a request to send back to the requester for the second word.  */
		rc = __greeting_ping_pong_request(ctxt);
		if (rc)
			__cleanup_greeting_ping_pong_ctxt(ctxt);
	} else {
		rc = __greeting_ping_pong_words(reqc, json_value_str(word));
	}
out:
	return rc;
}

static int
__greeting_long_rsp_handler(ldmsd_req_ctxt_t reqc, json_entity_t v)
{
	int rc;
	json_entity_t len_s;
	int len;

	if (JSON_DICT_VALUE != json_entity_type(v)) {
		ldmsd_log(LDMSD_LINFO, "greeting handler: Received a wrong JSON type\n");
		rc = ldmsd_send_error(reqc, EINVAL, "The JSON object of "
				"the 'value' attribute has type '%s' "
				"instead of '%s'\n", json_type_str(json_entity_type(v)),
				json_type_str(JSON_DICT_VALUE));
		goto out;
	}

	len_s = json_value_find(v, "length");
	if (!len_s) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting:long_rsp", "length");
		goto out;
	}
	if (JSON_INT_VALUE != json_entity_type(len_s)) {
		ldmsd_log(LDMSD_LINFO, "greeting/long_rsp: \"length\" must be an integer\n");
		rc = ldmsd_send_error(reqc, EINVAL, "'length' must be an integer.");
		goto out;
	}
	len = (int)json_value_int(len_s);

	rc = ldmsd_append_info_obj_hdr(reqc, "greeting");
	if (rc)
		return rc;

	rc = ldmsd_append_response_va(reqc, LDMSD_REC_EOM_F,
				"{\"mode\":\"long_rsp\","
				"\"msg\":\"%0*d\"}}", len, len);
out:
	return rc;
}

static int greeting_handler(ldmsd_req_ctxt_t reqc)
{
	json_entity_t spec, mode, value;
	json_str_t mode_s;
	int rc = 0;

	spec = json_value_find(reqc->json, "spec");
	mode = json_value_find(spec, "mode");
	if (!mode) {
		rc = ldmsd_send_missing_attr_err(reqc, "greeting", "mode");
		return rc;
	}
	mode_s = json_value_str(mode);

	value = json_value_find(spec, "value");
	if (0 == strncmp(mode_s->str, "echo", mode_s->str_len)) {
		if (!value) {
			rc = ldmsd_send_missing_attr_err(reqc, "greeting/echo", "value");
			goto out;
		}
		rc = __greeting_echo_handler(reqc, value);
	} else if (0 == strncmp(mode_s->str, "rsp_multi_rec", mode_s->str_len)) {
		if (!value) {
			rc = ldmsd_send_missing_attr_err(reqc, "greeting/rsp_multi_rec", "value");
			goto out;
		}
		rc = __greeting_rsp_multi_rec_handler(reqc, value);
	} else if (0 == strncmp(mode_s->str, "ping_pong", mode_s->str_len)) {
		if (!value) {
			rc = ldmsd_send_missing_attr_err(reqc, "greeting/ping_pong", "value");
			goto out;
		}
		rc = __greeting_ping_pong_handler(reqc, value);
	} else if (0 == strncmp(mode_s->str, "long_rsp", mode_s->str_len)) {
		if (!value) {
			rc = ldmsd_send_missing_attr_err(reqc, "greeting/long_rsp", "value");
			goto out;
		}
		rc = __greeting_long_rsp_handler(reqc, value);
	} else {
		rc = ldmsd_send_error(reqc, ENOTSUP, "Not supported mode %s",
								mode_s->str);
	}
out:
	return rc;
}

//
//static int unimplemented_handler(ldmsd_req_ctxt_t reqc)
//{
//	reqc->errcode = ENOSYS;
//
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"The request is not implemented");
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//
//static int eperm_handler(ldmsd_req_ctxt_t reqc)
//{
//	reqc->errcode = EPERM;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Operation not permitted.");
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//
//static int ebusy_handler(ldmsd_req_ctxt_t reqc)
//{
//	reqc->errcode = EBUSY;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			"Daemon busy.");
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}

int ldmsd_set_route_request(ldmsd_prdcr_t prdcr,
			ldmsd_req_ctxt_t org_reqc, char *inst_name,
			ldmsd_req_resp_fn resp_handler, void *ctxt)
{
	size_t inst_name_len;
	ldmsd_req_ctxt_t reqc;
	ldmsd_cfg_xprt_t xprt;
	struct ldmsd_msg_key key;
	char *str;
	int rc;

	xprt = ldmsd_cfg_xprt_ldms_new(prdcr->xprt);

	ldmsd_msg_key_get(xprt, &key);

	inst_name_len = strlen(inst_name);
	reqc = ldmsd_req_ctxt_alloc(&key, xprt);
	if (!reqc)
		return ENOMEM;
	reqc->resp_handler = resp_handler;
	reqc->ctxt = ctxt;

	str = 	"{\"cmd\": \"set_route\","
		" \"spec\": {\"instance\":\"";

	rc = ldmsd_append_request(reqc, LDMSD_REC_SOM_F, str, strlen(str));
	if (rc)
		goto out;

	/* append the instance name to the JSON string */
	rc = ldmsd_append_request(reqc, 0, inst_name, inst_name_len);
	if (rc)
		goto out;

	str =	"\","
		" \"type\":\"internal\"}"
		"}";
	rc = ldmsd_append_request(reqc, 0, str, strlen(str));
out:
	if (rc) {
		/* rc is not zero only if sending fails (a transport error) so
		 * no need to keep the request command context around */
		ldmsd_req_ctxt_free(reqc);
	}

	return rc;
}

ldmsd_req_buf_t __set_route_json_get(int is_internal, ldmsd_set_info_t info)
{
	ldmsd_req_buf_t buf;
	size_t cnt;

	buf = calloc(1, sizeof(*buf) + 1024);
	if (!buf)
		return NULL;
	buf->len = 1024;
	buf->buf[0] = '\0';

	if (info->origin_type == LDMSD_SET_ORIGIN_SMPLR) {
	smplr:
		cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off,
				"{"
				"\"host\":\"%s\","
				"\"type\":\"%s\","
				"\"detail\":"
					"{"
					"\"name\":\"%s\","
					"\"interval_us\":\"%lu\","
					"\"offset_us\":\"%ld\","
					"\"sync\":\"%s\","
					"\"trans_start_sec\":\"%ld\","
					"\"trans_start_usec\":\"%ld\","
					"\"trans_end_sec\":\"%ld\","
					"\"trans_end_usec\":\"%ld\""
					"}"
				"}",
				ldmsd_myhostname_get(),
				ldmsd_set_info_origin_enum2str(info->origin_type),
				info->origin_name,
				info->interval_us,
				info->offset_us,
				((info->sync)?"true":"false"),
				info->start.tv_sec,
				info->start.tv_usec,
				info->end.tv_sec,
				info->end.tv_usec);
		if (cnt > buf->len - buf->off) {
			buf = ldmsd_req_buf_realloc(buf, buf->len * 2);
			if (!buf)
				return NULL;
			goto smplr;
		}
		buf->off += cnt;
		if (!is_internal) {
			cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off, "]}");
			if (cnt > buf->len - buf->off) {
				buf = ldmsd_req_buf_realloc(buf, buf->len * 2);
				if (!buf)
					return NULL;
				cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off, "]}");
			}
			buf->off += cnt;
		}
	} else {
	prdcr_set:
		cnt = snprintf(&buf->buf[buf->off], buf->len - buf->off,
				"{"
				"\"host\":\"%s\","
				"\"type\":\"%s\","
				"\"detail\":"
					"{"
					"\"name\":\"%s\","
					"\"host\":\"%s\","
					"\"update_int\":\"%ld\","
					"\"update_off\":\"%ld\","
					"\"update_sync\":\"%s\","
					"\"last_start_sec\":\"%ld\","
					"\"last_start_usec\":\"%ld\","
					"\"last_end_sec\":\"%ld\","
					"\"last_end_usec\":\"%ld\""
					"}"
				"}",
				ldmsd_myhostname_get(),
				ldmsd_set_info_origin_enum2str(info->origin_type),
				info->origin_name,
				info->prd_set->prdcr->host_name,
				info->interval_us,
				info->offset_us,
				((info->sync)?"true":"false"),
				info->start.tv_sec,
				info->start.tv_usec,
				info->end.tv_sec,
				info->end.tv_usec);
		if (cnt > buf->len - buf->off) {
			buf = ldmsd_req_buf_realloc(buf, buf->len * 2);
			if (!buf)
				return NULL;
			goto prdcr_set;
		}
		buf->off += cnt;
	}
	return buf;
}

typedef struct set_route_ctxt {
	ldmsd_req_buf_t my_set;
	int is_internal;
	const char *inst_name;
	const char *schema_name;
	/*
	 * The request context of the command
	 * that LDMSD has received from a client.
	 */
	ldmsd_req_ctxt_t cmd_reqc;
} *set_route_ctxt_t;

static void free_set_route_ctxt(set_route_ctxt_t ctxt)
{
	if (ctxt->inst_name)
		free((char *)ctxt->inst_name);
	if (ctxt->schema_name)
		free((char *)ctxt->schema_name);
	if (ctxt->my_set)
		ldmsd_req_buf_free(ctxt->my_set);
	if (ctxt->cmd_reqc)
		ldmsd_req_ctxt_ref_put(ctxt->cmd_reqc, "set_route_ctxt:ref");
	free(ctxt);
}

static int __send_set_route_rsp_obj(ldmsd_req_ctxt_t reqc,
		const char *inst_name, const char *schema_name,
		ldmsd_req_buf_t my_set, json_entity_t downstream)
{
	int rc = 0;
	char *str;
	json_entity_t hop;
	jbuf_t jb;

	str = "{\"type\":\"info\",\"name\":\"set_route\",\"info\":{\")";
	if ((rc = ldmsd_append_response(reqc, LDMSD_REC_SOM_F, str, strlen(str))))
		goto out;

	str = "{\"instance\":\"";
	if ((rc = ldmsd_append_response(reqc, 0, str, strlen(str))))
		goto out;
	if ((rc = ldmsd_append_response(reqc, 0, inst_name, strlen(inst_name))))
		goto out;
	str = "\",\"schema\":\"";
	if ((rc = ldmsd_append_response(reqc, 0, str, strlen(str))))
		goto out;
	if ((rc = ldmsd_append_response(reqc, 0, schema_name, strlen(schema_name))))
		goto out;
	str = "\",\"route\":[";
	if ((rc = ldmsd_append_response(reqc, 0, str, strlen(str))))
		goto out;
	if ((rc = ldmsd_append_response(reqc, 0, my_set->buf, my_set->off)))
		goto out;
	if (!downstream)
		goto end;
	for (hop = json_item_first(downstream); hop; hop = json_item_next(hop)) {
		if ((rc = ldmsd_append_response(reqc, 0, ",", 1)))
			goto out;
		jb = json_entity_dump(jb, hop);
		if (!jb) {
			rc = ENOMEM;
			ldmsd_log(LDMSD_LCRITICAL, "ldmsd: out of memory\n");
			rc = ldmsd_send_error(reqc, ENOMEM, "ldmsd: out of memory");
			if (rc)
				goto out;
		}
		if ((rc = ldmsd_append_response(reqc, 0, jb->buf, strlen(jb->buf))))
			goto out;
		jbuf_free(jb);
	}
end:
	str = "]}}";
	rc = ldmsd_append_response(reqc, LDMSD_REC_EOM_F, str, strlen(str));
out:
	return rc;
}

static int set_route_resp_handler(ldmsd_req_ctxt_t reqc)
{
	int rc;
	json_entity_t route, spec;
	set_route_ctxt_t ctxt = (set_route_ctxt_t)(reqc->ctxt);
	ldmsd_req_ctxt_t recv_reqc = ctxt->cmd_reqc;

	spec = json_value_find(reqc->json, "spec");
	route = json_value_find(spec, "route");
	if (!route) {
		if ((rc = ldmsd_send_error(recv_reqc, EINTR,
				"ldmsd: internal error when "
				"handling the set_route request")))
			goto out;
	}

	rc = __send_set_route_rsp_obj(recv_reqc, ctxt->inst_name,
			ctxt->schema_name, ctxt->my_set, route);
out:
	free_set_route_ctxt(ctxt);
	ldmsd_req_ctxt_free(reqc);
	return 0;
}

static int set_route_handler(ldmsd_req_ctxt_t reqc)
{
	char *inst_name;
	struct set_route_ctxt *ctxt = NULL;
	int is_internal = 0;
	int rc = 0;
	ldmsd_set_info_t info = NULL;
	ldmsd_req_buf_t buf;
	json_entity_t spec, name, type;

	spec = json_value_find(reqc->json, "spec");
	name = json_value_find(spec, "instance");
	if (!name) {
		return ldmsd_send_missing_attr_err(reqc,
					"set_route", "instance");
	}
	inst_name = json_value_str(name)->str;

	type = json_value_find(spec, "type");
	if (!type) {
		return ldmsd_send_missing_attr_err(reqc, "set_route", "type");
	}
	is_internal = json_value_bool(type);

	info = ldmsd_set_info_get(inst_name);
	if (!info) {
		rc = ldmsd_send_error(reqc, ENOENT,
				"%s: Set '%s' not exist.",
				ldmsd_myhostname_get(), inst_name);
		goto out;
	}

	buf = __set_route_json_get(is_internal, info);
	if (info->origin_type == LDMSD_SET_ORIGIN_PRDCR) {
		ctxt = calloc(1, sizeof(*ctxt));
		if (!ctxt)
			goto oom;
		ldmsd_req_ctxt_ref_get(reqc, "set_route_ctxt:ref"); /* put back when the ctxt is freed */
		ctxt->cmd_reqc = reqc;
		ctxt->inst_name = strdup(ldms_set_instance_name_get(info->set));
		if (!ctxt->inst_name)
			goto oom;
		ctxt->schema_name = strdup(ldms_set_schema_name_get(info->set));
		if (!ctxt->schema_name)
			goto oom;
		ctxt->is_internal = is_internal;
		ctxt->my_set = buf;
		rc = ldmsd_set_route_request(info->prd_set->prdcr,
				reqc, inst_name, set_route_resp_handler, ctxt);
		if (rc) {
			rc = ldmsd_send_error(reqc, rc,
					"%s: error forwarding set_route_request to "
					"prdcr '%s'", ldmsd_myhostname_get(),
					info->origin_name);
			goto err;
		}
	} else {
		rc = __send_set_route_rsp_obj(reqc, inst_name,
				ldms_set_schema_name_get(info->set),
				buf, NULL);
	}

	goto out;
oom:
	ldmsd_log(LDMSD_LCRITICAL, "ldmsd: out of memory\n");
err:
	if (ctxt)
		free_set_route_ctxt(ctxt);
out:
	if (info)
		ldmsd_set_info_delete(info);
	return rc;
}

//static int stream_publish_handler(ldmsd_req_ctxt_t reqc)
//{
//	char *stream_name;
//	ldmsd_stream_type_t stream_type = LDMSD_STREAM_STRING;
//	ldmsd_req_attr_t attr;
//	json_parser_t parser;
//	json_entity_t entity = NULL;
//
//	stream_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!stream_name) {
//		reqc->errcode = EINVAL;
//		ldmsd_log(LDMSD_LERROR, "%s: The stream name is missing "
//			  "in the config message\n", __func__);
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "The stream name is missing.");
//		goto err_reply;
//	}
//
//	/* Check for string */
//	attr = ldmsd_req_attr_get_by_id(reqc->req_buf, LDMSD_ATTR_STRING);
//	if (attr)
//		goto out;
//
//	/* Check for JSon */
//	attr = ldmsd_req_attr_get_by_id(reqc->req_buf, LDMSD_ATTR_JSON);
//	if (attr) {
//		parser = json_parser_new(0);
//		if (!parser) {
//			ldmsd_log(LDMSD_LERROR,
//				  "%s: error creating JSon parser.\n", __func__);
//			reqc->errcode = ENOMEM;
//			Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				       "Could not create the JSon parser.");
//			goto err_reply;
//		}
//		int rc = json_parse_buffer(parser,
//					   (char*)attr->attr_value, attr->attr_len,
//					   &entity);
//		json_parser_free(parser);
//		if (rc) {
//			ldmsd_log(LDMSD_LERROR,
//				  "%s: syntax error parsing JSon payload.\n", __func__);
//			reqc->errcode = EINVAL;
//			goto err_reply;
//		}
//		stream_type = LDMSD_STREAM_JSON;
//	} else {
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//				"No data provided.");
//		reqc->errcode = EINVAL;
//		goto err_reply;
//	}
//out:
//	ldmsd_stream_deliver(stream_name, stream_type,
//			     (char*)attr->attr_value, attr->attr_len, entity);
//	free(stream_name);
//	json_entity_free(entity);
//	reqc->errcode = 0;
//	ldmsd_send_req_response(reqc, NULL);
//	return 0;
//err_reply:
//	if (stream_name)
//		free(stream_name);
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//
//static int __on_republish_resp(ldmsd_req_cmd_t rcmd)
//{
//	return 0;
//}
//
//static int stream_republish_cb(ldmsd_stream_client_t c, void *ctxt,
//			       ldmsd_stream_type_t stream_type,
//			       const char *data, size_t data_len,
//			       json_entity_t entity)
//{
//	ldms_t ldms = ldms_xprt_get(ctxt);
//	int rc, attr_id = LDMSD_ATTR_STRING;
//	const char *stream = ldmsd_stream_client_name(c);
//	ldmsd_req_cmd_t rcmd = ldmsd_req_cmd_new(ldms, LDMSD_STREAM_PUBLISH_REQ,
//						 NULL, __on_republish_resp, NULL);
//	rc = ldmsd_req_cmd_attr_append_str(rcmd, LDMSD_ATTR_NAME, stream);
//	if (rc)
//		goto out;
//	if (stream_type == LDMSD_STREAM_JSON)
//		attr_id = LDMSD_ATTR_JSON;
//	rc = ldmsd_req_cmd_attr_append_str(rcmd, attr_id, data);
//	if (rc)
//		goto out;
//	rc = ldmsd_req_cmd_attr_term(rcmd);
// out:
//	return rc;
//}
//
//static int stream_subscribe_handler(ldmsd_req_ctxt_t reqc)
//
//{
//	char *stream_name;
//
//	stream_name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!stream_name) {
//		reqc->errcode = EINVAL;
//		Snprintf(&reqc->recv_buf, &reqc->recv_len,
//			       "The stream name is missing.");
//		goto send_reply;
//	}
//
//	ldmsd_stream_subscribe(stream_name, stream_republish_cb, reqc->xprt->ldms.ldms);
//	reqc->errcode = 0;
//	Snprintf(&reqc->recv_buf, &reqc->recv_len, "OK");
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//

char __get_opt(char *name)
{
	if (0 == strncmp("banner", name, strlen(name)))
		return 'B';
	else if (0 == strncmp("daemon-name", name, strlen(name)))
		return 'n';
	else if (0 == strncmp("default-auth:plugin", name, strlen(name)))
		return 'a';
	else if (0 == strncmp("default-auth:args", name, strlen(name)))
		return 'A';
	else if (0 == strncmp("hostname", name, strlen(name)))
		return 'H';
	else if (0 == strncmp("log:dst", name, strlen(name)))
		return 'l';
	else if (0 ==strncmp("log:level", name, strlen(name)))
		return 'v';
	else if (0 == strncmp("pidfile", name, strlen(name)))
		return 'r';
	else if (0 == strncmp("kernel:publish", name, strlen(name)))
		return 'k';
	else if (0 == strncmp("kernel:file", name, strlen(name)))
		return 's';
	else if (0 == strncmp("mem", name, strlen(name)))
		return 'm';
	else if (0 == strncmp("workers", name, strlen(name)))
		return 'P';
	else {
		return '-';
	}
}

static inline int __daemon_type_error(ldmsd_req_ctxt_t reqc,
					const char *name, const char *type)
{
	char n[512];
	snprintf(n, 512, "daemon:%s", name);
	return ldmsd_send_type_error(reqc, n, type);
}

static inline int __daemon_proc_error(ldmsd_req_ctxt_t reqc,
					const char *name, int errcode)
{
	return ldmsd_send_error(reqc, errcode, "daemon:%s caused an error.", name);
}

static int __daemon_default_auth(ldmsd_req_ctxt_t reqc, json_entity_t auth)
{
	char *name;
	json_entity_t value, attr;
	json_str_t n, av;
	char *v;
	char tmp[512];
	int rc;

	if (JSON_DICT_VALUE != json_entity_type(auth))
		return __daemon_type_error(reqc, "default-auth", "a dictionary");

	/* default-auth:plugin */
	name = "default-auth:plugin";
	value = json_value_find(auth, "plugin");
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);

		/* default-auth:args */
		name = "default-auth:args";
		value = json_value_find(auth, "args");
		if (!value)
			goto out;

		if (JSON_DICT_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a dictionary");
		for (attr = json_attr_first(value); attr; attr = json_attr_next(attr)) {
			n = json_attr_name(attr);
			value = json_attr_value(attr);
			if (JSON_STRING_VALUE != json_entity_type(value)) {
				snprintf(tmp, 512, "%s:%s", name, n->str);
				return __daemon_type_error(reqc, tmp, "a string");
			}
			av = json_value_str(value);

			snprintf(tmp, 512, "%s=%s", n->str, av->str);
			rc = ldmsd_process_cmd_line_arg(__get_opt(name), tmp);
			if (rc) {
				snprintf(tmp, 512, "%s:%s=%s", name, n->str, av->str);
				return __daemon_proc_error(reqc, tmp, rc);
			}
		}
	}
out:
	return 0;
}

static int __daemon_log(ldmsd_req_ctxt_t reqc, json_entity_t dict)
{
	json_entity_t value;
	char *name, *v;
	int rc;

	if (JSON_DICT_VALUE != json_entity_type(dict))
		return __daemon_type_error(reqc, "log", "a dictionary");

	/* log:dst */
	name = "log:dst";
	value = json_value_find(dict, "dst");
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");

		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* log:level */
	name = "log:level";
	value = json_value_find(dict, "level");
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}
	return 0;
}

static int __daemon_kernel(ldmsd_req_ctxt_t reqc, json_entity_t kernel)
{
	json_entity_t value;
	char *v, *name;
	int rc;

	if (JSON_DICT_VALUE != json_entity_type(kernel))
		return __daemon_type_error(reqc, "kernel", "a dictionary");

	/* kernel:publish */
	name = "kernek:publish";
	value = json_value_find(kernel, name);
	if (value) {
		if (JSON_BOOL_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "true/false");
		if (!json_value_bool(value))
			goto out;

		rc = ldmsd_process_cmd_line_arg(__get_opt(name), "true");
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
		/* kernel:file */
		name = "kernel:file";
		value = json_value_find(kernel, "file");
		if (value) {
			if (JSON_STRING_VALUE != json_entity_type(value))
				return __daemon_type_error(reqc, name, "a string");
			v = json_value_str(value)->str;
			rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
			if (rc)
				return __daemon_proc_error(reqc, name, rc);
		}
	}
out:
	return 0;
}

static int daemon_handler(ldmsd_req_ctxt_t reqc)
{
	json_entity_t spec, value;
	char *v, *name, *type;
	char buf[512];
	int rc = 0;

	if (ldmsd_is_initialized()) {
		/*
		 * No changes to command-line options are allowed
		 * after LDMSD is initialized.
		 *
		 * The only exception is loglevel which can be changed
		 * using loglevel command.
		 *
		 */
		rc = ldmsd_send_error(reqc, EPERM, "LDMSD is already initialized."
				"The daemon configuration cannot be altered.");
		return rc;
	}

	spec = json_value_find(reqc->json, "spec");

	/* log */
	name = "log";
	value = json_value_find(spec, name);
	if (value) {
		rc = __daemon_log(reqc, value);
		if (rc)
			return rc;
	}

	/* Default authentication */
	name = "default-auth";
	value = json_value_find(spec, name);
	if (value) {
		rc = __daemon_default_auth(reqc, value);
		if (rc)
			return rc;
	}

	/* memory */
	name = "mem";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* workers */
	name = "workers";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_INT_VALUE == json_entity_type(value)) {
			snprintf(buf, 512, "%" PRIu64, json_value_int(value));
			v = buf;
		} else if (JSON_STRING_VALUE != json_entity_type(value)) {
			type = "a string or an integer";
			return __daemon_type_error(reqc, name, type);
		} else {
			v = json_value_str(value)->str;
		}
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* pidfile */
	name = "pidfile";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* banner */
	name = "banner";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_BOOL_VALUE == json_entity_type(value)) {
			v = json_value_bool(value)?"1":"0";
		} else if (JSON_INT_VALUE == json_entity_type(value)) {
			v = json_value_int(value)?"1":"0";
		} else {
			return __daemon_type_error(reqc, name, "true/false or 0/1");
		}
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* hostname */
	name = "hostname";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* daemon-name */
	name = "daemon-name";
	value = json_value_find(spec, name);
	if (value) {
		if (JSON_STRING_VALUE != json_entity_type(value))
			return __daemon_type_error(reqc, name, "a string");
		v = json_value_str(value)->str;
		rc = ldmsd_process_cmd_line_arg(__get_opt(name), v);
		if (rc)
			return __daemon_proc_error(reqc, name, rc);
	}

	/* kernel */
	value = json_value_find(spec, "kernel");
	if (value) {
		rc = __daemon_kernel(reqc, value);
		if (rc)
			return rc;
	}

	return 0;
}

static int listen_handler(ldmsd_req_ctxt_t reqc)
{
	ldmsd_listen_t listen;
	int rc = 0;
	json_entity_t xprt, port, host, auth, spec;
	char *xprt_s, *host_s, *port_s, *auth_s;
	unsigned short port_no = -1;
	xprt_s = port_s = host_s = auth_s = NULL;

	if (ldmsd_is_initialized()) {
		/*
		 * Adding a new listening endpoint is prohibited
		 * after LDMSD is initialized.
		 */
		rc = ldmsd_send_error(reqc, EPERM, "LDMSD has been started. "
				"Adding a listening endpoint is prohibited.");
		goto out;
	}

	spec = json_value_find(reqc->json, "spec");

	xprt = json_value_find(spec, "xprt");
	if (!xprt) {
		rc = ldmsd_send_missing_attr_err(reqc, "listen", "xprt");
		goto out;
	}
	if (JSON_STRING_VALUE != json_entity_type(xprt)) {
		rc = ldmsd_send_error(reqc, EINVAL, "listen:xprt must be a string.");
		goto out;
	}
	xprt_s = json_value_str(xprt)->str;

	port = json_value_find(spec, "port");
	if (port) {
		if (JSON_STRING_VALUE == json_entity_type(port)) {
			port_s = json_value_str(port)->str;
			port_no = atoi(port_s);
		} else if (JSON_INT_VALUE == json_entity_type(port)) {
			port_no = (int)json_value_int(port);
		} else {
			rc = ldmsd_send_error(reqc, EINVAL,
				"listen:port must be either 'init' or 'string'.");
			goto out;
		}

		if (port_no < 1 || port_no > USHRT_MAX) {
			rc = ldmsd_send_error(reqc, EINVAL,
					"'%s' transport with invalid port '%s'",
					xprt, port);
			goto out;
		}
	}
	host = json_value_find(spec, "host");
	if (host) {
		if (JSON_STRING_VALUE != json_entity_type(host)) {
			rc = ldmsd_send_error(reqc, EINVAL, "listen:host must be a string.");
			goto out;
		}
		host_s = json_value_str(host)->str;
	}
	auth = json_value_find(spec, "auth");
	if (auth) {
		if (JSON_STRING_VALUE != json_entity_type(auth)) {
			rc = ldmsd_send_error(reqc, EINVAL, "listen:auth must be a string.");
			goto out;
		}
		auth_s = json_value_str(auth)->str;
	}

	listen = ldmsd_listen_new(xprt_s, port_no, host_s, auth_s);
	if (!listen) {
		if (errno == EEXIST) {
			rc = ldmsd_send_error(reqc, EEXIST,
					"The listening endpoint %s:%s is already exists",
					xprt_s, port_s);
		} else if (errno == ENOENT) {
			rc = ldmsd_send_error(reqc, ENOENT,
					"The given 'auth' (%s) "
					"does not exist.", auth_s);
		} else {
			ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
		}
	}
out:
	return rc;
}
//
//struct envvar_name {
//	const char *env;
//	uint8_t is_exported;
//};
//
//static void __print_env(FILE *f, const char *env, struct rbt *exported_env_tree)
//{
//	int rc = env_node_new(env, exported_env_tree, NULL);
//	if (rc == EEXIST) {
//		/* The environment variable was already exported */
//		return;
//	}
//
//	char *v = getenv(env);
//	if (v)
//		fprintf(f, "env %s=%s\n", env, v);
//}
//
//struct env_trav_ctxt {
//	FILE *f;
//	struct rbt *exported_env_tree;;
//};
//
//static int __export_env(struct rbn *rbn, void *ctxt, int i)
//{
//	struct env_trav_ctxt *_ctxt = (struct env_trav_ctxt *)ctxt;
//	__print_env(_ctxt->f, (const char *)rbn->key, _ctxt->exported_env_tree);
//	return i;
//}
//
//static int __export_envs(ldmsd_req_ctxt_t reqc, FILE *f)
//{
//	int rc = 0;
//	struct rbt exported_env_tree = RBT_INITIALIZER(env_cmp);
//
//	char *ldmsd_envvar_tbl[] = {
//		"LDMS_AUTH_FILE",
//		"LDMSD_MEM_SIZE_ENV",
//		"LDMSD_PIDFILE",
//		"LDMSD_PLUGIN_LIBPATH",
//		"LDMSD_UPDTR_OFFSET_INCR",
//		"MMALLOC_DISABLE_MM_FREE",
//		"OVIS_EVENT_HEAP_SIZE",
//		"OVIS_NOTIFICATION_RETRY",
//		"ZAP_EVENT_WORKERS",
//		"ZAP_EVENT_QDEPTH",
//		"ZAP_LIBPATH",
//		NULL,
//	};
//
//	struct env_trav_ctxt ctxt = { f, &exported_env_tree };
//
//	fprintf(f, "# ---------- Environment Variables ----------\n");
//
//	/*
//	 *  Export all environment variables set with the env command.
//	 */
//	pthread_mutex_lock(&env_tree_lock);
//	rbt_traverse(&env_tree, __export_env, (void *)&ctxt);
//	pthread_mutex_unlock(&env_tree_lock);
//
//
//	/*
//	 * Export environment variables used in the LDMSD process that are
//	 * set directly in bash.
//	 */
//	/*
//	 * Environment variables used by core LDMSD (not plugins).
//	 */
//	int i;
//	for (i = 0; ldmsd_envvar_tbl[i]; i++) {
//		__print_env(f, ldmsd_envvar_tbl[i], &exported_env_tree);
//	}
//
//	/*
//	 * Environment variables used by zap transports
//	 */
//	char **zap_envs = ldms_xprt_zap_envvar_get();
//	if (!zap_envs) {
//		if (errno) {
//			rc = errno;
//			(void) linebuf_printf(reqc, "Failed to get zap "
//					"environment variables. Error %d\n", rc);
//			goto cleanup;
//		} else {
//			/*
//			 * nothing to do
//			 * no env var used by the loaded zap transports
//			 */
//		}
//	} else {
//		int i;
//		for (i = 0; zap_envs[i]; i++) {
//			__print_env(f, zap_envs[i], &exported_env_tree);
//			free(zap_envs[i]);
//		}
//		free(zap_envs);
//	}
//
//	/*
//	 * Environment variables used by loaded plugins.
//	 */
//	ldmsd_plugin_inst_t inst;
//	json_entity_t qr, env_attr, envs, item;
//	char *name;
//	LDMSD_PLUGIN_INST_FOREACH(inst) {
//		qr = inst->base->query(inst, "env"); /* Query env used by the pi instance */
//		if (!qr)
//			continue;
//		env_attr = json_attr_find(qr, "env");
//		if (!env_attr)
//			goto next;
//		envs = json_attr_value(env_attr);
//		if (JSON_LIST_VALUE != json_entity_type(envs)) {
//			(void) linebuf_printf(reqc, "Cannot get the environment "
//					"variable list from plugin instance '%s'",
//					inst->inst_name);
//			rc = EINTR;
//			goto cleanup;
//		}
//		for (item = json_item_first(envs); item; item = json_item_next(item)) {
//			name = json_value_str(item)->str;
//			__print_env(f, name, &exported_env_tree);
//		}
//	next:
//		json_entity_free(qr);
//	}
//
//	/*
//	 * Clean up the exported_env_tree;
//	 */
//	struct env_node *env_node;
//	struct rbn *rbn;
//cleanup:
//	rbn = rbt_min(&exported_env_tree);
//	while (rbn) {
//		rbt_del(&exported_env_tree, rbn);
//		env_node = container_of(rbn, struct env_node, rbn);
//		env_node_del(env_node);
//		rbn = rbt_min(&exported_env_tree);
//	}
//	return rc;
//}
//
//extern struct ldmsd_cmd_line_args cmd_line_args;
//static void __export_cmdline_args(FILE *f)
//{
//	int i = 0;
//	ldmsd_listen_t listen;
//
//	fprintf(f, "# ---------- CMD-line Options ----------\n");
//	/* xprt */
//	for (listen = (ldmsd_listen_t)ldmsd_cfgobj_first(LDMSD_CFGOBJ_LISTEN);
//			listen ; listen = (ldmsd_listen_t)ldmsd_cfgobj_next(&listen->obj)) {
//		fprintf(f, "listen xprt=%s port=%hu", listen->xprt, listen->port_no);
//		if (listen->host) {
//			fprintf(f, " host=%s", listen->host);
//		}
//		if (listen->auth_name) {
//			fprintf(f, " auth=%s", listen->auth_name);
//			if (!listen->auth_attrs)
//				goto next_lend;
//			for (i = 0; i < listen->auth_attrs->count; i++) {
//				fprintf(f, " %s=%s",
//					listen->auth_attrs->list[i].name,
//					listen->auth_attrs->list[i].value);
//			}
//		}
//	next_lend:
//		fprintf(f, "\n");
//	}
//
//	if (cmd_line_args.log_path) {
//		fprintf(f, "set %s=%s %s=%s\n",
//				opts['l'].l,
//				cmd_line_args.log_path,
//				opts['v'].l,
//				ldmsd_loglevel_to_str(cmd_line_args.verbosity));
//	} else {
//		fprintf(f, "set %s=%s\n", opts['v'].l,
//				ldmsd_loglevel_to_str(cmd_line_args.verbosity));
//	}
//	fprintf(f, "set %s=%s\n", opts['m'].l, cmd_line_args.mem_sz_str);
//	fprintf(f, "set %s=%d\n", opts['P'].l, cmd_line_args.ev_thread_count);
//
//	/* authentication */
//	if (cmd_line_args.auth_name) {
//		fprintf(f, "set %s=%s", opts['a'].l, cmd_line_args.auth_name);
//		if (cmd_line_args.auth_attrs) {
//			for (i = 0; i < cmd_line_args.auth_attrs->count; i++) {
//				fprintf(f, " %s=%s",
//					cmd_line_args.auth_attrs->list[i].name,
//					cmd_line_args.auth_attrs->list[i].value);
//			}
//		}
//		fprintf(f, "\n");
//	}
//	fprintf(f, "set %s=%d\n", opts['B'].l, cmd_line_args.banner);
//	if (cmd_line_args.pidfile)
//		fprintf(f, "set %s=%s\n", opts['r'].l, cmd_line_args.pidfile);
//	fprintf(f, "set %s=%s\n", opts['H'].l, cmd_line_args.myhostname);
//	fprintf(f, "set %s=%s\n", opts['n'].l, cmd_line_args.daemon_name);
//
//	/* kernel options */
//	if (cmd_line_args.do_kernel) {
//		fprintf(f, "set %s=true\n", opts['k'].l);
//		fprintf(f, "set %s=%s\n", opts['s'].l,
//				cmd_line_args.kernel_setfile);
//	}
//}
//
//static int __export_smplr_config(FILE *f)
//{
//	fprintf(f, "# ----- Sampler Policies -----\n");
//	ldmsd_smplr_t smplr;
//	ldmsd_cfg_lock(LDMSD_CFGOBJ_SMPLR);
//	for (smplr = ldmsd_smplr_first(); smplr; smplr = ldmsd_smplr_next(smplr)) {
//		ldmsd_smplr_lock(smplr);
//		fprintf(f, "smplr_add name=%s instance=%s",
//				smplr->obj.name, smplr->pi->inst_name);
//		fprintf(f, " interval=%ld", smplr->interval_us);
//		if (smplr->offset_us != LDMSD_UPDT_HINT_OFFSET_NONE)
//			fprintf(f, " offset=%ld", smplr->offset_us);
//		fprintf(f, "\n");
//		if (smplr->state == LDMSD_SMPLR_STATE_RUNNING)
//			fprintf(f, "smplr_start name=%s\n", smplr->obj.name);
//		ldmsd_smplr_unlock(smplr);
//	}
//	ldmsd_cfg_unlock(LDMSD_CFGOBJ_SMPLR);
//	return 0;
//}
//
//static int __export_prdcrs_config(FILE *f)
//{
//	int rc = 0;
//	fprintf(f, "# ----- Producer Policies -----\n");
//	ldmsd_prdcr_t prdcr;
//	ldmsd_cfg_lock(LDMSD_CFGOBJ_PRDCR);
//	for (prdcr = ldmsd_prdcr_first(); prdcr; prdcr = ldmsd_prdcr_next(prdcr)) {
//		ldmsd_prdcr_lock(prdcr);
//		fprintf(f, "prdcr_add name=%s type=%s host=%s port=%hu xprt=%s interval=%ld",
//				prdcr->obj.name,
//				ldmsd_prdcr_type2str(prdcr->type),
//				prdcr->host_name,
//				prdcr->port_no,
//				prdcr->xprt_name,
//				prdcr->conn_intrvl_us);
//		if (prdcr->conn_auth) {
//			fprintf(f, " auth=%s", prdcr->conn_auth);
//			if (prdcr->conn_auth_args) {
//				char *s = av_to_string(prdcr->conn_auth_args, 0);
//				if (!s) {
//					ldmsd_prdcr_unlock(prdcr);
//					rc =  ENOMEM;
//					goto out;
//				}
//				fprintf(f, " %s", s);
//				free(s);
//			}
//		}
//		fprintf(f, "\n");
//		if ((prdcr->conn_state != LDMSD_PRDCR_STATE_STOPPED) &&
//				(prdcr->conn_state != LDMSD_PRDCR_STATE_STOPPING)) {
//			fprintf(f, "prdcr_start name=%s\n", prdcr->obj.name);
//		}
//		ldmsd_prdcr_unlock(prdcr);
//	}
//out:
//	ldmsd_cfg_unlock(LDMSD_CFGOBJ_PRDCR);
//	return rc;
//}
//
//static int __export_updtrs_config(FILE *f)
//{
//	ldmsd_updtr_t updtr;
//	ldmsd_str_ent_t regex_ent;
//	ldmsd_name_match_t match;
//
//	fprintf(f, "# ----- Updater Policies -----\n");
//	ldmsd_cfg_lock(LDMSD_CFGOBJ_UPDTR);
//	for (updtr = ldmsd_updtr_first(); updtr; updtr = ldmsd_updtr_next(updtr)) {
//		ldmsd_updtr_lock(updtr);
//		/* updtr_add */
//		fprintf(f, "updtr_add name=%s interval=%ld",
//				updtr->obj.name,
//				updtr->sched.intrvl_us);
//		if (updtr->sched.offset_us != LDMSD_UPDT_HINT_OFFSET_NONE) {
//			/* Specify offset */
//			fprintf(f, " offset=%ld", updtr->sched.offset_us);
//		}
//		if (updtr->is_auto_task) {
//			/* Specify auto_interval */
//			fprintf(f, " auto_interval=true");
//		}
//		fprintf(f, "\n");
//		/*
//		 * Both updtr_prdcr_add and updtr_prdcr_del lines are exported because
//		 * producers that are matched the regex's in the add_prdcr_regex_list
//		 * but not matched the regex's in the del_prdcr_regex_list
//		 * are those in prdcr_tree. LDMSD processes prdcr_add and then prdcr_del
//		 * is faster than LDMSD processes updtr_prdcr_add line-by-line
//		 * for each prdcr in prdcr_tree.
//		 */
//
//		/* updtr_prdcr_add */
//		LIST_FOREACH(regex_ent, &updtr->added_prdcr_regex_list, entry) {
//			fprintf(f, "updtr_prdcr_add name=%s regex=%s\n",
//					updtr->obj.name,
//					regex_ent->str);
//		}
//
//		/* updtr_prdcr_del */
//		LIST_FOREACH(regex_ent, &updtr->del_prdcr_regex_list, entry) {
//			fprintf(f, "updtr_prdcr_del name=%s regex=%s\n",
//					updtr->obj.name, regex_ent->str);
//		}
//
//		/* updtr_match_add */
//		for (match = ldmsd_updtr_match_first(updtr); match;
//				match = ldmsd_updtr_match_next(match)) {
//			fprintf(f, "updtr_match_add name=%s regex=%s match=%s\n",
//					updtr->obj.name,
//					match->regex_str,
//					ldmsd_updtr_match_enum2str(match->selector));
//		}
//
//		/* updtr_start */
//		if (updtr->state == LDMSD_UPDTR_STATE_RUNNING)
//			fprintf(f, "updtr_start name=%s\n",
//					updtr->obj.name);
//		ldmsd_updtr_unlock(updtr);
//	}
//	ldmsd_cfg_unlock(LDMSD_CFGOBJ_UPDTR);
//	return 0;
//}
//
//static int __export_strgps_config(FILE *f)
//{
//	ldmsd_strgp_t strgp;
//	ldmsd_name_match_t match;
//	ldmsd_strgp_metric_t metric;
//
//	fprintf(f, "# ----- Storage Policies -----\n");
//	ldmsd_cfg_lock(LDMSD_CFGOBJ_STRGP);
//	for (strgp = ldmsd_strgp_first(); strgp; strgp = ldmsd_strgp_next(strgp)) {
//		ldmsd_strgp_lock(strgp);
//
//		/* strgp_add */
//		fprintf(f, "strgp_add name=%s container=%s schema=%s\n",
//				strgp->obj.name,
//				strgp->inst->inst_name,
//				strgp->schema);
//
//		/* strgp_prdcr_add */
//		LIST_FOREACH(match, &strgp->prdcr_list, entry) {
//			fprintf(f, "strgp_prdcr_add name=%s regex=%s\n",
//					strgp->obj.name,
//					match->regex_str);
//		}
//
//		/* strgp_metric_add */
//		TAILQ_FOREACH(metric, &strgp->metric_list, entry) {
//			fprintf(f, "strgp_metric_add name=%s metric=%s\n",
//					strgp->obj.name,
//					metric->name);
//		}
//
//		/* strgp_start */
//		if (strgp->state != LDMSD_STRGP_STATE_STOPPED) {
//			fprintf(f, "strgp_start name=%s\n", strgp->obj.name);
//		}
//		ldmsd_strgp_unlock(strgp);
//	}
//	ldmsd_cfg_unlock(LDMSD_CFGOBJ_STRGP);
//	return 0;
//}
//
//struct setgroup_ctxt {
//	FILE *f;
//	ldmsd_setgrp_t setgrp;
//	int cnt;
//};
//
//static int __export_setgroup_member(ldms_set_t set, const char *name, void *arg)
//{
//	struct setgroup_ctxt *ctxt = (struct setgroup_ctxt *)arg;
//	if (ctxt->cnt == 0) {
//		fprintf(ctxt->f, "setgroup_ins name=%s instance=%s",
//					ctxt->setgrp->obj.name, name);
//	} else {
//		fprintf(ctxt->f, ",%s", name);
//	}
//	ctxt->cnt++;
//	return 0;
//}
//
//static int __decimal_to_octal(int decimal)
//{
//	int octal = 0;
//	int i = 1;
//	while (0 != decimal) {
//		octal += (decimal % 8) * i;
//		decimal /= 8;
//		i *= 10;
//	}
//	return octal;
//}
//
//static int __export_setgroups_config(FILE *f)
//{
//	int rc = 0;
//	ldmsd_setgrp_t setgrp;
//	struct setgroup_ctxt ctxt = {f, 0, 0};
//	fprintf(f, "# ----- Setgroups -----\n");
//	ldmsd_cfg_lock(LDMSD_CFGOBJ_SETGRP);
//	for (setgrp = ldmsd_setgrp_first(); setgrp;
//			setgrp = ldmsd_setgrp_next(setgrp)) {
//		ldmsd_setgrp_lock(setgrp);
//		/* setrgroup_add */
//		fprintf(f, "setgroup_add name=%s producer=%s perm=0%d",
//					setgrp->obj.name, setgrp->producer,
//					__decimal_to_octal(setgrp->obj.perm));
//		if (setgrp->interval_us) {
//			fprintf(f, " interval=%ld", setgrp->interval_us);
//			if (setgrp->offset_us != LDMSD_UPDT_HINT_OFFSET_NONE)
//				fprintf(f, " offset=%ld", setgrp->offset_us);
//		}
//		fprintf(f, "\n");
//
//		/* setgroup_ins */
//		ctxt.setgrp = setgrp;
//		rc = ldmsd_group_iter(setgrp->set, __export_setgroup_member, &ctxt);
//		if (rc) {
//			ldmsd_setgrp_unlock(setgrp);
//			goto out;
//		}
//		fprintf(f, "\n"); /* newline of the setgroup_ins line */
//		ldmsd_setgrp_unlock(setgrp);
//	}
//out:
//	ldmsd_cfg_unlock(LDMSD_CFGOBJ_SETGRP);
//	return rc;
//}
//
//static int __export_plugin_config(FILE *f)
//{
//	ldmsd_plugin_inst_t inst;
//	json_entity_t json, cfg, l, d, a;
//	json = NULL;
//
//	fprintf(f, "# ----- Plugin Instances -----\n");
//	LDMSD_PLUGIN_INST_FOREACH(inst) {
//		fprintf(f, "load name=%s plugin=%s\n",
//				inst->inst_name,
//				inst->plugin_name);
//		json = inst->base->query(inst, "config");
//		if (!json || !(cfg = json_attr_find(json, "config"))) {
//			ldmsd_log(LDMSD_LERROR, "Failed to export the config of "
//					"plugin instance '%s'. "
//					"The config record cannot be founded.\n",
//					inst->inst_name);
//			goto next;
//		}
//		/*
//		 * Assume that \c json is a JSON dict that contains
//		 * the plugin instance configuration attributes.
//		 */
//		l = json_attr_value(cfg);
//		if (JSON_LIST_VALUE != json_entity_type(l)) {
//			ldmsd_log(LDMSD_LERROR, "Failed to export the config of "
//					"plugin instance '%s'. "
//					"LDMSD cannot intepret the query result.\n",
//					inst->inst_name);
//			goto next;
//		}
//		for (d = json_item_first(l); d; d = json_item_next(d)) {
//			fprintf(f, "config name=%s", inst->inst_name);
//			for (a = json_attr_first(d); a; a = json_attr_next(a)) {
//				fprintf(f, " %s=%s", json_attr_name(a)->str,
//						json_value_str(json_attr_value(a))->str);
//			}
//			/*
//			 * End of a config line.
//			 */
//			fprintf(f, "\n");
//		}
//next:
//		if (json) {
//			json_entity_free(json);
//			json = NULL;
//		}
//	}
//	return 0;
//}
//
//int failover_config_export(FILE *f);
//static int export_config_handler(ldmsd_req_ctxt_t reqc)
//{
//	int rc;
//	FILE *f = NULL;
//	ldmsd_req_attr_t attr_exist;
//	int mode = 0; /* 0x1 -- env, 0x10 -- cmdline, 0x100 -- cfgcmd */
//	char *path, *attr_name;
//	path = NULL;
//
//	path = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_PATH);
//	if (!path) {
//		attr_name = "path";
//		goto einval;
//	}
//	f = fopen(path, "w");
//	if (!f) {
//		reqc->errcode = errno;
//		(void)snprintf(reqc->recv_buf, reqc->recv_len,
//				"Failed to open the file: %s", path);
//		goto send_reply;
//	}
//
//	attr_exist = ldmsd_req_attr_get_by_id(reqc->req_buf, LDMSD_ATTR_ENV);
//	if (attr_exist)
//		mode = 0x1;
//	attr_exist = ldmsd_req_attr_get_by_id(reqc->req_buf, LDMSD_ATTR_CMDLINE);
//	if (attr_exist)
//		mode |= 0x10;
//	attr_exist = ldmsd_req_attr_get_by_id(reqc->req_buf, LDMSD_ATTR_CFGCMD);
//	if (attr_exist)
//		mode |= 0x100;
//
//	if (!mode || (mode & 0x1)) {
//		/* export environment variables */
//		__export_envs(reqc, f);
//	}
//	if (!mode || (mode & 0x10)) {
//		/* export command-line options */
//		__export_cmdline_args(f);
//	}
//	if (!mode || (mode & 0x100)) {
//		fprintf(f, "# ---------- CFG commands ----------\n");
//		rc = __export_plugin_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the plugin-related "
//					"config commands");
//			goto send_reply;
//		}
//		rc = __export_smplr_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void) snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the configuration "
//					"of sampler policies");
//			goto send_reply;
//		}
//		rc = __export_prdcrs_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the Producer-related "
//					"config commands");
//			goto send_reply;
//		}
//		rc = __export_updtrs_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the Updater-related "
//					"config commands");
//			goto send_reply;
//		}
//		rc = __export_strgps_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the Storage "
//					"policy-related config commands");
//			goto send_reply;
//		}
//		rc = __export_setgroups_config(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the setgroup-related "
//					"config commands");
//			goto send_reply;
//		}
//		rc = failover_config_export(f);
//		if (rc) {
//			reqc->errcode = rc;
//			(void)snprintf(reqc->recv_buf, reqc->recv_len,
//					"Failed to export the failover-related "
//					"config commands");
//			goto send_reply;
//		}
//	}
//	goto send_reply;
//einval:
//	reqc->errcode = EINVAL;
//	(void)linebuf_printf(reqc, "The attribute '%s' is required.", attr_name);
//send_reply:
//	if (f)
//		fclose(f);
//	if (path)
//		free(path);
//	ldmsd_send_req_response(reqc, reqc->recv_buf);
//	return 0;
//}
//
static int auth_handler(ldmsd_req_ctxt_t reqc)
{
	int rc = 0;
	struct attr_value_list *avl = NULL;
	json_entity_t name, plugin, args, arg, spec;
	json_str_t an, av;
	char *name_s = NULL, *plugin_s = NULL;
	ldmsd_auth_t auth_dom = NULL;

	spec = json_value_find(reqc->json, "spec");
	name = json_value_find(spec, "name");
	if (!name) {
		rc = ldmsd_send_missing_attr_err(reqc, "auth", "name");
		goto out;
	}
	if (JSON_STRING_VALUE != json_entity_type(name)) {
		rc = ldmsd_send_error(reqc, EINVAL, "auth:name must be a string.");
		goto out;
	}
	name_s = json_value_str(name)->str;

	plugin = json_value_find(spec, "plugin");
	if (!plugin) {
		rc = ldmsd_send_missing_attr_err(reqc, "auth", "plugin");
		goto out;
	}
	if (JSON_STRING_VALUE != json_entity_type(plugin)) {
		rc = ldmsd_send_error(reqc, EINVAL, "auth:plugin must be a string.");
		goto out;
	}
	plugin_s = json_value_str(plugin)->str;

	args = json_value_find(spec, "args");
	if (!args)
		goto new_auth;

	avl = av_new(json_attr_count(args));
	if (!avl) {
		ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
		rc = ENOMEM;
		goto out;
	}
	for (arg = json_attr_first(args); arg; arg = json_attr_next(arg)) {
		an = json_attr_name(arg);
		av = json_value_str(json_attr_value(arg));
		rc = av_add(avl, an->str, av->str);
		if (rc)
			goto out;
	}

new_auth:
	auth_dom = ldmsd_auth_new_with_auth(name_s, plugin_s, avl,
					    geteuid(), getegid(), 0600);
	if (!auth_dom) {
		rc = ldmsd_send_error(reqc, errno,
				"Authentication domain creation failed, "
				"errno: %d", errno);
	}

out:
	return rc;
}

//static int auth_del_handler(ldmsd_req_ctxt_t reqc)
//{
//	const char *attr_name;
//	char *name = NULL;
//	struct ldmsd_sec_ctxt sctxt;
//
//	name = ldmsd_req_attr_str_value_get_by_id(reqc, LDMSD_ATTR_NAME);
//	if (!name) {
//		attr_name = "name";
//		goto attr_required;
//	}
//
//	ldmsd_sec_ctxt_get(&sctxt);
//	reqc->errcode = ldmsd_auth_del(name, &sctxt);
//	switch (reqc->errcode) {
//	case EACCES:
//		snprintf(reqc->line_buf, reqc->line_len, "Permission denied");
//		break;
//	case ENOENT:
//		snprintf(reqc->line_buf, reqc->line_len,
//			 "'%s' authentication domain not found", name);
//		break;
//	default:
//		snprintf(reqc->line_buf, reqc->line_len,
//			 "Failed to delete authentication domain '%s', "
//			 "error: %d", name, reqc->errcode);
//		break;
//	}
//
//	goto send_reply;
//
//attr_required:
//	reqc->errcode = EINVAL;
//	(void) snprintf(reqc->line_buf, reqc->line_len,
//			"Attribute '%s' is required", attr_name);
//	goto send_reply;
//send_reply:
//	ldmsd_send_req_response(reqc, reqc->line_buf);
//	/* cleanup */
//	if (name)
//		free(name);
//	return 0;
//}
