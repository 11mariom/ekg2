/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#include "commands.h"
#include "dynstuff.h"
#include "objects.h"
#include "plugins.h"
#include "stuff.h"
#include "vars.h"
#include "xmalloc.h"

list_t plugins = NULL;
list_t queries = NULL;

/*
 * plugin_load()
 *
 * �aduje wtyczk� o podanej nazwie.
 * 
 * 0/-1
 */
int plugin_load(const char *name)
{
#ifdef HAVE_LIBDL
	char *lib = saprintf("%s/%s.so", PLUGINS_PATH, name);
	void *plugin = dlopen(lib, RTLD_LAZY);
	char *init;
	int (*plugin_init)();

	if (!plugin) {
		print("generic_error", "Nie ma plagina!");
		xfree(lib);	
		return -1;
	}

	xfree(lib);

	init = saprintf("%s_plugin_init", name);

	if (!(plugin_init = dlsym(plugin, init))) {
		print("generic_error", "To nie plagin ekg!");
		dlclose(plugin);
		xfree(init);
		return -1;
	}
		
	xfree(init);
	
	if (plugin_init() == -1) {
		print("generic_error", "Plagin si� nie zainicjowa�");
		dlclose(plugin);
		return -1;
	}

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (!strcasecmp(p->name, name)) {
			p->dl = plugin;
			break;
		}
	}
#else
	debug("tried to load plugin \"%s\", but no dynamic plugins support\n", name);
	return -1;
#endif
}

/*
 * plugin_find()
 *
 * odnajduje plugin_t odpowiadaj�ce wtyczce o danej nazwie.
 */
plugin_t *plugin_find(const char *name)
{
	list_t l;

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (!p || !p->name || strcmp(p->name, name))
			continue;

		return p;
	}

	return NULL;
}

/*
 * plugin_unload()
 *
 * usuwa z pami�ci dan� wtyczk�, lub je�li wtyczka jest wkompilowana na
 * sta�e, deaktywuje j�.
 *
 * 0/-1
 */
int plugin_unload(plugin_t *p)
{
	if (!p)
		return -1;

	if (p->dl) {
#ifdef HAVE_LIBDL
		dlclose(p->dl);
		p->dl = NULL;
#else
		debug("tried to unload plugin, but no dynamic plugins support\n");
		return -1;
#endif
	}

	if (p->destroy)
		p->destroy();

	return 0;
}

/*
 * plugin_register()
 *
 * rejestruje dan� wtyczk�.
 *
 * 0/-1
 */
int plugin_register(plugin_t *p)
{
	list_add(&plugins, p, 0);

	return 0;
}

/*
 * plugin_unregister()
 *
 * od��cza wtyczk�.
 *
 * 0/-1
 */
int plugin_unregister(plugin_t *p)
{
	list_t l;

	if (!p)
		return -1;

	for (l = queries; l; ) {
		query_t *q = l->data;

		l = l->next;

		if (q->plugin == p)
			query_disconnect(q->plugin, q->name);
	}

	for (l = variables; l; ) {
		variable_t *v = l->data;

		l = l->next;

		if (v->plugin == p)
			variable_remove(v->plugin, v->name);
	}

	for (l = commands; l; ) {
		command_t *c = l->data;

		l = l->next;

		if (c->plugin == p)
			command_remove(c->plugin, c->name);
	}

	list_remove(&plugins, p, 0);

	return 0;
}

int query_connect(plugin_t *plugin, const char *name, void *handler, void *data)
{
	query_t q;

	memset(&q, 0, sizeof(q));
	q.plugin = plugin;
	q.name = xstrdup(name);
	q.handler = handler;
	q.data = data;
	
	return (list_add(&queries, &q, sizeof(q)) != NULL);
}

int query_disconnect(plugin_t *plugin, const char *name)
{
	list_t l;

	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if (q->plugin == plugin && q->name == name) {
			list_remove(&queries, q, 1);
			return 0;
		}
	}

	return -1;
}

int query_emit(plugin_t *plugin, const char *name, ...)
{
	static int nested = 0;
	int result = -1;
	va_list ap;
	list_t l;

	if (nested > 32) {
//		if (nested == 33)
//			debug("too many nested queries. exiting to avoid deadlock\n");
		return -1;
	}

	nested++;

	va_start(ap, name);

	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if (!q->name)
			continue;

		if (!strcmp(q->name, name) && (!plugin || (plugin == q->plugin))) {
			int (*handler)(void *data, va_list ap) = q->handler;

			q->count++;

			result = 0;

			if (handler(q->data, ap) == -1) {
				result = -1;
				goto cleanup;
			}
		}
	}

cleanup:
	va_end(ap);

	nested--;

	return result;
}

/*
 * watch_new()
 *
 * tworzy nowy obiekt typu watch_t i zwraca do niego wska�nik.
 *
 *  - plugin - obs�uguj�cy plugin
 *  - fd - obserwowany deskryptor
 *  - type - rodzaj obserwacji watch_type_t
 */
watch_t *watch_new(plugin_t *plugin, int fd, watch_type_t type)
{
	watch_t *w = xmalloc(sizeof(watch_t));

	memset(w, 0, sizeof(watch_t));
	w->plugin = plugin;
	w->fd = fd;
	w->type = type;

	if (w->type == WATCH_READ_LINE) {
		w->type = WATCH_READ;
		w->buf = string_init(NULL);
	}
	
	w->started = time(NULL);

	list_add(&watches, w, 0);

	return w;
}

/*
 * watch_find()
 *
 * zwraca obiekt watch_t o podanych parametrach.
 */
watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type)
{
	list_t l;

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w->plugin == plugin && w->fd == fd && w->type == type && !w->removed)
			return w;
	}

	return NULL;
}

/*
 * watch_free()
 *
 * zwalnia pami�� po obiekcie watch_t.
 */
void watch_free(watch_t *w)
{
	void (*handler)(int, int, int, void *);

	if (!w)
		return;

	w->removed = 1;
		
	if (w->buf)
		string_free(w->buf, 1);

	handler = w->handler;
	handler(1, w->fd, w->type, w->data);

	list_remove(&watches, w, 1);
}

/*
 * watch_handle_line()
 *
 * obs�uga deskryptor�w przegl�danych WATCH_READ_LINE.
 */
void watch_handle_line(watch_t *w)
{
	char buf[1024], *tmp;
	int ret;
	void (*handler)(int, int, const char *, void *) = w->handler;

	if (!w->persist)
		list_remove(&watches, w, 0);

	ret = read(w->fd, buf, sizeof(buf) - 1);

	if (ret > 0) {
		buf[ret] = 0;
		string_append(w->buf, buf);
	}

	if (ret == 0 || (ret == -1 && errno != EAGAIN))
		string_append_c(w->buf, '\n');

	while ((tmp = strchr(w->buf->str, '\n'))) {
		int index = tmp - w->buf->str;
		char *line = xstrmid(w->buf->str, 0, index);
		string_t new;
			
		if (strlen(line) > 1 && line[strlen(line) - 1] == '\r')
			line[strlen(line) - 1] = 0;

		handler(0, w->fd, line, w->data);
					
		new = string_init(w->buf->str + index + 1);
		string_free(w->buf, 1);
		w->buf = new;
		xfree(line);
	}

	/* je�li koniec strumienia, lub nie jest to ci�g�e przegl�danie,
	 * zwolnij pami�� i usu� z listy */
	if (!w->persist || ret == 0 || (ret == -1 && errno != EAGAIN)) {
		handler(1, w->fd, NULL, w->data);
		string_free(w->buf, 1);
		close(w->fd);

		if (w->persist)
			list_remove(&watches, w, 1);
	}
}

/*
 * watch_handle()
 *
 * obs�uga deskryptor�w typu WATCH_READ i WATCH_WRITE. je�li wyst�pi na
 * nich jakakolwiek aktywno��, wywo�ujemy dan� funkcj�. je�li nie jest
 * to sta�e przegl�danie, usuwamy.
 */
void watch_handle(watch_t *w)
{
	void (*handler)(int, int, int, void *) = w->handler;

	if (!w->persist) {
		list_remove(&watches, w, 0);
		handler(0, w->fd, w->type, w->data);
		handler(1, w->fd, w->type, w->data);
		xfree(w);
	} else {
		handler(0, w->fd, w->type, w->data);
		w->started = time(NULL);
	}
}

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, int persist, void *handler, void *data)
{
	watch_t *w = watch_new(plugin, fd, type);

	watch_persist_set(w, persist);
	watch_handler_set(w, handler);
	watch_data_set(w, data);
	
	return w;
}

int watch_remove(plugin_t *plugin, int fd, watch_type_t type)
{
	int res = -1;
	watch_t *w;

	while ((w = watch_find(plugin, fd, type))) {
		watch_free(w);
		res = 0;
	}

	return res;
}

PROPERTY_INT(watch, persist, int);
PROPERTY_INT(watch, timeout, time_t);
PROPERTY_DATA(watch);
PROPERTY_MISC(watch, handler, watch_handler_func_t, NULL);


