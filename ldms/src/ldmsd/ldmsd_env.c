/*
 * ldmsd_env.c
 *
 *  Created on: Apr 9, 2020
 *      Author: nnich
 */

#include "ldmsd.h"
#include "ldmsd_request.h"

void ldmsd_env___del(ldmsd_cfgobj_t obj)
{
	ldmsd_env_t env = (ldmsd_env_t)obj;
	if (env->name)
		free(env->name);
	if (env->value)
		free(env->value);
	ldmsd_cfgobj___del(obj);
}

int ldmsd_env_update(ldmsd_cfgobj_t obj, json_entity_t dft, json_entity_t value)
{
	return ENOTSUP;
}

int ldmsd_env_delete(ldmsd_cfgobj_t obj)
{
	return ENOTSUP;
}

int ldmsd_env_start(ldmsd_cfgobj_t obj)
{
	ldmsd_env_t env = (ldmsd_env_t)obj;
	int rc = setenv(env->name);
	return rc;
}

int ldmsd_env_stop(ldmsd_cfgobj_t obj)
{
	return ENOTSUP;
}

static json_entity_t __add_value(ldmsd_env_t env, json_entity_t result)
{
	json_entity_t n, v, a;
	if (!(n = json_entity_new(JSON_STRING_VALUE, "value")))
		goto oom;
	if (!(v = json_entity_new(JSON_STRING_VALUE, env->value)))
		goto oom;
	if (!(a = json_entity_new(JSON_ATTR_VALUE, n, v)))
		goto oom;
	json_attr_add(result, a);
	return result;
oom:
	return NULL;
}

json_entity_t ldmsd_env_query(ldmsd_cfgobj_t obj, json_entity_t target)
{
	ldmsd_env_t env = (ldmsd_env_t)obj;
	json_entity_t result, a, n, v;
	int rc;

	result = ldmsd_cfgobj_query_result_new();
	if (!result)
		return NULL;
	return __add_value(env, result);
}

json_entity_t ldmsd_env_export(ldmsd_cfgobj_t obj)
{
	json_entity_t result, n, v, a;
	ldmsd_env_t env = (ldmsd_env_t)obj;
	result = ldmsd_cfgobj_export_result_new(LDMSD_CFGOBJ_ENV);
	if (!result)
		return NULL;
	return __add_value(env, result);
}

int ldmsd_env_create(const char *name, short enabled, json_entity_t dft, json_entity_t spc,
				uid_t uid, gid_t gid, int perm,
				ldmsd_req_buf_t buf)
{
	int rc;
	json_entity_t value;
	char *value_s;
	value = json_value_find(spc, "value");
	value_s = json_value_str(value)->str;
	ldmsd_env_t env = ldmsd_cfgobj_new_with_auth(name, LDMSD_CFGOBJ_ENV,
				sizeof(*env), ldmsd_env___del,
				ldmsd_env_update,
				ldmsd_env_delete,
				ldmsd_env_query,
				ldmsd_env_export,
				ldmsd_env_start,
				ldmsd_env_stop,
				uid, gid, perm, enabled);
	if (!env) {
		rc = errno;
		switch (rc) {
		case EEXIST:
			buf->off = snprintf(buf->buf, buf->len,
					"The env '%s' already exists.", name);
			break;
		case ENOMEM:
			buf->off = snprintf(buf->buf, buf->len, "Out of memory");
			break;
		default:
			buf->off = snprintf(buf->buf, buf->len,
					"Failed to create 'env' %s.", name);
			break;
		}
		return rc;
	}
	/*
	 * Export the environment variable right away so that
	 * any reference to these environ
	 */
	rc = setenv(name, value, 1);
	env->name = strdup(name);
	env->value = strdup(value_s);
	return 0;
}
