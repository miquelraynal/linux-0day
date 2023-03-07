// SPDX-License-Identifier: GPL-2.0
/*
 * Linux kernel module helpers.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "of_private.h"

ssize_t of_modalias(const struct device_node *np, char *str, ssize_t len)
{
	const char *compat;
	char *c;
	struct property *p;
	ssize_t csize;
	ssize_t tsize;

	/* Name & Type */
	/* %p eats all alphanum characters, so %c must be used here */
	csize = snprintf(str, len, "of:N%pOFn%c%s", np, 'T',
			 of_node_get_device_type(np));
	tsize = csize;
	len -= csize;
	if (str)
		str += csize;

	of_property_for_each_string(np, "compatible", p, compat) {
		csize = strlen(compat) + 1;
		tsize += csize;
		if (csize > len)
			continue;

		csize = snprintf(str, len, "C%s", compat);
		for (c = str; c; ) {
			c = strchr(c, ' ');
			if (c)
				*c++ = '_';
		}
		len -= csize;
		str += csize;
	}

	return tsize;
}

ssize_t of_printable_modalias(const struct device_node *np, char *str, ssize_t len)
{
	ssize_t sl;

	if (!np)
		return -ENODEV;

	sl = of_modalias(np, str, len - 2);
	if (sl < 0)
		return sl;
	if (sl > len - 2)
		return -ENOMEM;

	str[sl++] = '\n';
	str[sl] = 0;
	return sl;
}
EXPORT_SYMBOL_GPL(of_printable_modalias);

int of_request_module(const struct device_node *np)
{
	char *str;
	ssize_t size;
	int ret;

	if (!np)
		return -ENODEV;

	size = of_modalias(np, NULL, 0);
	if (size < 0)
		return size;

	/* Reserve an additional byte for the trailing '\0' */
	size++;

	str = kmalloc(size, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	of_modalias(np, str, size);
	str[size - 1] = '\0';
	ret = request_module(str);
	kfree(str);

	return ret;
}
EXPORT_SYMBOL_GPL(of_request_module);

int of_uevent(struct device_node *np, struct kobj_uevent_env *env)
{
	const char *compat, *type;
	struct alias_prop *app;
	struct property *p;
	int seen = 0;

	if (!np)
		return -ENODEV;

	add_uevent_var(env, "OF_NAME=%pOFn", np);
	add_uevent_var(env, "OF_FULLNAME=%pOF", np);
	type = of_node_get_device_type(np);
	if (type)
		add_uevent_var(env, "OF_TYPE=%s", type);

	/* Since the compatible field can contain pretty much anything
	 * it's not really legal to split it out with commas. We split it
	 * up using a number of environment variables instead. */
	of_property_for_each_string(np, "compatible", p, compat) {
		add_uevent_var(env, "OF_COMPATIBLE_%d=%s", seen, compat);
		seen++;
	}
	add_uevent_var(env, "OF_COMPATIBLE_N=%d", seen);

	seen = 0;
	mutex_lock(&of_mutex);
	list_for_each_entry(app, &aliases_lookup, link) {
		if (np == app->np) {
			add_uevent_var(env, "OF_ALIAS_%d=%s", seen,
				       app->alias);
			seen++;
		}
	}
	mutex_unlock(&of_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(of_uevent);

int of_uevent_modalias(const struct device_node *np, struct kobj_uevent_env *env)
{
	int sl;

	if (!np)
		return -ENODEV;

	/* Devicetree modalias is tricky, we add it in 2 steps */
	if (add_uevent_var(env, "MODALIAS="))
		return -ENOMEM;

	sl = of_modalias(np, &env->buf[env->buflen-1],
			 sizeof(env->buf) - env->buflen);
	if (sl >= (sizeof(env->buf) - env->buflen))
		return -ENOMEM;
	env->buflen += sl;

	return 0;
}
EXPORT_SYMBOL_GPL(of_uevent_modalias);
