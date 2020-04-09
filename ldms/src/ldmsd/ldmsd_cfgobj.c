/* -*- c-basic-offset: 8 -*-
 * Copyright (c) 2015,2018 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS). Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 * Copyright (c) 2015,2018 Open Grid Computing, Inc. All rights reserved.
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <coll/rbt.h>
#include "ldmsd.h"

int cfgobj_cmp(void *a, const void *b)
{
	return strcmp(a, b);
}

int ldmsd_cfgobj_access_check(ldmsd_cfgobj_t obj, int acc, ldmsd_sec_ctxt_t ctxt)
{
	return ovis_access_check(ctxt->crd.uid, ctxt->crd.gid, acc,
				 obj->uid, obj->gid, obj->perm);
}

struct rbt prdcr_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t prdcr_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt updtr_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t updtr_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt strgp_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t strgp_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt smplr_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t smplr_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt listen_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t listen_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt setgrp_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t setgrp_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt auth_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t auth_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt env_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t env_tree_lock = PTHREAD_MUTEX_INITIALIZER;

struct rbt daemon_tree = RBT_INITIALIZER(cfgobj_cmp);
pthread_mutex_t daemon_tree_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t *cfgobj_locks[] = {
	[LDMSD_CFGOBJ_PRDCR] = &prdcr_tree_lock,
	[LDMSD_CFGOBJ_UPDTR] = &updtr_tree_lock,
	[LDMSD_CFGOBJ_STRGP] = &strgp_tree_lock,
	[LDMSD_CFGOBJ_SMPLR] = &smplr_tree_lock,
	[LDMSD_CFGOBJ_LISTEN] = &listen_tree_lock,
	[LDMSD_CFGOBJ_SETGRP] = &setgrp_tree_lock,
	[LDMSD_CFGOBJ_AUTH]   = &auth_tree_lock,
	[LDMSD_CFGOBJ_ENV]   = &env_tree_lock,
	[LDMSD_CFGOBJ_DAEMON] = &daemon_tree_lock,
};

struct rbt *cfgobj_trees[] = {
	[LDMSD_CFGOBJ_PRDCR] = &prdcr_tree,
	[LDMSD_CFGOBJ_UPDTR] = &updtr_tree,
	[LDMSD_CFGOBJ_STRGP] = &strgp_tree,
	[LDMSD_CFGOBJ_SMPLR] = &smplr_tree,
	[LDMSD_CFGOBJ_LISTEN] = &listen_tree,
	[LDMSD_CFGOBJ_SETGRP] = &setgrp_tree,
	[LDMSD_CFGOBJ_AUTH]   = &auth_tree,
	[LDMSD_CFGOBJ_ENV]   = &env_tree,
	[LDMSD_CFGOBJ_DAEMON] = &daemon_tree,
};

void ldmsd_cfgobj_init(void)
{
	rbt_init(&prdcr_tree, cfgobj_cmp);
	rbt_init(&updtr_tree, cfgobj_cmp);
	rbt_init(&strgp_tree, cfgobj_cmp);
	rbt_init(&smplr_tree, cfgobj_cmp);
	rbt_init(&listen_tree, cfgobj_cmp);
	rbt_init(&setgrp_tree, cfgobj_cmp);
	rbt_init(&auth_tree,   cfgobj_cmp);
	rbt_init(&env_tree, cfgobj_cmp);
	rbt_init(&daemon_tree, cfgobj_cmp);
}

struct cfgobj_type_entry {
	const char *s;
	enum ldmsd_cfgobj_type e;
};

static int cfgobj_type_entry_comp(void *a, void *b)
{
	struct cfgobj_type_entry *a_, *b_;
	a_ = (struct cfgobj_type_entry *)a;
	b_ = (struct cfgobj_type_entry *)b;
	return strcmp(a_->s, b_->s);
}

const char *ldmsd_cfgobj_types[] = {
		[LDMSD_CFGOBJ_AUTH]	= "auth",
		[LDMSD_CFGOBJ_ENV]	= "env",
		[LDMSD_CFGOBJ_DAEMON]	= "daemon",
		NULL,
};

static struct cfgobj_type_entry cfgobj_type_tbl[] = {
		{ "auth",	LDMSD_CFGOBJ_AUTH },
		{ "env",	LDMSD_CFGOBJ_ENV },
		{ "daemon",	LDMSD_CFGOBJ_DAEMON },
		/* TODO: populate this table */
};

enum ldmsd_cfgobj_type ldmsd_cfgobj_type_str2enum(const char *s)
{
	struct cfgobj_type_entry *entry;

	entry = bsearch(s, cfgobj_type_tbl, ARRAY_SIZE(cfgobj_type_tbl),
				sizeof(*entry), cfgobj_type_entry_comp);
	if (!entry)
		return -1;
	return entry->e;
}

void ldmsd_cfgobj___del(ldmsd_cfgobj_t obj)
{
	free(obj->name);
	free(obj);
}

void ldmsd_cfg_lock(ldmsd_cfgobj_type_t type)
{
	pthread_mutex_lock(cfgobj_locks[type]);
}

void ldmsd_cfg_unlock(ldmsd_cfgobj_type_t type)
{
	pthread_mutex_unlock(cfgobj_locks[type]);
}

void ldmsd_cfgobj_lock(ldmsd_cfgobj_t obj)
{
	pthread_mutex_lock(&obj->lock);
}

void ldmsd_cfgobj_unlock(ldmsd_cfgobj_t obj)
{
	pthread_mutex_unlock(&obj->lock);
}

ldmsd_cfgobj_t ldmsd_cfgobj_new_with_auth(const char *name,
					  ldmsd_cfgobj_type_t type,
					  size_t obj_size,
					  ldmsd_cfgobj_del_fn_t __del,
					  ldmsd_cfgobj_update_fn_t update,
					  ldmsd_cfgobj_delete_fn_t delete,
					  ldmsd_cfgobj_query_fn_t query,
					  ldmsd_cfgobj_export_fn_t export,
					  uid_t uid,
					  gid_t gid,
					  int perm,
					  short enabled)
{
	ldmsd_cfgobj_t obj = NULL;

	pthread_mutex_lock(cfgobj_locks[type]);

	errno = EEXIST;
	struct rbn *n = rbt_find(cfgobj_trees[type], name);
	if (n)
		goto out_1;

	errno = ENOMEM;
	obj = calloc(1, obj_size);
	if (!obj)
		goto out_1;
	obj->name = strdup(name);
	if (!obj->name)
		goto out_2;

	obj->type = type;
	obj->ref_count = 1; /* for obj->rbn inserting into the tree */
	if (__del)
		obj->__del = __del;
	else
		obj->__del = ldmsd_cfgobj___del;
	obj->uid = uid;
	obj->gid = gid;
	obj->perm = perm;
	obj->enabled = enabled;

	pthread_mutex_init(&obj->lock, NULL);
	pthread_mutex_lock(&obj->lock);
	rbn_init(&obj->rbn, obj->name);
	rbt_ins(cfgobj_trees[type], &obj->rbn);
	goto out_1;

out_2:
	free(obj);
	obj = NULL;

out_1:
	pthread_mutex_unlock(cfgobj_locks[type]);
	return obj;
}

/**
 * Allocate a configuration object of the requested size. A
 * configuration object with the same name and type must not already
 * exist. On success, the object is returned locked.
 */
ldmsd_cfgobj_t ldmsd_cfgobj_new(const char *name, ldmsd_cfgobj_type_t type,
				size_t obj_size, ldmsd_cfgobj_del_fn_t __del)
{
	return ldmsd_cfgobj_new_with_auth(name, type, obj_size, __del,
					  getuid(), getgid(), 0777);
}

ldmsd_cfgobj_t ldmsd_cfgobj_get(ldmsd_cfgobj_t obj)
{
	if (obj)
		__sync_fetch_and_add(&obj->ref_count, 1);
	return obj;
}

void ldmsd_cfgobj_put(ldmsd_cfgobj_t obj)
{
	if (!obj)
		return;
	if (0 == __sync_sub_and_fetch(&obj->ref_count, 1))
		obj->__del(obj);
}

/** This function is only useful if the cfgobj lock is held when the function is called. */
int ldmsd_cfgobj_refcount(ldmsd_cfgobj_t obj)
{
	return obj->ref_count;
}

/*
 * *** Must be called with `cfgobj_locks[type]` held.
 */
ldmsd_cfgobj_t __cfgobj_find(const char *name, ldmsd_cfgobj_type_t type)
{
	ldmsd_cfgobj_t obj = NULL;
	struct rbn *n = rbt_find(cfgobj_trees[type], name);
	if (!n)
		goto out;
	obj = container_of(n, struct ldmsd_cfgobj, rbn);
out:
	return ldmsd_cfgobj_get(obj);
}

ldmsd_cfgobj_t ldmsd_cfgobj_find(const char *name, ldmsd_cfgobj_type_t type)
{
	ldmsd_cfgobj_t obj;
	pthread_mutex_lock(cfgobj_locks[type]);
	obj = __cfgobj_find(name, type);
	pthread_mutex_unlock(cfgobj_locks[type]);
	return obj;
}

void ldmsd_cfgobj_del(const char *name, ldmsd_cfgobj_type_t type)
{
	ldmsd_cfgobj_t obj;
	pthread_mutex_lock(cfgobj_locks[type]);
	obj = __cfgobj_find(name, type);
	if (obj)
		rbt_del(cfgobj_trees[type], &obj->rbn);
	pthread_mutex_unlock(cfgobj_locks[type]);
}

/**
 * Return the first configuration object of the given type
 *
 * This function must be called with the cfgobj_type lock held
 */
ldmsd_cfgobj_t ldmsd_cfgobj_first(ldmsd_cfgobj_type_t type)
{
	struct rbn *n;
	n = rbt_min(cfgobj_trees[type]);
	if (n)
		return ldmsd_cfgobj_get(container_of(n, struct ldmsd_cfgobj, rbn));
	return NULL;
}

/**
 * Return the next configuration object of the given type
 *
 * This function must be called with the cfgobj_type lock held
 */
ldmsd_cfgobj_t ldmsd_cfgobj_next(ldmsd_cfgobj_t obj)
{
	struct rbn *n;
	ldmsd_cfgobj_t nobj = NULL;

	n = rbn_succ(&obj->rbn);
	if (!n)
		goto out;
	nobj = ldmsd_cfgobj_get(container_of(n, struct ldmsd_cfgobj, rbn));
out:
	ldmsd_cfgobj_put(obj);	/* Drop the next reference */
	return nobj;
}

/*
 * *** Must be called with `cfgobj_locks[type]` held.
 */
ldmsd_cfgobj_t ldmsd_cfgobj_next_re(ldmsd_cfgobj_t obj, regex_t regex)
{
	int rc;
	for ( ; obj; obj = ldmsd_cfgobj_next(obj)) {
		rc = regexec(&regex, obj->name, 0, NULL, 0);
		if (rc)
			break;
	}
	return obj;
}

json_entity_t ldmsd_cfgobj_query_result_new()
{
	json_entity_t obj;

	if (!(obj = json_entity_new(JSON_DICT_VALUE)))
		goto oom;
	return obj;
oom:
	ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
	return NULL;
}

int ldmsd_cfgobj_query_attr_add(json_entity_t query, const char *name, enum json_value_e type, ...)
{
	json_entity_t n, v, a;
	va_list ap;

	if (!(n = json_entity_new(JSON_STRING_VALUE, name)))
		return ENOMEM;

	va_start(ap, type);
	switch (type) {
	case JSON_BOOL_VALUE:
		v = json_entity_new(type, va_arg(ap, int));
		if (!v)
			return ENOMEM;
		break;
	case JSON_DICT_VALUE:
		v = va_arg(ap, json_entity_t);
		if (!v)
			v = json_entity_new(type);
		break;
	case JSON_FLOAT_VALUE:
		v = json_entity_new(type, va_arg(ap, double));
		break;
	case JSON_INT_VALUE:
		v = json_entity_new(type, va_arg(ap, uint64_t));
		break;
	case JSON_STRING_VALUE:
		v = json_entity_new(type, va_arg(ap, char *));
		break;
	case JSON_ATTR_VALUE:
	default:
		return EINVAL;
	}
	va_end(ap);
	if (!v)
		return ENOMEM;
	a = json_entity_new(JSON_ATTR_VALUE, n, v);
	if (a)
		return ENOMEM;
	json_attr_add(query, a);
	return 0;
}

typedef int (*attr_add_fn)(ldmsd_cfgobj_t obj, json_entity_t query);
struct base_attr_add_entry {
	const char *attr_name;
	attr_add_fn fn;
};

int base_attr_add_entry_cmp(void *a, void *b)
{
	struct base_attr_add_entry *a_ = (struct base_attr_add_entry *)a;
	struct base_attr_add_entry *b_ = (struct base_attr_add_entry *)b;
	return strcmp(a_->attr_name, b_->attr_name);
}

int type_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	return ldmsd_cfgobj_query_attr_add(query, "type",
			JSON_STRING_VALUE, ldmsd_cfgobj_types[obj->type]);
}

int ref_count_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	return ldmsd_cfgobj_query_attr_add(query, "ref_count",
				JSON_INT_VALUE, obj->ref_count);
}

int enabled_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	return ldmsd_cfgobj_query_attr_add(query, "enabled", JSON_BOOL_VALUE, obj->enabled);
}

int uid_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	return ldmsd_cfgobj_query_attr_add(query, "uid", JSON_INT_VALUE, obj->uid);
}

int gid_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	return ldmsd_cfgobj_query_attr_add(query, "gid", JSON_INT_VALUE, obj->gid);
}

int perm_attr_add(ldmsd_cfgobj_t obj, json_entity_t query)
{
	char perm_s[8];
	snprintf(perm_s, 8, "%o", obj->perm);
	return ldmsd_cfgobj_query_attr_add(query, "perm", JSON_STRING_VALUE, perm_s);
}

static struct base_attr_add_entry base_attr_add_tbl[] = {
		{ "enabled",	enabled_attr_add },
		{ "gid",	gid_attr_add },
		{ "perm",	perm_attr_add },
		{ "ref_count",	ref_count_attr_add },
		{ "type",	type_attr_add },
		{ "uid",	uid_attr_add },
		NULL
};

int ldmsd_cfgobj_query_base_attr_add(ldmsd_cfgobj_t obj, json_entity_t query,
							const char *tgt)
{
	int rc, i = 0;
	json_entity_t t;
	struct base_attr_add_entry *handler;

	if (tgt) {
		handler = bsearch(tgt, base_attr_add_tbl,
				ARRAY_SIZE(base_attr_add_tbl),
				sizeof(struct base_attr_add_entry),
				base_attr_add_entry_cmp);
		if (!handler)
			return ENOENT;
		rc = handler->fn(obj, query);
		if (rc)
			return rc;
	} else {
		handler = base_attr_add_tbl[i++];
		while (handler) {
			rc = handler->fn(obj, query);
			if (rc)
				return rc;
			handler = base_attr_add_tbl[i++];
		}
	}
	return 0;
}

json_entity_t ldmsd_cfgobj_export_result_new(ldmsd_cfgobj_type_t type)
{
	json_entity_t obj, n, v, a;

	if (!(obj = json_entity_new(JSON_DICT_VALUE)))
		goto oom;
	if (!(n = json_entity_new(JSON_STRING_VALUE, "schema")))
		goto oom;
	if (!(v = json_entity_new(JSON_STRING_VALUE, cfgobj_type_tbl[type])))
		goto oom;
	if (!(a = json_entity_new(JSON_ATTR_VALUE, n, v)))
		goto oom;
	json_attr_add(obj, a);
	return obj;
oom:
	return NULL;
}

//(TYPE, n, v, TYPE, n, v, TYP, n, v, ...)

json_entity_t fn(va_list ap)
{
	json_entity_t obj, x;

	obj = json_entity_new(JSON_DICT_VALUE);
	if (!obj)
		return NULL;
	enum json_value_e type;
	switch (type) {
	case JSON_BOOL_VALUE:
		x = json_entity_new(type, va_arg(ap, int));
		break;
	case JSON_FLOAT_VALUE:
		x = json_entity_new(type, va_arg(ap, double));
		break;
	case JSON_INT_VALUE:
		x = json_entity_new(type, va_arg(ap, uint64_t));
		break;
	case JSON_STRING_VALUE:
		x = json_entity_new(type, va_arg(ap, char *));
		break;
	case JSON_DICT_VALUE:
		break;
	case JSON_LIST_VALUE:
		break;

	default:
		break;
	}
}

json_entity_t ldmsd_cfgobj_query_build(const char *name, ...)
{
	va_list ap;
	enum json_value_e type;
	json_entity_t obj, x;

	obj = json_entity_new(JSON_DICT_VALUE);
	if (!obj)
		goto err;

	va_start(ap, name);
attr:
	n = va_arg(ap, const char *);
	type = va_arg(ap, enum json_value_e);


	va_end(ap);
	return obj;
err:
	if (obj)
		json_entity_free(obj);
	return NULL;
}
