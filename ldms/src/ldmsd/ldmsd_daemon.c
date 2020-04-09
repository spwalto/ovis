/*
 * ldmsd_daemon.c
 *
 *  Created on: Apr 13, 2020
 *      Author: nnich
 */

#include <stdlib.h>
#include "ldmsd.h"

void ldmsd_daemon___del(ldmsd_cfgobj_t obj)
{
	ldmsd_daemon_t daemon = (ldmsd_daemon_t)obj;
	if (daemon->attr)
		json_entity_free(daemon->attr);
	ldmsd_cfgobj___del(obj);
}

int log_init(ldmsd_daemon_t obj, ldmsd_req_buf_t buf)
{
	json_entity_t path, level, a, n, v;
	path = level = NULL;

	if (!(n = json_entity_new(JSON_STRING_VALUE, "path")))
		goto oom;
	if (!(v = json_entity_new(JSON_STRING_VALUE, "stdout")))
		goto oom;
	if (!(a = json_entity_new(JSON_ATTR_VALUE, n, v)))
		goto oom;
	json_attr_add(obj->attr, a);
	if (!(n = json_entity_new(JSON_STRING_VALUE, "level")))
		goto oom;
	if (!(v = json_entity_new(JSON_STRING_VALUE, "ERROR")))
		goto oom;
	if (!(a = json_entity_new(JSON_ATTR_VALUE, n, v)))
		goto oom;
	json_attr_add(obj->attr, a);
	return 0;
oom:
	ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
	return ENOMEM;
}

int log_update(ldmsd_cfgobj_t obj, short enabled, json_entity_t dft, json_entity_t spc, ldmsd_req_buf_t buf)
{
	int rc;
	json_entity_t path, level;
	FILE *new;
	ldmsd_daemon_t log = (ldmsd_daemon_t)obj;
	if (spc) {
		path = json_value_find(spc, "path");
		level = json_value_find(spc, "level");
	}
	if (!path && dft)
		path = json_value_find(dft, "path");
	if (!level && dft)
		level = json_value_find(dft, "level");
	if (path) {
		rc = json_attr_mod(log->d, "path", json_value_str(path)->str);
		if (rc) {
			snprintf(buf->buf, buf->len, "Failed to update 'log:path'.");
			return rc;
		}
	}
	if (level) {
		rc = json_attr_mod(log->d, "level", json_value_str(level)->str);
		if (rc) {
			snprintf(buf->buf, buf->len, "Failed to update 'log:level'.");
			return rc;
		}
	}
	obj->enabled = enabled;

	if (obj->enabled) {
		new = ldmsd_open_log(json_value_str(path)->str);
		if (!new) {
			rc = errno;
			snprintf(buf->buf, buf->len, "Failed to open the log file '%s'.",
							json_value_str(path)->str);
			return rc;
		}
		if (log_fp)
			fclose(log_fp);
		log_fp = new;
	}
	return 0;
oom:
	ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
	return ENOMEM;
}

json_entity_t log_query(ldmsd_cfgobj_t obj, json_entity_t target, ldmsd_req_buf_t buf)
{
	json_entity_t query, name, attr, path, level, tgt;
	char *tgt_s;
	ldmsd_daemon_t log = (ldmsd_daemon_t)obj;

	query = ldmsd_cfgobj_query_result_new();
	if (!query)
		goto oom;
	if (target) {
		for (tgt = json_item_first(target); tgt; tgt = json_item_next(tgt)) {
			tgt_s = json_value_str(tgt)->str;
			if (0 == strncmp(tgt_s, "path", 4)) {
				name = json_entity_new(JSON_STRING_VALUE, "path");
				if (!name)
					goto oom;
				path = json_entity_new(JSON_STRING_VALUE, log->path);
				if (!path)
					goto oom;
				attr = json_entity_new(JSON_ATTR_VALUE, name, path);
				if (!attr)
					goto oom;
				json_attr_add(query, attr);
			} else if (0 == strncmp(tgt_s, "level", 5)) {
				name = json_entity_new(JSON_STRING_VALUE, "level");
				if (!name)
					goto oom;
				level = json_entity_new(JSON_INT_VALUE, log->level);
				if (!level)
					goto oom;
				attr = json_entity_new(JSON_ATTR_VALUE, name, level);
				if (!attr)
					goto oom;
				json_attr_add(query, attr);
			}
		}
	} else {
		name = json_entity_new(JSON_STRING_VALUE, "path");
		if (!name)
			goto oom;
		path = json_entity_new(JSON_STRING_VALUE, log->path);
		if (!path)
			goto oom;
		attr = json_entity_new(JSON_ATTR_VALUE, name, path);
		if (!attr)
			goto oom;
		json_attr_add(query, attr);
		name = json_entity_new(JSON_STRING_VALUE, "level");
		if (!name)
			goto oom;
		level = json_entity_new(JSON_INT_VALUE, log->level);
		if (!level)
			goto oom;
		attr = json_entity_new(JSON_ATTR_VALUE, name, level);
		if (!attr)
			goto oom;
		json_attr_add(query, attr);
	}
	return query;
oom:
	ldmsd_log(LDMSD_LERROR, "Out of memory\n");
	if (query)
		json_entity_free(query);
	errno = ENOMEM;
	return NULL;
}

json_entity_t __log_export(ldmsd_cfgobj_t obj, ldmsd_req_buf_t buf)
{
	return __log_query(obj, NULL, buf);
}

typedef int (*daemon_init_fn_t)(ldmsd_daemon_t daemon, ldmsd_req_buf_t buf);
struct daemon_obj_entry {
	const char *name;
	const char *key;
	short enabled;
	daemon_init_fn_t init;
	ldmsd_cfgobj_update_fn_t update;
	ldmsd_cfgobj_delete_fn_t delete;
	ldmsd_cfgobj_query_fn_t query;
	ldmsd_cfgobj_export_fn_t export;
};

int daemon_obj_entry_cmp(void *a, void *b)
{
	struct daemon_obj_entry *a_ = (struct daemon_obj_entry *)a;
	struct daemon_obj_entry *b_ = (struct daemon_obj_entry *)b;
	return strcmp(a_->name, b_->name);
}

/*
 * The numbers in the key fields are start ordered. The start order of the cfgobjs
 * with the same number does not matter.
 */
static struct daemon_obj_entry handler_tbl[] = {
	{ "default_auth",	"4_auth",
		auth_create,		auth_update,		auth_delete,
		auth_query,		auth_export },
	{ "daemonize",		"1_daemonize",
		daemonize_create,	daemonize_update,	daemonize_delete,
		daemonize_query,	daemonize_export },
	{ "kernel_set_path",	"4_kernel_set_path",
		kernel_create,		kernel_update,		kernel_delete,
		kernel_query,		kernel_export },
	{ "ldmsd-id",		"4_ldmsd-id",
		ldmsd_id_create,	ldmsd_id_update,	ldmsd_id_delete,
		ldmsd_id_query,		ldmsd_id_export },
	{ "log",		"0_log",	1,		log_init,
	  log_update,		log_delete,	log_query,	log_export },
	{ "pid_file",		"4_pid_file",		__pid_file_create },
	{ "set_memory",		"2_set_mem",		__set_mem_create },
	{ "startup",		"3_startup",		__startup_create },
	{ "workers",		"4_workers",		__worker_count_create },
	NULL,
};

void ldmsd_daemon___del(ldmsd_cfgobj_t obj)
{
	ldmsd_daemon_t daemon = (ldmsd_daemon_t)obj;
	if (daemon->attr)
		json_entity_free(daemon->attr);
	ldmsd_cfgobj___del(obj);
}

int ldmsd_daemon_create(const char *name, json_entity_t dft, json_entity_t spc,
		uid_t uid, gid_t gid, int perm, ldmsd_req_buf_t buf)
{
	struct daemon_obj_entry *prop;
	ldmsd_daemon_t obj;
	int rc;

	prop = bsearch(name, handler_tbl, ARRAY_SIZE(handler_tbl),
				sizeof(*prop), daemon_obj_entry_cmp);
	if (!prop) {
		snprintf(buf->buf, buf->len, "'%s' is invalid.", name);
		return EINVAL;
	}
	obj = ldmsd_cfgobj_new_with_auth(prop->key, LDMSD_CFGOBJ_DAEMON,
			sizeof(*prop), ldmsd_daemon___del, prop->update, prop->delete,
			prop->query, prop->export, uid, gid, perm, prop->enabled);
	if (!obj)
		goto oom;
	obj->attr = json_entity_new(JSON_DICT_VALUE);
	if (!obj->attr)
		goto oom;
	rc = prop->init(obj, dft, spc, buf);
	if (rc)
		goto err;

	ldmsd_cfgobj_unlock(&obj->obj);
	return rc;
oom:
	ldmsd_log(LDMSD_LCRITICAL, "Out of memory\n");
	rc = ENOMEM;
err:
	ldmsd_cfgobj_unlock(&obj->obj);
	ldmsd_cfgobj_put(&obj->obj);
	return rc;
}
