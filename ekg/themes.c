/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include "ekg2-config.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "xmalloc.h"
#include "windows.h"
#include "userlist.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

char *prompt_cache = NULL, *prompt2_cache = NULL, *error_cache = NULL;
const char *timestamp_cache = NULL;

int no_prompt_cache = 0;

list_t formats = NULL;

/*
 * format_find()
 *
 * odnajduje warto�� danego formatu. je�li nie znajdzie, zwraca pusty ci�g,
 * �eby nie musie� uwa�a� na �adne null-references.
 *
 *  - name.
 */
const char *format_find(const char *name)
{
	const char *tmp;
	int hash;
	list_t l;

	if (!name)
		return "";

	hash = ekg_hash(name);

	if (config_speech_app && !xstrchr(name, ',')) {
		char *name2 = saprintf("%s,speech", name);
		const char *tmp;
		
		if (xstrcmp((tmp = format_find(name2)), "")) {
			xfree(name2);
			return tmp;
		}
		
		xfree(name2);
	}

	if (config_theme && (tmp = xstrchr(config_theme, ',')) && !xstrchr(name, ',')) {
		char *name2 = saprintf("%s,%s", name, tmp + 1);
		const char *tmp;
		
		if (xstrcmp((tmp = format_find(name2)), "")) {
			xfree(name2);
			return tmp;
		}
		
		xfree(name2);
	}
	
	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (hash == f->name_hash && !xstrcasecmp(f->name, name))
			return f->value;
	}
	
	return "";
}

/*
 * format_ansi()
 *
 * zwraca sekwencj� ansi odpowiadaj�c� danemu kolorkowi z themem�w ekg.
 */
const char *format_ansi(char ch)
{
	if (ch == 'k')
		return "\033[0;30m";
	if (ch == 'K')
		return "\033[1;30m";
	if (ch == 'l')
		return "\033[40m";
	if (ch == 'r')
		return "\033[0;31m";
	if (ch == 'R')
		return "\033[1;31m";
	if (ch == 's')
		return "\033[41m";
	if (ch == 'g')
		return "\033[0;32m";
	if (ch == 'G')
		return "\033[1;32m";
	if (ch == 'h')
		return "\033[42m";
	if (ch == 'y')
		return "\033[0;33m";
	if (ch == 'Y')
		return "\033[1;33m";
	if (ch == 'z')
		return "\033[43m";
	if (ch == 'b')
		return "\033[0;34m";
	if (ch == 'B')
		return "\033[1;34m";
	if (ch == 'e')
		return "\033[44m";
	if (ch == 'm' || ch == 'p')
		return "\033[0;35m";
	if (ch == 'M' || ch == 'P')
		return "\033[1;35m";
	if (ch == 'q')
		return "\033[45m";
	if (ch == 'c')
		return "\033[0;36m";
	if (ch == 'C')
		return "\033[1;36m";
	if (ch == 'd')
		return "\033[46m";
	if (ch == 'w')
		return "\033[0;37m";
	if (ch == 'W')
		return "\033[1;37m";
	if (ch == 'x')
		return "\033[47m";
	if (ch == 'i')
		return "\033[5m";
	if (ch == 'n')
		return "\033[0m";
	if (ch == 'T')
		return "\033[1m";

	return "";
}

/*
 * va_format_string()
 *
 * formatuje zgodnie z podanymi parametrami ci�g znak�w.
 *
 *  - format - warto��, nie nazwa formatu,
 *  - ap - argumenty.
 */
char *va_format_string(const char *format, va_list ap)
{
	static int dont_resolve = 0;
	string_t buf = string_init(NULL);
	const char *p, *args[9];
	int i, argc = 0;

	/* liczymy ilo�� argument�w */
	for (p = format; *p; p++) {
		if (*p == '\\' && p[1] == '%') {
			p += 2;
			continue;
		}

		if (*p != '%')
			continue;

		p++;

		if (!*p)
			break;

		if (*p == '@') {
			p++;

			if (!*p)
				break;

			if ((*p - '0') > argc)
				argc = *p - '0';
			
		} else if (*p == '(' || *p == '[') {
			if (*p == '(') {
				while (*p && *p != ')')
					p++;
			} else {
				while (*p && *p != ']')
					p++;
			}

			if (*p)
				p++;
			
			if (!*p)
				break;
			
			if ((*p - '0') > argc)
				argc = *p - '0';
		} else {
			if (*p >= '1' && *p <= '9' && (*p - '0') > argc)
				argc = *p - '0';
		}
	}

	for (i = 0; i < 9; i++)
		args[i] = NULL;

	for (i = 0; i < argc; i++)
		args[i] = va_arg(ap, char*);

	if (!dont_resolve) {
		dont_resolve = 1;
		if (no_prompt_cache) {
			/* zawsze czytaj */
			timestamp_cache = format_find("timestamp");
			prompt_cache = format_string(format_find("prompt"));
			prompt2_cache = format_string(format_find("prompt2"));
			error_cache = format_string(format_find("error"));
		} else {
			/* tylko je�li nie s� keszowanie */
			if (!timestamp_cache)
				timestamp_cache = format_find("timestamp");
			if (!prompt_cache)
				prompt_cache = format_string(format_find("prompt"));
			if (!prompt2_cache)
				prompt2_cache = format_string(format_find("prompt2"));
			if (!error_cache)
				error_cache = format_string(format_find("error"));
		}
		dont_resolve = 0;
	}
	
	p = format;
	
	while (*p) {
		int escaped = 0;

		if (*p == '\\' && p[1] == '%') {
			escaped = 1;
			p++;
		}

		if (*p == '%' && !escaped) {
			int fill_before, fill_after, fill_soft, fill_length;
			char fill_char;

			p++;
			if (!*p)
				break;
			if (*p == '%')
				string_append_c(buf, '%');
			if (*p == '>')
				string_append(buf, prompt_cache);
			if (*p == ')')
				string_append(buf, prompt2_cache);
			if (*p == '!')
				string_append(buf, error_cache);
			if (*p == '|')
				string_append(buf, "\033[00m");	/* g�upie, wiem */
			if (*p == ']')
				string_append(buf, "\033[000m");	/* jeszcze g�upsze */
			if (*p == '#')
				string_append(buf, timestamp(timestamp_cache));
			else if (config_display_color) {
				string_append(buf, format_ansi(*p));
			}

			if (*p == '@') {
				char *str = (char*) args[*(p + 1) - '1'];

				if (str) {
					char *q = str + xstrlen(str) - 1;

					while (q >= str && !isalpha_pl_PL(*q))
						q--;

					if (*q == 'a')
						string_append(buf, "a");
					else
						string_append(buf, "y");
				}
				p += 2;
				continue;
			}

			fill_before = 0;
			fill_after = 0;
			fill_length = 0;
			fill_char = ' ';
			fill_soft = 1;

			if (*p == '[' || *p == '(') {
				char *q;

				fill_soft = (*p == '(');

				p++;
				fill_char = ' ';

				if (*p == '.') {
					fill_char = '0';
					p++;
				} else if (*p == ',') {
					fill_char = '.';
					p++;
				} else if (*p == '_') {
					fill_char = '_';
					p++;
				}

				fill_length = strtol(p, &q, 0);
				p = q;
				if (fill_length > 0)
					fill_after = 1;
				else {
					fill_length = -fill_length;
					fill_before = 1;
				}
				p++;
			}

			if (*p >= '1' && *p <= '9') {
				char *str = (char *) args[*p - '1'];
				int i, len;

				if (!str)
					str = "";
				len = xstrlen(str);

				if (fill_length) {
					if (len >= fill_length) {
						if (!fill_soft)
							len = fill_length;
						fill_length = 0;
					} else
						fill_length -= len;
				}

				if (fill_before)
					for (i = 0; i < fill_length; i++)
						string_append_c(buf, fill_char);

				string_append_n(buf, str, len);

				if (fill_after) 
					for (i = 0; i < fill_length; i++)
						string_append_c(buf, fill_char);

			}
		} else
			string_append_c(buf, *p);

		p++;
	}

	if (!dont_resolve && no_prompt_cache)
		theme_cache_reset();

	if (!config_display_pl_chars)
		iso_to_ascii(buf->str);

	return string_free(buf, 0);
}

/*
 * fstring_new()
 *
 * zamienia sformatowany ci�g znak�w ansi na Nowy-i-Lepszy(tm).
 *
 *  - str - ci�g znak�w,
 * 
 * zwraca zaalokowan� fstring_t.
 */
fstring_t *fstring_new(const char *str)
{
	fstring_t *res = xmalloc(sizeof(fstring_t));
	short attr = 128;
	int i, j, len = 0;

	res->margin_left = -1;

	for (i = 0; str[i]; i++) {
		if (str[i] == 27) {
			if (str[i + 1] != '[')
				continue;

			while (str[i] && !isalpha_pl_PL(str[i]))
				i++;

			i--;
			
			continue;
		}

		if (str[i] == 9) {
			len += (8 - (len % 8));
			continue;
		}

		if (str[i] == 13)
			continue;

                if (str[i + 1] && str[i] == '/' && str[i + 1] == '|') {
                        if ((i != 0 && str[i - 1] != '/') || i == 0) {
                                i++;
                                continue;
                        }
			continue;
                }

		len++;
	}

	res->str = xmalloc(len + 1);
	res->attr = xmalloc((len + 1) * sizeof(short));
	res->prompt_len = 0;
	res->prompt_empty = 0;

	for (i = 0, j = 0; str[i]; i++) {
		if (str[i] == 27) {
			int tmp = 0;

			if (str[i + 1] != '[')
				continue;

			i += 2;

			/* obs�uguje tylko "\033[...m", tak ma by� */
			
			for (; str[i]; i++) {
				if (str[i] >= '0' && str[i] <= '9') {
					tmp *= 10;
					tmp += (str[i] - '0');
				}

				if (str[i] == ';' || isalpha_pl_PL(str[i])) {
					if (tmp == 0) {
						attr = 128;

						/* prompt jako \033[00m */
						if (str[i - 1] == '0' && str[i - 2] == '0')
							res->prompt_len = j;

						/* odst�p jako \033[000m */
						if (i > 3 && str[i - 1] == '0' && str[i - 2] == '0' && str[i - 3] == 0) {
							res->prompt_len = j;
							res->prompt_empty = 1;
						}
					}
					if (tmp == 1)
						attr |= 64;

					if (tmp == 5)
						attr |= 256;

					if (tmp >= 30 && tmp <= 37) {
						attr &= 127;
						attr |= (tmp - 30);
					}

					if (tmp >= 40 && tmp <= 47) {
						attr &= 127;
						attr |= (tmp - 40) << 3;
					} 

					tmp = 0;
				}

				if (isalpha_pl_PL(str[i]))
					break;
			}

			continue;
		}

		if (str[i] == 13)
			continue;

		if (str[i + 1] && str[i] == '/' && str[i + 1] == '|') {
			if ((i != 0 && str[i - 1] != '/') || i == 0) {
				res->margin_left = j;
				i++;
				continue;
			}
			continue;
		}

		if (str[i] == 9) {
			int k = 0, l = 8 - (j % 8);

			for (k = 0; k < l; j++, k++) {
				res->str[j] = ' ';
				res->attr[j] = attr;
			}

			continue;
		}
		
		res->str[j] = str[i];
		res->attr[j] = attr;
		j++;
	}

	res->str[j] = 0;
	res->attr[j] = 0;

	return res;
}

/*
 * fstring_free()
 *
 * zwalnia pami�� zajmowan� przez fstring_t
 *
 *  - str - do usuni�cia.
 */
void fstring_free(fstring_t *str)
{
	if (!str)
		return;

	xfree(str->str);
	xfree(str->attr);
	xfree(str->private);
	xfree(str);
}

/*
 * format_string()
 *
 * j.w. tyle �e nie potrzeba dawa� mu va_list, a wystarcz� zwyk�e parametry.
 *
 *  - format... - j.w.,
 */
char *format_string(const char *format, ...)
{
	va_list ap;
	char *tmp;
	
	va_start(ap, format);
	tmp = va_format_string(format, ap);
	va_end(ap);

	return tmp;
}

/*
 * print_window()
 *
 * wy�wietla tekst w podanym oknie.
 *  
 *  - target - nazwa okna
 *  - session - sesja, w kt�rej wy�wietlamy
 *  - separate - czy niezb�dne jest otwieranie nowego okna?
 *  - theme, ... - tre��.
 */
void print_window(const char *target, session_t *session, int separate, const char *theme, ...)
{
	char *tmp, *stmp, *line, *prompt = NULL, *newtarget = NULL;
	va_list ap;

	/* je�li podamy nazw� z zasobem
	 * i nie ma otwartego okna, a jest otwarte dla nazwy bez
	 * zasobem to wrzucamy tam. je�li mamy otwarte okno dla zasobu,
	 * a przychodzi z innego, otwieramy nowe. */

	if (!window_find_s(session, target)) {
		const char *res;
		userlist_t *u;
		
		if ((res = xstrchr(target, '/'))) {
			newtarget = xstrdup(target);
			*(xstrchr(newtarget, '/')) = 0;
			u = userlist_find(session, target);
			/* XXX cza dorobi�, szefie */
		} else {
			u = userlist_find(session, target);

			if (u && window_find_s(session, u->uid))
				newtarget = xstrdup(u->uid);
			else if (u && u->nickname)
				newtarget = xstrdup(u->nickname);
		}
	}

	if (newtarget) 
		target = newtarget;

	if (!target)
		target = "__current";

	va_start(ap, theme);
	tmp = stmp = va_format_string(format_find(theme), ap);
	va_end(ap);

	while ((line = split_line(&tmp))) {
		char *p;

		if ((p = xstrstr(line, "\033[00m"))) {
			xfree(prompt);
			if (p != line) 
				prompt = xstrmid(line, 0, (int) (p - line) + 5);
			else
				prompt = NULL;
			line = p;
		}

		if (prompt) {
			char *tmp = saprintf("%s%s", prompt, line);
			window_print(target, session, separate, fstring_new(tmp));
			xfree(tmp);
		} else
			window_print(target, session, separate, fstring_new(line));
	}

	xfree(prompt);
	xfree(stmp);
	xfree(newtarget);
}

/*
 * theme_cache_reset()
 *
 * usuwa cache'owane prompty. przydaje si� przy zmianie theme'u.
 */
void theme_cache_reset()
{
	xfree(prompt_cache);
	xfree(prompt2_cache);
	xfree(error_cache);
	
	prompt_cache = prompt2_cache = error_cache = NULL;
	timestamp_cache = NULL;
}

/*
 * format_add()
 *
 * dodaje dan� formatk� do listy.
 *
 *  - name - nazwa,
 *  - value - warto��,
 *  - replace - je�li znajdzie, to zostawia (=0) lub zamienia (=1).
 */
int format_add(const char *name, const char *value, int replace)
{
	struct format f;
	list_t l;
	int hash;

	if (!name || !value)
		return -1;

	hash = ekg_hash(name);

	if (hash == ekg_hash("no_prompt_cache") && !xstrcasecmp(name, "no_prompt_cache")) {
		no_prompt_cache = 1;
		return 0;
	}
	
	for (l = formats; l; l = l->next) {
		struct format *g = l->data;

		if (hash == g->name_hash && !xstrcasecmp(name, g->name)) {
			if (replace) {
				xfree(g->value);
				g->value = xstrdup(value);
			}

			return 0;
		}
	}

	f.name = xstrdup(name);
	f.name_hash = ekg_hash(name);
	f.value = xstrdup(value);

	return (list_add(&formats, &f, sizeof(f)) ? 0 : -1);
}

/*
 * format_remove()
 *
 * usuwa formatk� o danej nazwie.
 *
 *  - name.
 */
int format_remove(const char *name)
{
	list_t l;

	if (!name)
		return -1;

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		if (!xstrcasecmp(f->name, name)) {
			xfree(f->value);
			xfree(f->name);
			list_remove(&formats, f, 1);
		
			return 0;
		}
	}

        return -1;
}

/*
 * theme_open() // funkcja wewn�trzna
 *
 * pr�buje otworzy� plik, je�li jeszcze nie jest otwarty.
 *
 *  - prevfd - deskryptor z poprzedniego wywo�ania,
 *  - prefix - �cie�ka,
 *  - filename - nazwa pliku.
 */
static FILE *theme_open(FILE *prevfd, const char *prefix, const char *filename)
{
	char buf[PATH_MAX];
	int save_errno;
	FILE *f;

	if (prevfd)
		return prevfd;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s", filename);

	if ((f = fopen(buf, "r")))
		return f;

	if (prefix)
		snprintf(buf, sizeof(buf), "%s/%s.theme", prefix, filename);
	else
		snprintf(buf, sizeof(buf), "%s.theme", filename);

	save_errno = errno;
	
	if ((f = fopen(buf, "r")))
		return f;

	if (errno == ENOENT)
		errno = save_errno;

	return NULL;
}

/*
 * theme_read()
 *
 * wczytuje opis wygl�du z podanego pliku. 
 *
 *  - filename - nazwa pliku z opisem,
 *  - replace - czy zast�powa� istniej�ce wpisy.
 *
 * zwraca 0 je�li wszystko w porz�dku, -1 w przypadku b��du.
 */
int theme_read(const char *filename, int replace)
{
        char *buf;
        FILE *f = NULL;

        if (!filename) {
                filename = prepare_path("default.theme", 0);
		if (!filename || !(f = fopen(filename, "r")))
			return -1;
        } else {
		char *fn = xstrdup(filename), *tmp;

		if ((tmp = xstrchr(fn, ',')))
			*tmp = 0;
		
		errno = ENOENT;
		f = NULL;

		if (!xstrchr(filename, '/')) {
			f = theme_open(f, prepare_path("", 0), fn);
			f = theme_open(f, prepare_path("themes", 0), fn);
			f = theme_open(f, DATADIR "/themes", fn);
		}

		xfree(fn);

		if (!f)
			return -1;
	}

	theme_free();
	theme_init();
//	ui_event("theme_init");

        while ((buf = read_file(f))) {
                char *value, *p;

                if (buf[0] == '#') {
			xfree(buf);
                        continue;
		}

                if (!(value = xstrchr(buf, ' '))) {
			xfree(buf);
			continue;
		}

		*value++ = 0;

		for (p = value; *p; p++) {
			if (*p == '\\') {
				if (!*(p + 1))
					break;
				if (*(p + 1) == 'n')
					*p = '\n';
				memmove(p + 1, p + 2, xstrlen(p + 1));
			}
		}

		if (buf[0] == '-')
			format_remove(buf + 1);
		else
			format_add(buf, value, replace);

		xfree(buf);
        }

        fclose(f);

	theme_cache_reset();

        return 0;
}

/*
 * theme_free()
 *
 * usuwa formatki z pami�ci.
 */
void theme_free()
{
	list_t l;

	for (l = formats; l; l = l->next) {
		struct format *f = l->data;

		xfree(f->name);
		xfree(f->value);
	}	

	list_destroy(formats, 1);
	formats = NULL;

	theme_cache_reset();
}

void theme_plugins_init()
{
	list_t l;
	
	if (!plugins)
		return;

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (!p)
			continue;

		plugin_theme_reload(p);
	}
}

/*
 * theme_init()
 *
 * ustawia domy�lne warto�ci formatek.
 */
void theme_init()
{
	theme_cache_reset();

	/* wykorzystywane w innych formatach */
	format_add("prompt", "%K:%g:%G:%n", 1);
	format_add("prompt,speech", " ", 1);
	format_add("prompt2", "%K:%c:%C:%n", 1);
	format_add("prompt2,speech", " ", 1);
	format_add("error", "%K:%r:%R:%n", 1);
	format_add("error,speech", "b��d!", 1);
	format_add("timestamp", "%T", 1);
	format_add("timestamp,speech", " ", 1);

	/* prompty dla ui-readline */
	format_add("readline_prompt", "% ", 1);
	format_add("readline_prompt_away", "/ ", 1);
	format_add("readline_prompt_invisible", ". ", 1);
	format_add("readline_prompt_query", "%1> ", 1);
	format_add("readline_prompt_win", "%1%% ", 1);
	format_add("readline_prompt_away_win", "%1/ ", 1);
	format_add("readline_prompt_invisible_win", "%1. ", 1);
	format_add("readline_prompt_query_win", "%2:%1> ", 1);
	format_add("readline_prompt_win_act", "%1 (act/%2)%% ", 1);
	format_add("readline_prompt_away_win_act", "%1 (act/%2)/ ", 1);
	format_add("readline_prompt_invisible_win_act", "%1 (act/%2). ", 1);
	format_add("readline_prompt_query_win_act", "%2:%1 (act/%3)> ", 1);

	format_add("readline_more", _("-- Wci�nij Enter by kontynuowa� lub Ctrl-D by przerwa� --"), 1);

	/* prompty i statusy dla ui-ncurses */
	format_add("ncurses_prompt_none", "", 1);
	format_add("ncurses_prompt_query", "[%1] ", 1);
	format_add("statusbar", " %c(%w%{time}%c)%w %c(%w%{?session %{?away %G}%{?avail %Y}%{?chat %W}%{?dnd %K}%{?xa %g}%{?invisible %C}%{?notavail %r}%{session}}%{?!session ---}%c) %{?window (%wwin%c/%w%{window}}%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%W}%{activity}%{?activity %c)%w}%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
	format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)}}%{?!query %c(%wekg2%c/%w%{version}%c) (%w%{url}%c)}", 1);
	format_add("statusbar_act_important", "%W", 1);
	format_add("statusbar_act", "%K", 1);
	format_add("statusbar_timestamp", "%H:%M", 1);

	/* dla funkcji format_user() */
	format_add("known_user", "%T%1%n/%2", 1);
	format_add("known_user,speech", "%1", 1);
	format_add("unknown_user", "%T%1%n", 1);

	/* cz�sto wykorzystywane, r�ne, przydatne itd. */
	format_add("none", "%1\n", 1);
	format_add("generic", "%> %1\n", 1);
	format_add("generic_bold", "%> %T%1%n\n", 1);
	format_add("generic2", "%) %1\n", 1);
	format_add("generic2_bold", "%) %T%1%n\n", 1);
	format_add("generic_error", "%! %1\n", 1);
	format_add("debug", "%n%1\n", 1);
	format_add("not_enough_params", _("%! Za ma�o parametr�w. Spr�buj %Thelp %1%n\n"), 1);
	format_add("invalid_params", _("%! Nieprawid�owe parametry. Spr�buj %Thelp %1%n\n"), 1);
	format_add("invalid_uid", _("%! Nieprawid�owy identyfikator u�ytkownika\n"), 1);
	format_add("invalid_session", _("%! Nieprawid�owa sesja\n"), 1);
	format_add("invalid_nick", _("%! Nieprawid�owa nazwa u�ytkownika\n"), 1);
	format_add("user_not_found", _("%! Nie znaleziono u�ytkownika %T%1%n\n"), 1);
	format_add("not_implemented", _("%! Tej funkcji jeszcze nie ma\n"), 1);
	format_add("unknown_command", _("%! Nieznane polecenie: %T%1%n\n"), 1);
	format_add("welcome", _("%> %Tekg2-%1%n (%ge%Gk%gg %Gr%ge%Ga%gk%Gt%gy%Gw%ga%Gc%gj%Ga%n)\n%> Program jest rozprowadzany na zasadach licencji GPL v2\n\n"), 1);
	format_add("welcome,speech", _("witamy w e k g 2."), 1);
	format_add("ekg_version", _("%) %Tekg2-%1%n (skompilowano %2)\n"), 1);
	format_add("secure", _("%Y(szyfrowane)%n "), 1);

	/* mail */
	format_add("new_mail_one", _("%) Masz now� wiadomo�� email\n"), 1);
	format_add("new_mail_two_four", _("%) Masz %1 nowe wiadomo�ci email\n"), 1);
	format_add("new_mail_more", _("%) Masz %1 nowych wiadomo�ci email\n"), 1);

	/* add, del */
	format_add("user_added", _("%> (%2) Dopisano %T%1%n do listy kontakt�w\n"), 1);
	format_add("user_deleted", _("%) (%2) Usuni�to %T%1%n z listy kontakt�w\n"), 1);
	format_add("user_cleared_list", _("%) (%1) Wyczyszczono list� kontakt�w\n"), 1);
	format_add("user_exists", _("%! (%2) %T%1%n ju� istnieje w li�cie kontakt�w\n"), 1);
	format_add("user_exists_other", _("%! (%3) %T%1%n ju� istnieje w li�cie kontakt�w jako %2\n"), 1);

	/* zmiany stanu */
	format_add("away", _("%> (%1) Zmieniono stan na %Gzaj�ty%n\n"), 1);
	format_add("away_descr", _("%> (%3) Zmieniono stan na %Gzaj�ty%n: %T%1%n%2\n"), 1);
	format_add("back", _("%> (%1) Zmieniono stan na %Ydost�pny%n\n"), 1);
	format_add("back_descr", _("%> (%3) Zmieniono stan na %Ydost�pny%n: %T%1%n%2%n\n"), 1);
	format_add("invisible", _("%> (%1) Zmieniono stan na %cniewidoczny%n\n"), 1);
	format_add("invisible_descr", _("%> (%3) Zmieniono stan na %cniewidoczny%n: %T%1%n%2\n"), 1);
	format_add("dnd", _("%> (%1) Zmieniono stan na %Bnie przeszkadza�%n\n"), 1);
	format_add("dnd_descr", _("%> (%3) Zmieniono stan na %Bnie przeszkadza�%n: %T%1%n%2%n\n"), 1);
	format_add("chat", _("%> (%1) Zmieniono stan na %Wch�tny do rozmowy%n\n"), 1);
	format_add("chat_descr", _("%> (%3) Zmieniono stan na %Wch�tny do rozmowy%n: %T%1%n%2%n\n"), 1);
	format_add("xa", _("%> (%1) Zmieniono stan na %gextended away%n\n"), 1);
	format_add("xa_descr", _("%> (%3) Zmieniono stan na %gextended away%n: %T%1%n%2%n%n\n"), 1);
	format_add("private_mode_is_on", _("%> (%1) Tryb ,,tylko dla znajomych'' jest w��czony\n"), 1);
	format_add("private_mode_is_off", _("%> (%1) Tryb ,,tylko dla znajomych'' jest wy��czony\n"), 1);
	format_add("private_mode_on", _("%> (%1) W��czono tryb ,,tylko dla znajomych''\n"), 1);
	format_add("private_mode_off", _("%> (%1) Wy��czono tryb ,,tylko dla znajomych''\n"), 1);
	format_add("private_mode_invalid", _("%! Nieprawid�owa warto��\n"), 1);
	format_add("descr_too_long", _("%! %|D�ugo�� opisu przekracza limit o %T%1%n znak�w\nOpis:%B%2%b%3%n\n"), 1);

	format_add("auto_away", _("%> (%1) Automagicznie zmieniono stan na %Gzaj�ty%n\n"), 1);
	format_add("auto_away_descr", _("%> (%2) Automagicznie zmieniono stan na %Gzaj�ty%n: %T%1%n\n"), 1);
	format_add("auto_back", _("%> (%1) Automagicznie zmieniono stan na %Ydost�pny%n\n"), 1);
	format_add("auto_back_descr", _("%> (%2) Automagicznie zmieniono stan na %Ydost�pny%n: %T%1%n\n"), 1);

	/* pomoc */
	format_add("help", "%> %T%1%n%2 - %3\n", 1);
	format_add("help_more", "%) %|%1\n", 1);
	format_add("help_alias", _("%) %T%1%n jest aliasem i nie posiada opisu\n"), 1);
	format_add("help_footer", _("\n%> %|Wi�cej szczeg��w na temat komend zwr�ci %Thelp <komenda>%n. Poprzedzenie komendy znakiem %T^%n spowoduje ukrycie jej wyniku. Zamiast parametru <numer/alias> mo�na u�y� znaku %T$%n oznaczaj�cego aktualnego rozm�wc�.\n\n"), 1);
	format_add("help_quick", _("%> %|Przed u�yciem przeczytaj ulotk�. Plik %Tdocs/ULOTKA%n zawiera kr�tki przewodnik po za��czonej dokumentacji. Je�li go nie masz, mo�esz �ci�gn�� pakiet ze strony %Thttp://dev.null.pl/ekg2/%n\n"), 1);
	format_add("help_set_file_not_found", _("%! Nie znaleziono opisu zmiennych (nieprawid�owa instalacja)\n"), 1);
        format_add("help_set_file_not_found_plugin", _("%! Nie znaleziono opisu zmiennych dla pluginu %T%1%n (nieprawid�owa instalacja)\n"), 1);
	format_add("help_set_var_not_found", _("%! Nie znaleziono opisu zmiennej %T%1%n\n"), 1);
	format_add("help_set_header", _("%> %T%1%n (%2, domy�lna warto��: %3)\n%>\n"), 1);
	format_add("help_set_body", "%> %|%1\n", 1);
	format_add("help_set_footer", "", 1);
        format_add("help_command_body", "%> %|%1\n", 1);
        format_add("help_command_file_not_found", _("%! Nie znaleziono opisu komend (nieprawid�owa instalacja)\n"), 1);
        format_add("help_command_file_not_found_plugin", _("%! Nie znaleziono opisu komend dla pluginu %T%1%n (nieprawid�owa instalacja)\n"), 1);
        format_add("help_command_not_found", _("%! Nie znaleziono opisu komendy %T%1%n\n"), 1);


	/* ignore, unignore, block, unblock */
	format_add("ignored_added", _("%> Dodano %T%1%n do listy ignorowanych\n"), 1);
        format_add("ignored_modified", _("%> Zmodyfikowano poziom ignorowania %T%1%n\n"), 1);
	format_add("ignored_deleted", _("%) Usuni�to %1 z listy ignorowanych\n"), 1);
	format_add("ignored_deleted_all", _("%) Usuni�to wszystkich z listy ignorowanych\n"), 1);
	format_add("ignored_exist", _("%! %1 jest ju� na li�cie ignorowanych\n"), 1);
	format_add("ignored_list", "%> %1 %2\n", 1);
	format_add("ignored_list_empty", _("%! Lista ignorowanych u�ytkownik�w jest pusta\n"), 1);
	format_add("error_not_ignored", _("%! %1 nie jest na li�cie ignorowanych\n"), 1);
	format_add("blocked_added", _("%> Dodano %T%1%n do listy blokowanych\n"), 1);
	format_add("blocked_deleted", _("%) Usuni�to %1 z listy blokowanych\n"), 1);
	format_add("blocked_deleted_all", _("%) Usuni�to wszystkich z listy blokowanych\n"), 1);
	format_add("blocked_exist", _("%! %1 jest ju� na li�cie blokowanych\n"), 1);
	format_add("blocked_list", "%> %1\n", 1);
	format_add("blocked_list_empty", _("%! Lista blokowanych u�ytkownik�w jest pusta\n"), 1);
	format_add("error_not_blocked", _("%! %1 nie jest na li�cie blokowanych\n"), 1);

	/* lista kontakt�w */
	format_add("list_empty", _("%! Lista kontakt�w jest pusta\n"), 1);
	format_add("list_avail", _("%> %1 %Y(dost�pn%@2)%n %b%3:%4%n\n"), 1);
	format_add("list_avail_descr", _("%> %1 %Y(dost�pn%@2: %n%5%Y)%n %b%3:%4%n\n"), 1);
	format_add("list_away", _("%> %1 %G(zaj�t%@2)%n %b%3:%4%n\n"), 1);
	format_add("list_away_descr", _("%> %1 %G(zaj�t%@2: %n%5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd", _("%> %1 %B(nie przeszkadza�)%n %b%3:%4%n\n"), 1);
	format_add("list_dnd_descr", _("%> %1 %G(nie przeszkadza�%n: %5%G)%n %b%3:%4%n\n"), 1);
	format_add("list_chat", _("%> %1 %W(ch�tny do rozmowy)%n %b%3:%4%n\n"), 1);
	format_add("list_chat_descr", _("%> %1 %W(ch�tny do rozmowy%n: %5%W)%n %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(b��d) %b%3:%4%n\n"), 1);
	format_add("list_error", _("%> %1 %m(b��d%n: %5%m)%n %b%3:%4%n\n"), 1);
	format_add("list_xa", _("%> %1 %g(bardzo zaj�t%@2)%n %b%3:%4%n\n"), 1);
	format_add("list_xa_descr", _("%> %1 %g(bardzo zaj�t%@2: %n%5%g)%n %b%3:%4%n\n"), 1);
	format_add("list_notavail", _("%> %1 %r(niedost�pn%@2)%n\n"), 1);
	format_add("list_notavail_descr", _("%> %1 %r(niedost�pn%@2: %n%5%r)%n\n"), 1);
	format_add("list_invisible", _("%> %1 %c(niewidoczn%@2)%n %b%3:%4%n\n"), 1);
	format_add("list_invisible_descr", _("%> %1 %c(niewidoczn%@2: %n%5%c)%n %b%3:%4%n\n"), 1);
	format_add("list_blocked", _("%> %1 %m(blokuj�c%@2)%n\n"), 1);
	format_add("list_unknown", "%> %1\n", 1);
	format_add("modify_offline", _("%> %1 nie b�dzie widzie� naszego stanu\n"), 1);
	format_add("modify_online", _("%> %1 b�dzie widzie� nasz stan\n"), 1);
	format_add("modify_done", _("%> Zmieniono wpis w li�cie kontakt�w\n"), 1);

	/* lista kontakt�w z boku ekranu */
	format_add("contacts_header", "", 1);
	format_add("contacts_header_group", "%K %1%n", 1);
	format_add("contacts_metacontacts_header", "", 1);
	format_add("contacts_avail_header", "", 1);
	format_add("contacts_avail", " %Y%1%n", 1);
	format_add("contacts_avail_descr", "%Ki%Y%1%n", 1);
	format_add("contacts_avail_descr_full", "%Ki%Y%1%n %2", 1);
        format_add("contacts_avail_blink", " %Y%i%1%n", 1);
        format_add("contacts_avail_descr_blink", "%K%ii%Y%i%1%n", 1);
        format_add("contacts_avail_descr_full_blink", "%K%ii%Y%i%1%n %2", 1);
	format_add("contacts_avail_footer", "", 1);
	format_add("contacts_away_header", "", 1);
	format_add("contacts_away", " %G%1%n", 1);
	format_add("contacts_away_descr", "%Ki%G%1%n", 1);
	format_add("contacts_away_descr_full", "%Ki%G%1%n %2", 1);
        format_add("contacts_away_blink", " %G%i%1%n", 1);
        format_add("contacts_away_descr_blink", "%K%ii%G%i%1%n", 1);
        format_add("contacts_away_descr_full_blink", "%K%ii%G%i%1%n %2", 1);
	format_add("contacts_away_footer", "", 1);
	format_add("contacts_dnd_header", "", 1);
	format_add("contacts_dnd", " %B%1%n", 1);
	format_add("contacts_dnd_descr", "%Ki%B%1%n", 1);
	format_add("contacts_dnd_descr_full", "%Ki%B%1%n %2", 1);
	format_add("contacts_dnd_blink", " %B%i%1%n", 1);
	format_add("contacts_dnd_descr_blink", "%K%ii%B%i%1%n", 1);
	format_add("contacts_dnd_descr_full_blink", "%K%ii%B%i%1%n %2", 1);
	format_add("contacts_dnd_footer", "", 1);
	format_add("contacts_chat_header", "", 1);
	format_add("contacts_chat", " %W%1%n", 1);
	format_add("contacts_chat_descr", "%Ki%W%1%n", 1);
	format_add("contacts_chat_descr_full", "%Ki%W%1%n %2", 1);
	format_add("contacts_chat_blink", " %W%i%1%n", 1);
	format_add("contacts_chat_descr_blink", "%K%ii%W%i%1%n", 1);
	format_add("contacts_chat_descr_full_blink", "%K%ii%W%i%1%n %2", 1);
	format_add("contacts_chat_footer", "", 1);
	format_add("contacts_error_header", "", 1);
	format_add("contacts_error", " %m%1%n", 1);
	format_add("contacts_error_descr", "%Ki%m%1%n", 1);
	format_add("contacts_error_descr_full", "%Ki%m%1%n %2", 1);
	format_add("contacts_error_blink", " %m%i%1%n", 1);
	format_add("contacts_error_descr_blink", "%K%ii%m%i%1%n", 1);
	format_add("contacts_error_descr_full_blink", "%K%ii%m%i%1%n %2", 1);
	format_add("contacts_error_footer", "", 1);
	format_add("contacts_xa_header", "", 1);
	format_add("contacts_xa", " %g%1%n", 1);
	format_add("contacts_xa_descr", "%Ki%g%1%n", 1);
	format_add("contacts_xa_descr_full", "%Ki%g%1%n %2", 1);
        format_add("contacts_xa_blink", " %g%i%1%n", 1);
        format_add("contacts_xa_descr_blink", "%K%ii%g%i%1%n", 1);
        format_add("contacts_xa_descr_full_blink", "%K%ii%g%i%1%n %2", 1);
	format_add("contacts_xa_footer", "", 1);
	format_add("contacts_notavail_header", "", 1);
	format_add("contacts_notavail", " %r%1%n", 1);
	format_add("contacts_notavail_descr", "%Ki%r%1%n", 1);
	format_add("contacts_notavail_descr_full", "%Ki%r%1%n %2", 1);
        format_add("contacts_notavail_blink", " %r%i%1%n", 1);
        format_add("contacts_notavail_descr_blink", "%K%ii%r%i%1%n", 1);
        format_add("contacts_notavail_descr_full_blink", "%K%ii%r%i%1%n %2", 1);
	format_add("contacts_notavail_footer", "", 1);
	format_add("contacts_invisible_header", "", 1);
	format_add("contacts_invisible", " %c%1%n", 1);
	format_add("contacts_invisible_descr", "%Ki%c%1%n", 1);
	format_add("contacts_invisible_descr_full", "%Ki%c%1%n %2", 1);
        format_add("contacts_invisible_blink", " %c%i%1%n", 1);
        format_add("contacts_invisible_descr_blink", "%K%ii%c%i%1%n", 1);
        format_add("contacts_invisible_descr_full_blink", "%K%ii%c%i%1%n %2", 1);
	format_add("contacts_invisible_footer", "", 1);
	format_add("contacts_blocking_header", "", 1);
	format_add("contacts_blocking", " %m%1%n", 1);
	format_add("contacts_blocking_footer", "", 1);
	format_add("contacts_footer", "", 1);
	format_add("contacts_footer_group", "", 1);
	format_add("contacts_metacontacts_footer", "", 1);
	format_add("contacts_vertical_line_char", "|", 1);
	format_add("contacts_horizontal_line_char", "-", 1);
		
	/* �egnamy si�, zapisujemy konfiguracj� */
	format_add("quit", _("%> Papa\n"), 1);
	format_add("quit_descr", _("%> Papa: %T%1%n%2\n"), 1);
	format_add("config_changed", _("Zapisa� now� konfiguracj�? (tak/nie) "), 1);
	format_add("quit_keep_reason", _("Ustawi�e� keep_reason, tak aby zachowywa� opisy.\nCzy chcesz zachowa� aktualny opis do pliku (przy nast�pnym uruchomieniu EKG opis ten zostanie przywr�cony)? (tak/nie) "), 1);
	format_add("saved", _("%> Zapisano ustawienia\n"), 1);
	format_add("error_saving", _("%! Podczas zapisu ustawie� wyst�pi� b��d\n"), 1);

	/* przychodz�ce wiadomo�ci */
	format_add("message", "%g.-- %n%1 %c%2%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
	format_add("message_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("message_timestamp_today", "(%H:%M) ", 1);
	format_add("message_timestamp_now", "", 1);
	format_add("message,speech", _("wiadomo�� od %1: %3."), 1);

        format_add("conference", "%g.-- %n%1 %c%2%g--- -- -%n\n%g|%n %|%3%n\n%|%g`----- ---- --- -- -%n\n", 1);
        format_add("conference_timestamp", "(%Y-%m-%d %H:%M) ", 1);
        format_add("conference_timestamp_today", "(%H:%M) ", 1);
        format_add("conference_timestamp_now", "", 1);
        format_add("confrence,speech", _("wiadomo�� od %1: %3."), 1);

	format_add("chat", "%c.-- %n%1 %c%2%c--- -- -%n\n%c|%n %|%3%n\n%|%c`----- ---- --- -- -%n\n", 1);
	format_add("chat_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("chat_timestamp_today", "(%H:%M) ", 1);
	format_add("chat_timestamp_now", "", 1);
	format_add("chat,speech", _("wiadomo�� od %1: %3."), 1);

	format_add("sent", "%b.-- %n%1 %c%2%b--- -- -%n\n%b|%n %|%3%n\n%|%b`----- ---- --- -- -%n\n", 1);
	format_add("sent_timestamp", "(%Y-%m-%d %H:%M) ", 1);
	format_add("sent_timestamp_today", "(%H:%M) ", 1);
	format_add("sent_timestamp_now", "", 1);
	format_add("sent,speech", "", 1);

	format_add("system", _("%m.-- %TWiadomo�� systemowa%m --- -- -%n\n%m|%n %|%3%n\n%|%m`----- ---- --- -- -%n\n"), 1);
	format_add("system,speech", _("wiadomo�� systemowa: %3."), 1);

	/* potwierdzenia wiadomo�ci */
	format_add("ack_queued", _("%> Wiadomo�� do %1 zostanie dostarczona p�niej\n"), 1);
	format_add("ack_delivered", _("%> Wiadomo�� do %1 zosta�a dostarczona\n"), 1);
	format_add("ack_unknown", _("%> Nie wiadomo, co si� sta�o z wiadomo�ci� do %1\n"), 1);
	format_add("ack_filtered", _("%! %|Wiadomo�� do %1 najprawdopodobniej nie zosta�a dostarczona, poniewa� dana osoba jest niedost�pna, a serwer twierdzi, �e dor�czy� wiadomo��. Sytuacja taka ma miejsce, gdy wiadomo�� zosta�a odrzucona przez filtry serwera (np. zawiera adres strony WWW)\n"), 1);
	format_add("message_too_long", _("%! Wiadomo�� jest zbyt d�uga i zosta�a skr�cona\n"), 1);

	/* ludzie zmieniaj� stan */
	format_add("status_avail", _("%> (%3) %1 jest %Ydost�pn%@2%n\n"), 1);
	format_add("status_avail_descr", _("%> (%3) %1 jest %Ydost�pn%@2%n: %T%4%n\n"), 1);
	format_add("status_away", _("%> (%3) %1 jest %Gzaj�t%@2%n\n"), 1);
	format_add("status_away_descr", _("%> (%3) %1 jest %Gzaj�t%@2%n: %T%4%n\n"), 1);
	format_add("status_notavail", _("%> (%3) %1 jest %rniedost�pn%@2%n\n"), 1);
	format_add("status_notavail_descr", _("%> (%3) %1 jest %rniedost�pn%@2%n: %T%4%n\n"), 1);
	format_add("status_invisible", _("%> (%3) %1 jest %cniewidoczn%@2%n\n"), 1);
	format_add("status_invisible_descr", _("%> (%3) %1 jest %cniewidoczn%@2%n: %T%4%n\n"), 1);
        format_add("status_xa", _("%> (%3) %1 jest %gbardzo zaj�t%@2%n\n"), 1);
	format_add("status_xa_descr", _("%> (%3) %1 jest %gbardzo zaj�t%@2%n: %T%4%n\n"), 1);
        format_add("status_dnd", _("%> (%3) %1 %Bnie przeszkadza�%n\n"), 1);
        format_add("status_dnd_descr", _("%> (%3) %1 %Bnie przeszkadza�%n: %T%4%n\n"), 1);
        format_add("status_error", _("%> (%3) %1 %mb��d pobierania statusu%n\n"), 1);
        format_add("status_error_descr", _("%> (%3) %1 %mb��d pobierania statusu%n: %T%4%n\n"), 1);
        format_add("status_chat", _("%> (%3) %1 jest %Wch�tn%@2 do rozmowy%n\n"), 1);
        format_add("status_chat_descr", _("%> (%3) %1 jest %Wch�tn%@2 do rozmowy%n: %T%4%n\n"), 1);

	/* po��czenie z serwerem */
	format_add("connecting", _("%> (%1) ��czenie z serwerem %n\n"), 1);
	format_add("conn_failed", _("%! (%2) Po��czenie nie powiod�o si�: %1%n\n"), 1);
	format_add("conn_failed_resolving", _("Nie znaleziono serwera"), 1);
	format_add("conn_failed_connecting", _("Nie mo�na po��czy� si� z serwerem"), 1);
	format_add("conn_failed_invalid", _("Nieprawid�owa odpowied� serwera"), 1);
	format_add("conn_failed_disconnected", _("Serwer zerwa� po��czenie"), 1);
	format_add("conn_failed_password", _("Nieprawid�owe has�o"), 1);
	format_add("conn_failed_404", _("B��d serwera HTTP"), 1);
	format_add("conn_failed_tls", _("B��d negocjacji TLS"), 1);
	format_add("conn_failed_memory", _("Brak pami�ci"), 1);
	format_add("conn_stopped", _("%! (%1) Przerwano ��czenie %n\n"), 1);
	format_add("conn_timeout", _("%! (%1) Przekroczono limit czasu operacji ��czenia z serwerem%n\n"), 1);
	format_add("connected", _("%> (%1) Po��czono%n\n"), 1);
	format_add("connected_descr", _("%> (%2) Po��czono: %T%1%n\n"), 1);
	format_add("disconnected", _("%> (%1) Roz��czono%n\n"), 1);
	format_add("disconnected_descr", _("%> (%2) Roz��czono: %T%1%n\n"), 1);
	format_add("already_connected", _("%! (%1) Klient jest ju� po��czony. Wpisz %Treconnect%n aby po��czy� ponownie%n\n"), 1);
	format_add("during_connect", _("%! (%1) ��czenie trwa. Wpisz %Tdisconnect%n aby przerwa�%n\n"), 1);
	format_add("conn_broken", _("%! (%1) Po��czenie zosta�o przerwane%n\n"), 1);
	format_add("conn_disconnected", _("%! (%1) Serwer zerwa� po��czenie%n\n"), 1);
	format_add("not_connected", _("%! (%1) Brak po��czenia z serwerem.%n\n"), 1);
	format_add("not_connected_msg_queued", _("%! (%1) Brak po��czenia z serwerem. Wiadomo�� b�dzie wys�ana po po��czeniu.%n\n"), 1);
	format_add("wrong_id", _("%! (%1) Z�y id sesji.%n\n"), 1);
	format_add("inet_addr_failed", _("%! (%1) B��dna warto�� zmiennej sesyjnej \"server\".%n\n"), 1);
	format_add("invalid_local_ip", _("%! (%1) Nieprawid�owy adres lokalny. Czyszcz� zmienn� sesyjn� %Tlocal_ip%n\n"), 1);

	/* obs�uga motyw�w */
	format_add("theme_loaded", "%> Wczytano motyw %T%1%n\n", 1);
	format_add("theme_default", "%> Ustawiono domy�lny motyw\n", 1);
	format_add("error_loading_theme", "%! B��d podczas �adowania motywu: %1\n", 1);

	/* zmienne, konfiguracja */
	format_add("variable", "%> %1 = %2\n", 1);
	format_add("variable_not_found", _("%! Nieznana zmienna: %T%1%n\n"), 1);
	format_add("variable_invalid", _("%! Nieprawid�owa warto�� zmiennej\n"), 1);
	format_add("no_config", _("%! Niekompletna konfiguracja. Wpisz:\n%!   %Tsession -a <gg:numerek-gg/jid:jabber-id>%n\n%!   %Tsession password <has�o>%n\n%!   %Tsave%n\n%! Nast�pnie wydaj polecenie:\n%!   %Tconnect%n\n%! Je�li nie masz swojego numerka, wpisz:\n%!   %Tregister <e-mail> <has�o>%n\n\n%> %|Po po��czeniu, nowe okna rozmowy b�d� tworzone automatycznie, gdy kto� przy�le wiadomo��. Aby przej�� do okna o podanym numerze nale�y wcisn�� %TAlt-numer%n lub %TEsc%n, a nast�pnie cyfr�. Aby rozpocz�� rozmow�, nale�y u�y� polecenia %Tquery%n. Aby doda� kogo� do listy kontakt�w, nale�y u�y� polecenia %Tadd%n. Wszystkie kombinacje klawiszy s� opisane w pliku %TREADME%n, a list� komend wy�wietla polecenie %Thelp%n. Pami�taj o prefixie przed ka�dym UID'em, prawid�owy przyk�adowy UID ma posta�: %Tgg:<nr>%n. \n\n"), 2);
	format_add("no_config,speech", _("niekompletna konfiguracja. wpisz session -a, a za tym gg: nummerek-gg, jid: ewentualnie d�aber id, a p, potem session pas�ord, za tym swoje has�o. wpisz sejf, �eby zapisa� ustawienia. wpisz konekt by si� po��czy�. je�li nie masz swojego numeru gadu-gadu, wpisz red�ister, a po spacji imejl i has�o. po po��czeniu, nowe okna rozmowy b�d� tworzone automatycznie, gdy kto� przy�le wiadomo��. aby przej�� do okna o podanym numerze, nale�y wcisn�� alt-numer lub eskejp, a nast�pnie cyfr�. aby rozpocz�� rozmow�, nale�y u�y� polecenia k�ery. aby doda� kogo� do listy kontakt�w, nale�y u�y� polecenia edd. wszystkie kombinacje klawiszy s� opisane w pliku ridmi, a list� komend wy�wietla polecenie help. Pami�taj o prefixie przed ka�dym uidem, prawid�owy przyk�adowy uid ma posta� gg:<nr>"), 1);
	format_add("error_reading_config", _("%! Nie mo�na odczyta� pliku konfiguracyjnego: %1\n"), 1);
	format_add("config_read_success", _("%> Wczytano plik konfiguracyjny %T%1%n\n"), 1);
        format_add("config_line_incorrect", _("%! Nieprawid�owa linia '%T%1%n', pomijam\n"), 1);
	format_add("autosaved", _("%> Automatycznie zapisano ustawienia\n"), 1);
	
	/* rejestracja nowego numeru */
	format_add("register", _("%> Rejestracja poprawna. Wygrany numerek: %T%1%n\n"), 1);
	format_add("register_failed", _("%! B��d podczas rejestracji: %1\n"), 1);
	format_add("register_pending", _("%! Rejestracja w toku\n"), 1);
	format_add("register_timeout", _("%! Przekroczono limit czasu operacji rejestrowania\n"), 1);
	format_add("registered_today", _("%! Ju� zarejestrowano jeden numer. Nie nadu�ywaj\n"), 1);

	/* kasowanie konta u�ytkownika z katalogu publiczengo */
	format_add("unregister", _("%> Konto %T%1%n zosta�o usuni�te\n"), 1);
	format_add("unregister_timeout", _("%! Przekroczono limit czasu operacji usuwania konta\n"), 1);
	format_add("unregister_bad_uin", _("%! Niepoprawny numer: %T%1%n\n"), 1);
	format_add("unregister_failed", _("%! B��d podczas usuwania konta: %1\n"), 1);
	
	/* przypomnienie has�a */
	format_add("remind", _("%> Has�o zosta�o wys�ane\n"), 1);
	format_add("remind_failed", _("%! B��d podczas wysy�ania has�a: %1\n"), 1);
	format_add("remind_timeout", _("%! Przekroczono limit czasu operacji wys�ania has�a\n"), 1);
	
	/* zmiana has�a */
	format_add("passwd", _("%> Has�o zosta�o zmienione\n"), 1);
	format_add("passwd_failed", _("%! B��d podczas zmiany has�a: %1\n"), 1);
	format_add("passwd_timeout", _("%! Przekroczono limit czasu operacji zmiany has�a\n"), 1);
	
	/* zmiana informacji w katalogu publicznym */
	format_add("change", _("%> Informacje w katalogu publicznym zosta�y zmienione\n"), 1);
	format_add("change_failed", _("%! B��d podczas zmiany informacji w katalogu publicznym\n"), 1);

	/* wyszukiwanie u�ytkownik�w */
	format_add("search_failed", _("%! Wyst�pi� b��d podczas szukania: %1\n"), 1);
	format_add("search_not_found", _("%! Nie znaleziono\n"), 1);
	format_add("search_no_last", _("%! Brak wynik�w ostatniego wyszukiwania\n"), 1);
	format_add("search_no_last_nickname", _("%! Brak pseudonimu w ostatnim wyszukiwaniu\n"), 1);
	format_add("search_stopped", _("%> Zatrzymano wyszukiwanie\n"), 1);

	/* 1 uin, 2 name, 3 nick, 4 city, 5 born, 6 gender, 7 active */
	format_add("search_results_multi_avail", "%Y<>%n", 1);
	format_add("search_results_multi_away", "%G<>%n", 1);
	format_add("search_results_multi_invisible", "%c<>%n", 1);
	format_add("search_results_multi_notavail", "  ", 1);
	format_add("search_results_multi_unknown", "-", 1);
/*	format_add("search_results_multi_female", "k", 1); */
/*	format_add("search_results_multi_male", "m", 1); */
	format_add("search_results_multi", "%7 %[-7]1 %K|%n %[12]3 %K|%n %[12]2 %K|%n %[4]5 %K|%n %[12]4\n", 1);

	format_add("search_results_single_avail", _("%Y(dost�pn%@1)%n"), 1);
	format_add("search_results_single_away", _("%G(zaj�t%@1)%n"), 1);
	format_add("search_results_single_notavail", _("%r(niedost�pn%@1)%n"), 1);
	format_add("search_results_single_invisible", _("%c(niewidoczn%@1)%n)"), 1);
	format_add("search_results_single_unknown", "%T-%n", 1);
/*	format_add("search_results_single_female", "%Mkobieta%n", 1); */
/*	format_add("search_results_single_male", "%Cm�czyzna%n", 1); */
	format_add("search_results_single", _("%) Pseudonim: %T%3%n\n%) Numerek: %T%1%n %7\n%) Imi� i nazwisko: %T%2%n\n%) Miejscowo��: %T%4%n\n%) Rok urodzenia: %T%5%n\n"), 1);

	/* exec */
	format_add("process", "%> %(-5)1 %2\n", 1);
	format_add("no_processes", _("%! Nie ma dzia�aj�cych proces�w\n"), 1);
	format_add("process_exit", _("%> Proces %1 (%2) zako�czy� dzia�anie z wynikiem %3\n"), 1);
	format_add("exec", "%1\n",1);	/* linie s� zako�czone \n */
	format_add("exec_error", _("%! B��d uruchamiania procesu: %1\n"), 1);
	format_add("exec_prompt", "$ %1\n", 1);

	/* szczeg�owe informacje o u�ytkowniku */
	format_add("user_info_header", "%K.--%n %T%1%n/%2 %K--- -- -%n\n", 1);
	format_add("user_info_nickname", _("%K| %nPseudonim: %T%1%n\n"), 1);
	format_add("user_info_name", _("%K| %nImi� i nazwisko: %T%1 %2%n\n"), 1);
	format_add("user_info_status", _("%K| %nStan: %T%1%n\n"), 1);
	format_add("user_info_status_time_format", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_status_time", _("%K| %nAktualny stan od: %T%1%n\n"), 1);
	format_add("user_info_auth_type", _("%K| %nRodzaj autoryzacji: %T%1%n\n"), 1);
	format_add("user_info_block", _("%K| %nBlokowan%@1\n"), 1);
	format_add("user_info_offline", _("%K| %nNie widzi stanu\n"), 1);
	format_add("user_info_not_in_contacts", _("%K| %nNie ma nas w swoich kontaktach\n"), 1);
	format_add("user_info_firewalled", _("%K| %nZnajduje si� za firewall/NAT\n"), 1);
	format_add("user_info_ip", _("%K| %nAdres: %T%1%n\n"), 1);
	format_add("user_info_mobile", _("%K| %nTelefon: %T%1%n\n"), 1);
	format_add("user_info_groups", _("%K| %nGrupy: %T%1%n\n"), 1);
	format_add("user_info_never_seen", _("%K| %nNigdy nie widziano\n"), 1);
	format_add("user_info_last_seen", _("%K| %nOstatnio widziano: %T%1%n\n"), 1);
	format_add("user_info_last_seen_time", "%Y-%m-%d %H:%M", 1);
	format_add("user_info_last_ip", _("%K| %nOstatni adres: %T%1%n\n"), 1);
	format_add("user_info_last_status", _("%K| %nOstatni stan: %T%1%n\n"), 1);

	format_add("user_info_footer", "%K`----- ---- --- -- -%n\n", 1);

	format_add("user_info_avail", _("%Ydost�pn%@1%n"), 1);
	format_add("user_info_avail_descr", _("%Ydost�pn%@1%n %K(%n%2%K)%n"), 1);
	format_add("user_info_away", _("%Gzaj�t%@1%n"), 1);
	format_add("user_info_away_descr", _("%Gzaj�t%@1%n %K(%n%2%K)%n"), 1);
	format_add("user_info_notavail", _("%rniedost�pn%@1%n"), 1);
	format_add("user_info_notavail_descr", _("%rniedost�pn%@1%n %K(%n%2%K)%n"), 1);
	format_add("user_info_invisible", _("%cniewidoczn%@1%n"), 1);
	format_add("user_info_invisible_descr", _("%cniewidoczn%@1%n %K(%n%2%K)%n"), 1);
        format_add("user_info_dnd", _("%Bnie przeszkadza�%n"), 1);
        format_add("user_info_dnd_descr", _("%Bnie przeszkadza�%n %K(%n%2%K)%n"), 1);
        format_add("user_info_chat", _("%Wch�tn%@1 do rozmowy%n"), 1);
	format_add("user_info_chat_descr", _("%Wch�tn%@1 do rozmowy%n %K(%n%2%K)%n"), 1);
	format_add("user_info_error", _("%m b��d%n"), 1);
	format_add("user_info_error_descr", _("%mb��d%n %K(%n%2%K)%n"), 1);
	format_add("user_info_xa", _("%gbardzo zaj�t%@1%n"), 1);
	format_add("user_info_xa_descr", _("%gbardzo zaj�t%@1%n %K(%n%2%K)%n"), 1);
	format_add("user_info_blocked", _("%mblokuj�c%@1%n"), 1);
	format_add("user_info_blocked_descr", _("%mblokuj�c%@1%n %K(%n%2%K)%n"), 1);
	format_add("user_info_unknown", _("%Mnieznany%n"), 1);

	/* grupy */
	format_add("group_members", _("%> %|Grupa %T%1%n: %2\n"), 1);
	format_add("group_member_already", _("%! %1 nale�y ju� do grupy %T%2%n\n"), 1);
	format_add("group_member_not_yet", _("%! %1 nie nale�y do grupy %T%2%n\n"), 1);
	format_add("group_empty", _("%! Grupa %T%1%n jest pusta\n"), 1);

	/* status */
	format_add("show_status_profile", _("%) Profil: %T%1%n\n"), 1);
	format_add("show_status_uid", "%) UID: %T%1%n\n", 1);
	format_add("show_status_uid_nick", "%) UID: %T%1%n (%T%2%n)\n", 1);
	format_add("show_status_status", _("%) Aktualny stan: %T%1%2%n\n"), 1);
	format_add("show_status_status_simple", _("%) Aktualny stan: %T%1%n\n"), 1);
	format_add("show_status_server", _("%) Aktualny serwer: %T%1%n:%T%2%n\n"), 1);
	format_add("show_status_server_tls", _("%) Aktualny serwer: %T%1%n:%T%2%Y (po��czenie szyfrowane)%n\n"), 1);
	format_add("show_status_connecting", _("%) Trwa ��czenie ..."), 1);
	format_add("show_status_avail", _("%Ydost�pny%n"), 1);
	format_add("show_status_avail_descr", _("%Ydost�pny%n (%T%1%n%2)"), 1);
	format_add("show_status_away", _("%Gzaj�ty%n"), 1);
	format_add("show_status_away_descr", _("%Gzaj�ty%n (%T%1%n%2)"), 1);
	format_add("show_status_invisible", _("%cniewidoczny%n"), 1);
	format_add("show_status_invisible_descr", _("%cniewidoczny%n (%T%1%n%2)"), 1);
	format_add("show_status_xa", _("%gbardzo zaj�ty%n"), 1);
	format_add("show_status_xa_descr", _("%gbardzo zaj�ty%n (%T%1%n%2)"), 1);
        format_add("show_status_dnd", _("%cnie przeszka�%n"), 1);
        format_add("show_status_dnd_descr", _("%cnie przeszkadza�%n (%T%1%n%2)"), 1);
	format_add("show_status_chat", _("%Wch�tny do rozmowy%n"), 1);
	format_add("show_status_chat_descr", _("%Wch�tny do rozmowy%n (%T%1%n%2)"), 1);
	format_add("show_status_notavail", _("%rniedost�pny%n"), 1);
	format_add("show_status_private_on", _(", tylko dla znajomych"), 1);
	format_add("show_status_private_off", "", 1);
	format_add("show_status_connected_since", _("%) Po��czony od: %T%1%n\n"), 1);
	format_add("show_status_disconnected_since", _("%) Roz��czony od: %T%1%n\n"), 1);
	format_add("show_status_last_conn_event", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_last_conn_event_today", "%H:%M", 1);
	format_add("show_status_ekg_started_since", _("%) Program dzia�a od: %T%1%n\n"), 1);
	format_add("show_status_ekg_started", "%Y-%m-%d %H:%M", 1);
	format_add("show_status_ekg_started_today", "%H:%M", 1);
	format_add("show_status_msg_queue", _("%) Ilo�� wiadomo�ci w kolejce do wys�ania: %T%1%n\n"), 1);

	/* aliasy */
	format_add("aliases_list_empty", _("%! Brak alias�w\n"), 1);
	format_add("aliases_list", "%> %T%1%n: %2\n", 1);
	format_add("aliases_list_next", "%> %3  %2\n", 1);
	format_add("aliases_add", _("%> Utworzono alias %T%1%n\n"), 1);
	format_add("aliases_append", _("%> Dodano do aliasu %T%1%n\n"), 1);
	format_add("aliases_del", _("%) Usuni�to alias %T%1%n\n"), 1);
	format_add("aliases_del_all", _("%) Usuni�to wszystkie aliasy\n"), 1);
	format_add("aliases_exist", _("%! Alias %T%1%n ju� istnieje\n"), 1);
	format_add("aliases_noexist", _("%! Alias %T%1%n nie istnieje\n"), 1);
	format_add("aliases_command", _("%! %T%1%n jest wbudowan� komend�\n"), 1);
	format_add("aliases_not_enough_params", _("%! Alias %T%1%n wymaga wi�kszej ilo�ci parametr�w\n"), 1);

	/* po��czenia bezpo�rednie */
	format_add("dcc_attack", _("%! %|Program otrzyma� zbyt wiele ��da� bezpo�rednich po��cze�, ostatnie od %1\n"), 1);
	format_add("dcc_limit", _("%! %|Przekroczono limit bezpo�rednich po��cze� i dla bezpiecze�stwa zosta�y one wy��czone. Aby je w��czy� ponownie, nale�y wpisa� polecenie %Tset dcc 1%n i po��czy� si� ponownie. Limit mo�na zmieni� za pomoc� zmiennej %Tdcc_limit%n.\n"), 1);
	format_add("dcc_create_error", _("%! Nie mo�na w��czy� po��cze� bezpo�rednich: %1\n"), 1);
	format_add("dcc_error_network", _("%! B��d transmisji z %1\n"), 1);
	format_add("dcc_error_refused", _("%! Po��czenie z %1 zosta�o odrzucone\n"), 1);
	format_add("dcc_error_unknown", _("%! Nieznany b��d po��czenia bezpo�redniego\n"), 1);
	format_add("dcc_error_handshake", _("%! Nie mo�na nawi�za� po��czenia z %1\n"), 1);
	format_add("dcc_user_aint_dcc", _("%! %1 nie ma w��czonej obs�ugi po��cze� bezpo�rednich\n"), 1);
	format_add("dcc_timeout", _("%! Przekroczono limit czasu operacji bezpo�redniego po��czenia z %1\n"), 1);
	format_add("dcc_not_supported", _("%! Opcja %T%1%n nie jest jeszcze obs�ugiwana\n"), 1);
	format_add("dcc_open_error", _("%! Nie mo�na otworzy� %T%1%n: %2\n"), 1);
	format_add("dcc_show_pending_header", _("%> Po��czenia oczekuj�ce:\n"), 1);
	format_add("dcc_show_pending_send", _("%) #%1, %2, wysy�anie %T%3%n\n"), 1);
	format_add("dcc_show_pending_get", _("%) #%1, %2, odbi�r %T%3%n\n"), 1);
	format_add("dcc_show_pending_voice", _("%) #%1, %2, rozmowa\n"), 1);
	format_add("dcc_show_active_header", _("%> Po��czenia aktywne:\n"), 1);
	format_add("dcc_show_active_send", _("%) #%1, %2, wysy�anie %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_get", _("%) #%1, %2, odbi�r %T%3%n, %T%4b%n z %T%5b%n (%6%%)\n"), 1);
	format_add("dcc_show_active_voice", _("%) #%1, %2, rozmowa\n"), 1);
	format_add("dcc_show_empty", _("%! Brak bezpo�rednich po��cze�\n"), 1);
	format_add("dcc_receiving_already", _("%! Plik %T%1%n od u�ytkownika %2 jest ju� pobierany\n"), 1);

	format_add("dcc_done_get", _("%> Zako�czono pobieranie pliku %T%2%n od %1\n"), 1);
	format_add("dcc_done_send", _("%> Zako�czono wysy�anie pliku %T%2%n do %1\n"), 1);
	format_add("dcc_close", _("%) Zamkni�to po��czenie z %1\n"), 1);

	format_add("dcc_voice_offer", _("%) %1 chce rozmawia�\n%) Wpisz %Tdcc voice #%2%n, by rozpocz�� rozmow�, lub %Tdcc close #%2%n, by anulowa�\n"), 1);
	format_add("dcc_voice_running", _("%! Mo�na prowadzi� tylko jedn� rozmow� g�osow� na raz\n"), 1);
	format_add("dcc_voice_unsupported", _("%! Nie wkompilowano obs�ugi rozm�w g�osowych. Przeczytaj %Tdocs/voip.txt%n\n"), 1);
	format_add("dcc_get_offer", _("%) %1 przesy�a plik %T%2%n o rozmiarze %T%3b%n\n%) Wpisz %Tdcc get #%4%n, by go odebra�, lub %Tdcc close #%4%n, by anulowa�\n"), 1);
	format_add("dcc_get_offer_resume", _("%) Plik istnieje ju� na dysku, wi�c mo�na wznowi� pobieranie poleceniem %Tdcc resume #%4%n\n"), 1);
	format_add("dcc_get_getting", _("%) Rozpocz�to pobieranie pliku %T%2%n od %1\n"), 1);
	format_add("dcc_get_cant_create", _("%! Nie mo�na utworzy� pliku %T%1%n\n"), 1);
	format_add("dcc_not_found", _("%! Nie znaleziono po��czenia %T%1%n\n"), 1);
	format_add("dcc_invalid_ip", _("%! Nieprawid�owy adres IP\n"), 1);
	format_add("dcc_user_notavail", _("%! %1 musi by� dost�pn%@1, by m�c nawi�za� po��czenie\n"), 1);

	/* query */
	format_add("query_started", _("%) (%2) Rozpocz�to rozmow� z %T%1%n\n"), 1);
	format_add("query_started_window", _("%) Wci�nij %TAlt-G%n by ignorowa�, %TAlt-K%n by zamkn�� okno\n"), 1);
	format_add("query_finished", _("%) (%2) Zako�czono rozmow� z %T%1%n\n"), 1);
	format_add("query_exist", _("%! Rozmowa z %T%1%n jest ju� prowadzona w okienku nr %T%2%n\n"), 1);

	/* zdarzenia */
        format_add("events_list_empty", _("%! Brak zdarze�\n"), 1);
	format_add("events_list_header", "", 1);
        format_add("events_list", "%> %5 on %1 %3 %4 - prio %2\n", 1);
        format_add("events_add", _("%> Dodano zdarzenie %T%1%n\n"), 1);
        format_add("events_del", _("%) Usuni�to zdarzenie %T%1%n\n"), 1);
        format_add("events_del_all", _("%) Usuni�to wszystkie zdarzenia\n"), 1);
        format_add("events_exist", _("%! Zdarzenie %T%1%n istnieje dla %2\n"), 1);
        format_add("events_del_noexist", _("%! Zdarzenie %T%1%n nie istnieje\n"), 1);

	/* lista kontakt�w z serwera */
	format_add("userlist_put_ok", _("%> List� kontakt�w zachowano na serwerze\n"), 1);
	format_add("userlist_put_error", _("%! B��d podczas wysy�ania listy kontakt�w\n"), 1);
	format_add("userlist_get_ok", _("%> List� kontakt�w wczytano z serwera\n"), 1);
	format_add("userlist_get_error", _("%! B��d podczas pobierania listy kontakt�w\n"), 1);
	format_add("userlist_clear_ok", _("%) Usuni�to list� kontakt�w z serwera\n"), 1);
	format_add("userlist_clear_error", _("%! B��d podczas usuwania listy kontakt�w\n"), 1);

	/* szybka lista kontakt�w pod F2 */
	format_add("quick_list", "%)%1\n", 1);
	format_add("quick_list,speech", _("lista kontakt�w: "), 1);
	format_add("quick_list_avail", " %Y%1%n", 1);
	format_add("quick_list_avail,speech", _("%1 jest dost�pny, "), 1);
	format_add("quick_list_away", " %G%1%n", 1);
	format_add("quick_list_away,speech", _("%1 jest zaj�ty, "), 1);
	format_add("quick_list_invisible", " %c%1%n", 1);

	/* window */
	format_add("window_add", _("%) Utworzono nowe okno\n"), 1);
	format_add("window_noexist", _("%! Wybrane okno nie istnieje\n"), 1);
	format_add("window_doesnt_exist", _("%! Okno %T%1%n nie istnieje\n"), 1);
	format_add("window_no_windows", _("%! Nie mo�na zamkn�� ostatniego okna\n"), 1);
	format_add("window_del", _("%) Zamkni�to okno\n"), 1);
	format_add("windows_max", _("%! Wyczerpano limit ilo�ci okien\n"), 1);
	format_add("window_list_query", _("%) %1: rozmowa z %T%2%n\n"), 1);
	format_add("window_list_nothing", _("%) %1: brak rozmowy\n"), 1);
	format_add("window_list_floating", _("%) %1: p�ywaj�ce %4x%5 w %2,%3 %T%6%n\n"), 1);
	format_add("window_id_query_started", _("%) Rozmowa z %T%2%n rozpocz�ta w oknie %T%1%n\n"), 1);
	format_add("window_kill_status", _("%! Nie mo�na zamkn�� okna stanu\n"), 1);

	/* bind */
	format_add("bind_seq_incorrect", _("%! Sekwencja %T%1%n jest nieprawid�owa\n"), 1); 
	format_add("bind_seq_add", _("%> Dodano sekwencj� %T%1%n\n"), 1);
	format_add("bind_seq_remove", _("%) Usuni�to sekwencj� %T%1%n\n"), 1);	
	format_add("bind_seq_list", "%> %1: %T%2%n\n", 1);
	format_add("bind_seq_exist", _("%! Sekwencja %T%1%n ma ju� przypisan� akcj�\n"), 1);
	format_add("bind_seq_list_empty", _("%! Brak przypisanych akcji\n"), 1);
	format_add("bind_doesnt_exist", _("%! Nie mo�na znale�� kombinacji %T%1%n\n"), 1);
	format_add("bind_press_key", _("%! Naci�nij klawisz(e), kt�re maj� by� podbindowane\n"), 1);
	format_add("bind_added", _("%> Bindowanie zako�czono pomy�lnie\n"), 1);

	/* at */
	format_add("at_list", "%> %1, %2, %3 %K(%4)%n %5\n", 1);
	format_add("at_added", _("%> Utworzono plan %T%1%n\n"), 1);
	format_add("at_deleted", _("%) Usuni�to plan %T%1%n\n"), 1);
	format_add("at_deleted_all", _("%) Usuni�to plany u�ytkownika\n"), 1);
	format_add("at_exist", _("%! Plan %T%1%n ju� istnieje\n"), 1);
	format_add("at_noexist", _("%! Plan %T%1%n nie istnieje\n"), 1);
	format_add("at_empty", _("%! Brak plan�w\n"), 1);
	format_add("at_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("at_back_to_past", _("%! Gdyby mo�na by�o cofn�� czas...\n"), 1);

	/* timer */
	format_add("timer_list", "%> %1, %2s, %3 %K(%4)%n %T%5%n\n", 1);
	format_add("timer_added", _("%> Utworzono timer %T%1%n\n"), 1);
	format_add("timer_deleted", _("%) Usuni�to timer %T%1%n\n"), 1);
	format_add("timer_deleted_all", _("%) Usuni�to timery u�ytkownika\n"), 1);
	format_add("timer_exist", _("%! Timer %T%1%n ju� istnieje\n"), 1);
	format_add("timer_noexist", _("%! Timer %T%1%n nie istnieje\n"), 1);
	format_add("timer_empty", _("%! Brak timer�w\n"), 1);

	/* last */
	format_add("last_list_in", "%) %Y <<%n [%1] %2 %3\n", 1);
	format_add("last_list_out", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("last_list_empty", _("%! Nie zalogowano �adnych wiadomo�ci\n"), 1);
	format_add("last_list_empty_nick", _("%! Nie zalogowano �adnych wiadomo�ci dla %T%1%n\n"), 1);
	format_add("last_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("last_list_timestamp_today", "%H:%M", 1);
	format_add("last_clear_uin", _("%) Wiadomo�ci dla %T%1%n wyczyszczone\n"), 1);
	format_add("last_clear", _("%) Wszystkie wiadomo�ci wyczyszczone\n"), 1);

	/* queue */
	format_add("queue_list_timestamp", "%d-%m-%Y %H:%M", 1);
	format_add("queue_list_message", "%) %G >>%n [%1] %2 %3\n", 1);
	format_add("queue_clear", _("%) Kolejka wiadomo�ci wyczyszczona\n"), 1);
	format_add("queue_clear_uid", _("%) Kolejka wiadomo�ci wyczyszczona dla %T%1%n\n"), 1);
	format_add("queue_wrong_use", _("%! Komenda dzia�a tylko przy braku po��czenia z serwerem\n"), 1);
	format_add("queue_empty", _("%! Kolejka wiadomo�ci jest pusta\n"), 1);
	format_add("queue_empty_uid", _("%! Brak wiadomo�ci w kolejce dla %T%1%n\n"), 1);
	format_add("queue_flush", _("%> (%1) Wys�ano zaleg�e wiadomo�ci z kolejki\n"), 1);

	/* conference */
	format_add("conferences_list_empty", _("%! Brak konferencji\n"), 1);
	format_add("conferences_list", "%> %T%1%n: %2\n", 1);
	format_add("conferences_list_ignored", _("%> %T%1%n: %2 (%yingorowana%n)\n"), 1);
	format_add("conferences_add", _("%> Utworzono konferencj� %T%1%n\n"), 1);
	format_add("conferences_not_added", _("%! Nie utworzono konferencji %T%1%n\n"), 1);
	format_add("conferences_del", _("%) Usuni�to konferencj� %T%1%n\n"), 1);
	format_add("conferences_del_all", _("%) Usuni�to wszystkie konferencje\n"), 1);
	format_add("conferences_exist", _("%! Konferencja %T%1%n ju� istnieje\n"), 1);
	format_add("conferences_noexist", _("%! Konferencja %T%1%n nie istnieje\n"), 1);
	format_add("conferences_name_error", _("%! Nazwa konferencji powinna zaczyna� si� od %T#%n\n"), 1);
	format_add("conferences_rename", _("%> Nazwa konferencji zmieniona: %T%1%n --> %T%2%n\n"), 1);
	format_add("conferences_ignore", _("%> Konferencja %T%1%n b�dzie ignorowana\n"), 1);
	format_add("conferences_unignore", _("%> Konferencja %T%1%n nie b�dzie ignorowana\n"), 1);
	format_add("conferences_joined", _("%> Do��czono %1 do konferencji %T%2%n\n"), 1);
	format_add("conferences_already_joined", _("%> %1 uczestniczy ju� w konferencji %T%2%n\n"), 1);
	
	/* wsp�lne dla us�ug http */
	format_add("http_failed_resolving", _("Nie znaleziono serwera"), 1);
	format_add("http_failed_connecting", _("Nie mo�na po��czy� si� z serwerem"), 1);
	format_add("http_failed_reading", _("Serwer zerwa� po��czenie"), 1);
	format_add("http_failed_writing", _("Serwer zerwa� po��czenie"), 1);
	format_add("http_failed_memory", _("Brak pami�ci"), 1);

#ifdef WITH_PYTHON
	/* python */
	format_add("python_list", "%> %1\n", 1);
	format_add("python_list_empty", _("%! Brak za�adowanych skrypt�w\n"), 1);
	format_add("python_removed", _("%) Skrypt zosta� usuni�ty\n", 1);
	format_add("python_need_name", _("%! Nie podano nazwy skryptu\n", 1);
	format_add("python_not_found", _("%! Nie znaleziono skryptu %T%1%n\n", 1);
	format_add("python_wrong_location", _("%! Skrypt nale�y umie�ci� w katalogu %T%1%n\n", 1);
#endif
	format_add("session_name", "%B%1%n", 1);
	format_add("session_variable", "%> %T%1->%2 = %R%3%n\n", 1); /* uid, var, new_value*/
	format_add("session_variable_removed", _("%> Usuni�to %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_variable_doesnt_exist", _("%! Nieznana zmienna: %T%1->%2%n\n"), 1); /* uid, var */
	format_add("session_list", "%> %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_list_alias", "%> %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_list_empty", _("%! Lista sesji jest pusta\n"), 1);
	format_add("session_info_header", "%) %T%1%n %3\n", 1); /* uid, uid, %{user_info_*} */
	format_add("session_info_header_alias", "%) %T%2%n/%1 %3\n", 1); /* uid, alias, %{user_info_*} */
	format_add("session_info_param", "%)    %1 = %T%2%n\n", 1); /* key, value */
	format_add("session_info_footer", "", 1); /* uid */
	format_add("session_exists", _("%! Sesja %T%1%n ju� istnieje\n"), 1); /* uid */
	format_add("session_doesnt_exist", _("%! Sesja %T%1%n nie istnieje\n"), 1); /* uid */
	format_add("session_added", _("%> Utworzono sesj� %T%1%n\n"), 1); /* uid */
	format_add("session_removed", _("%> Usuni�to sesj� %T%1%n\n"), 1); /* uid */
	format_add("session_format", "%T%1%n", 1);
	format_add("session_format_alias", "%T%1%n/%2", 1);
	format_add("session_cannot_change", _("%! Nie mo�na zmieni� sesji w okienku rozmowy%n\n"), 1);

	format_add("metacontact_list", "%> %T%1%n", 1);
        format_add("metacontact_list_empty", "%! Nie ma �adnych metakontakt�w\n", 1);
	format_add("metacontact_exists", "%! Metakontakt %T%1%n ju� istnieje\n", 1);
        format_add("metacontact_added", "%> Utworzono metakontakt %T%1%n\n", 1);
        format_add("metacontact_removed", "%> Usuni�to metakontakt %T%1%n\n", 1);
	format_add("metacontact_doesnt_exist", "%! Metakontakt %T%1%n nie istnieje\n", 1);
        format_add("metacontact_added_item", "%> Dodano %T%1/%2%n do %T%3%n\n", 1);
	format_add("metacontact_removed_item", "%> Usuni�to %T%1/%2%n z %T%3%n\n", 1);
	format_add("metacontact_item_list_header", "", 1);
        format_add("metacontact_item_list", "%> %T%1/%2 (%3)%n - prio %T%4%n\n", 1);
        format_add("metacontact_item_list_empty", "%! Metakontakt jest pusty\n", 1);
	format_add("metacontact_item_list_footer", "", 1);
        format_add("metacontact_item_doesnt_exist", "%! Kontakt %T%1/%2%n nie istnieje\n", 1);
        format_add("metacontact_info_header", "%K.--%n Metakontakt %T%1%n %K--- -- -%n\n", 1);
        format_add("metacontact_info_status", "%K| %nStan: %T%1%n\n", 1);
        format_add("metacontact_info_footer", "%K`----- ---- --- -- -%n\n", 1);

        format_add("metacontact_info_avail", _("%Ydost�pn%@1%n"), 1);
        format_add("metacontact_info_avail_descr", _("%Ydost�pn%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_away", _("%Gzaj�t%@1%n"), 1);
        format_add("metacontact_info_away_descr", _("%Gzaj�t%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_notavail", _("%rniedost�pn%@1%n"), 1);
        format_add("metacontact_info_notavail_descr", _("%rniedost�pn%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_invisible", _("%cniewidoczn%@1%n"), 1);
        format_add("metacontact_info_invisible_descr", _("%cniewidoczn%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_dnd", _("%Bnie przeszkadza�%n"), 1);
        format_add("metacontact_info_dnd_descr", _("%Bnie przeszkadza�%n %K(%n%2%K)%n"), 1);
	format_add("metacontact_info_chat", _("%Wch�tn%@1 do rozmowy%n"), 1);
        format_add("metacontact_info_chat_descr", _("%Wch�tn%@1 do rozmowy%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_error", _("%mb��d%n"), 1);
        format_add("metacontact_info_error_descr", _("%mb��d%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_xa", _("%gbardzo zaj�t%@1%n"), 1);
        format_add("metacontact_info_xa_descr", _("%gbardzo zaj�t%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_blocked", _("%mblokuj�c%@1%n"), 1);
        format_add("metacontact_info_blocked_descr", _("%mblokuj�c%@1%n %K(%n%2%K)%n"), 1);
        format_add("metacontact_info_unknown", _("%Mnieznany%n"), 1);

	format_add("plugin_already_loaded", _("%! Plugin %T%1%n jest ju� za�adowany%n.\n"), 1);
	format_add("plugin_doesnt_exist", _("%! Plugin %T%1%n nie mo�e zosta� znaleziony%n\n"), 1);
	format_add("plugin_incorrect", _("%! Plugin %T%1%n nie jest prawid�owym pluginem EKG2%n\n"), 1);
	format_add("plugin_not_initialized", _("%! Plugin %T%1%n nie zosta� wczytany poprawnie%n\n"), 1);
	format_add("plugin_unload_ui", _("%! Plugin %T%1%n jest pluginem UI i nie mo�e zosta� wy�adowany%n\n"), 1);
	format_add("plugin_loaded", _("%> Plugin %T%1%n zosta� za�adowany%n\n"), 1);
	format_add("plugin_unloaded", _("%> Plugin %T%1%n zosta� wy�adowany%n\n"), 1);

	theme_plugins_init();
}

