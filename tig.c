/* Copyright (c) 2006-2009 Jonas Fonseca <fonseca@diku.dk>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
enum input_status {
	INPUT_OK,
	INPUT_SKIP,
	INPUT_STOP,
	INPUT_CANCEL
};

typedef enum input_status (*input_handler)(void *data, char *buf, int c);

static char *prompt_input(const char *prompt, input_handler handler, void *data);
static bool prompt_yesno(const char *prompt);
static void
argv_from_env(const char **argv, const char *name)
{
	char *env = argv ? getenv(name) : NULL;
	int argc = 0;

	if (env && *env)
		env = strdup(env);
	if (env && !argv_from_string(argv, &argc, env))
		die("Too many arguments in the `%s` environment variable", name);
}

	IO_BG,			/* Execute command in the background. */
	IO_AP,			/* Append fork+exec output to file. */
	pid_t pid;		/* Pipe for reading or writing. */
	int pipe;		/* Pipe end for reading or writing. */
	const char *argv[SIZEOF_ARG];	/* Shell command arguments. */
	char *buf;		/* Read buffer. */
	size_t bufsize;		/* Buffer content size. */
	char *bufpos;		/* Current buffer position. */
	unsigned int eof:1;	/* Has end of file been reached. */
	io->pipe = -1;
	io->pid = 0;
	io->buf = io->bufpos = NULL;
	io->bufalloc = io->bufsize = 0;
	io->eof = 0;
init_io_rd(struct io *io, const char *argv[], const char *dir,
		enum format_flags flags)
{
	init_io(io, dir, IO_RD);
	return format_argv(io->argv, argv, flags);
}

static bool
io_open(struct io *io, const char *name)
	io->pipe = *name ? open(name, O_RDONLY) : STDIN_FILENO;
	return io->pipe != -1;
}

static bool
kill_io(struct io *io)
{
	return io->pid == 0 || kill(io->pid, SIGKILL) != -1;
	pid_t pid = io->pid;

	if (io->pipe != -1)
		close(io->pipe);

	while (pid > 0) {
		int status;
		pid_t waiting = waitpid(pid, &status, 0);

		if (waiting < 0) {
			if (errno == EINTR)
				continue;
			report("waitpid failed (%s)", strerror(errno));
			return FALSE;
		}

		return waiting == pid &&
		       !WIFSIGNALED(status) &&
		       WIFEXITED(status) &&
		       !WEXITSTATUS(status);
	}

	int pipefds[2] = { -1, -1 };
	if (io->type == IO_FD)
		return TRUE;
	if ((io->type == IO_RD || io->type == IO_WR) &&
	    pipe(pipefds) < 0)
	else if (io->type == IO_AP)
		pipefds[1] = io->pipe;

	if ((io->pid = fork())) {
		if (pipefds[!(io->type == IO_WR)] != -1)
			close(pipefds[!(io->type == IO_WR)]);
		if (io->pid != -1) {
			io->pipe = pipefds[!!(io->type == IO_WR)];
			return TRUE;
		}

	} else {
		if (io->type != IO_FG) {
			int devnull = open("/dev/null", O_RDWR);
			int readfd  = io->type == IO_WR ? pipefds[0] : devnull;
			int writefd = (io->type == IO_RD || io->type == IO_AP)
							? pipefds[1] : devnull;

			dup2(readfd,  STDIN_FILENO);
			dup2(writefd, STDOUT_FILENO);
			dup2(devnull, STDERR_FILENO);

			close(devnull);
			if (pipefds[0] != -1)
				close(pipefds[0]);
			if (pipefds[1] != -1)
				close(pipefds[1]);
		}
		if (io->dir && *io->dir && chdir(io->dir) == -1)
			die("Failed to change directory: %s", strerror(errno));
		execvp(io->argv[0], (char *const*) io->argv);
		die("Failed to execute program: %s", strerror(errno));
	}

	if (pipefds[!!(io->type == IO_WR)] != -1)
		close(pipefds[!!(io->type == IO_WR)]);
	return FALSE;
run_io(struct io *io, const char **argv, const char *dir, enum io_type type)
	init_io(io, dir, type);
	if (!format_argv(io->argv, argv, FORMAT_NONE))
		return FALSE;
static int
run_io_bg(const char **argv)
{
	struct io io = {};

	init_io(&io, NULL, IO_BG);
	if (!format_argv(io.argv, argv, FORMAT_NONE))
		return FALSE;
	return run_io_do(&io);
}

	if (!format_argv(io.argv, argv, FORMAT_NONE))
run_io_append(const char **argv, enum format_flags flags, int fd)
	struct io io = {};
	init_io(&io, NULL, IO_AP);
	io.pipe = fd;
	if (format_argv(io.argv, argv, flags))
		return run_io_do(&io);
	close(fd);
	return FALSE;
}
static bool
run_io_rd(struct io *io, const char **argv, enum format_flags flags)
{
	return init_io_rd(io, argv, NULL, flags) && start_io(io);
	return io->eof;
static bool
io_can_read(struct io *io)
{
	struct timeval tv = { 0, 500 };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(io->pipe, &fds);

	return select(io->pipe + 1, &fds, NULL, NULL, &tv) > 0;
}

static ssize_t
io_read(struct io *io, void *buf, size_t bufsize)
{
	do {
		ssize_t readsize = read(io->pipe, buf, bufsize);

		if (readsize < 0 && (errno == EAGAIN || errno == EINTR))
			continue;
		else if (readsize == -1)
			io->error = errno;
		else if (readsize == 0)
			io->eof = 1;
		return readsize;
	} while (1);
}

io_get(struct io *io, int c, bool can_read)
	char *eol;
	ssize_t readsize;

		io->buf = io->bufpos = malloc(BUFSIZ);
		io->bufsize = 0;
	}

	while (TRUE) {
		if (io->bufsize > 0) {
			eol = memchr(io->bufpos, c, io->bufsize);
			if (eol) {
				char *line = io->bufpos;

				*eol = 0;
				io->bufpos = eol + 1;
				io->bufsize -= io->bufpos - line;
				return line;
			}
		}

		if (io_eof(io)) {
			if (io->bufsize) {
				io->bufpos[io->bufsize] = 0;
				io->bufsize = 0;
				return io->bufpos;
			}
			return NULL;
		}

		if (!can_read)
			return NULL;

		if (io->bufsize > 0 && io->bufpos > io->buf)
			memmove(io->buf, io->bufpos, io->bufsize);

		io->bufpos = io->buf;
		readsize = io_read(io, io->buf + io->bufsize, io->bufalloc - io->bufsize);
		if (io_error(io))
			return NULL;
		io->bufsize += readsize;
}

static bool
io_write(struct io *io, const void *buf, size_t bufsize)
{
	size_t written = 0;

	while (!io_error(io) && written < bufsize) {
		ssize_t size;
		size = write(io->pipe, buf + written, bufsize - written);
		if (size < 0 && (errno == EAGAIN || errno == EINTR))
			continue;
		else if (size == -1)
		else
			written += size;
	return written == bufsize;
}

static bool
run_io_buf(const char **argv, char buf[], size_t bufsize)
{
	struct io io = {};
	bool error;

	if (!run_io_rd(&io, argv, FORMAT_NONE))
		return FALSE;

	io.buf = io.bufpos = buf;
	io.bufalloc = bufsize;
	error = !io_get(&io, '\n', TRUE) && io_error(&io);
	io.buf = NULL;

	return done_io(&io) || error;
static int read_properties(struct io *io, const char *separators, int (*read)(char *, size_t, char *, size_t));
	REQ_(PARENT,		"Move to parent"), \
static char opt_prefix[SIZEOF_STR]	= "";
parse_options(int argc, const char *argv[], const char ***run_argv)
	/* XXX: This is vulnerable to the user overriding options
	 * required for the main view parser. */
	static const char *custom_argv[SIZEOF_ARG] = {
		"git", "log", "--no-color", "--pretty=raw", "--parents",
			"--topo-order", NULL
	};
	int i, j = 6;
	if (!isatty(STDIN_FILENO))
	if (subcommand) {
		custom_argv[1] = subcommand;
		j = 2;
	}
		custom_argv[j++] = opt;
		if (j >= ARRAY_SIZE(custom_argv))
	custom_argv[j] = NULL;
	*run_argv = custom_argv;
LINE(TREE_PARENT,  "",			COLOR_DEFAULT,	COLOR_DEFAULT,	A_BOLD), \
LINE(TREE_MODE,    "",			COLOR_CYAN,	COLOR_DEFAULT,	0), \
LINE(TREE_DIR,     "",			COLOR_YELLOW,	COLOR_DEFAULT,	A_NORMAL), \
	unsigned int cleareol:1;
	{ ',',		REQ_PARENT },
		struct {
			const char *name;
			enum request request;
		} obsolete[] = {
			{ "cherry-pick",	REQ_NONE },
			{ "screen-resize",	REQ_NONE },
			{ "tree-parent",	REQ_PARENT },
		};
			if (namelen != strlen(obsolete[i].name) ||
			    string_enum_compare(obsolete[i].name, argv[2], namelen))
				continue;
			if (obsolete[i].request != REQ_NONE)
				add_keybinding(keymap, obsolete[i].request, key);
			config_msg = "Obsolete request name";
			return ERR;
	struct io io = {};
	if (!io_open(&io, path))
	if (read_properties(&io, " \t", read_option) == ERR ||
	unsigned long p_offset;	/* Previous offset of the window top */
	unsigned long p_lineno;	/* Previous current line number */
	bool p_restore;		/* Should the previous position be restored. */
	time_t update_secs;
	/* Default command arguments. */
	const char **argv;
static struct view_ops diff_ops;
#define VIEW_STR(name, env, ref, ops, map, git) \
	{ name, #env, ref, ops, map, git }
	VIEW_STR(name, TIG_##id##_CMD, ref, ops, KEYMAP_##id, git)
	VIEW_(DIFF,   "diff",   &diff_ops,   TRUE,  ref_commit),
	if (line->cleareol)
		wclrtoeol(view->win);
	line->dirty = line->cleareol = 0;
		if (view->offset + lineno >= view->lines)
			break;
		if (!view->line[view->offset + lineno].dirty)
	werase(view->win);
	if (view != VIEW(REQ_VIEW_STATUS) && view->lines) {
		string_format_from(state, &statelen, " - %s %d of %d (%d%%)",
	}
	if (view->pipe) {
		time_t secs = time(NULL) - view->start_time;

		/* Three git seconds are a long time ... */
		if (secs > 2)
			string_format_from(state, &statelen, " loading %lds", secs);
		string_format_from(buf, &bufpos, "%s", state);
redraw_display(bool clear)
		if (clear)
			wclear(view->win);
static void
toggle_view_option(bool *option, const char *help)
{
	*option = !*option;
	redraw_display(FALSE);
	report("%sabling %s", *option ? "En" : "Dis", help);
}

static void
select_view_line(struct view *view, unsigned long lineno)
		if (view_is_displayed(view))
			redraw_view(view);
		if (view_is_displayed(view)) {
			draw_view_line(view, old_lineno);
			draw_view_line(view, view->lineno - view->offset);
			redrawwin(view->win);
			wrefresh(view->win);
		} else {
			view->ops->select(view, &view->line[view->lineno]);
		}
		if (view->ops->grep(view, &view->line[lineno])) {
			select_view_line(view, lineno);
			report("Line %ld matches '%s'", lineno + 1, view->grep);
		}
	view->p_offset = view->offset;
	view->p_lineno = view->lineno;

	view->update_secs = 0;
restore_view_position(struct view *view)
	if (!view->p_restore || (view->pipe && view->lines <= view->p_lineno))
		return FALSE;
	/* Changing the view position cancels the restoring. */
	/* FIXME: Changing back to the first line is not detected. */
	if (view->offset != 0 || view->lineno != 0) {
		view->p_restore = FALSE;
	if (view->p_lineno >= view->lines) {
		view->p_lineno = view->lines > 0 ? view->lines - 1 : 0;
		if (view->p_offset >= view->p_lineno) {
			unsigned long half = view->height / 2;

			if (view->p_lineno > half)
				view->p_offset = view->p_lineno - half;
			else
				view->p_offset = 0;
		}
	if (view_is_displayed(view) &&
	    view->offset != view->p_offset &&
	    view->lineno != view->p_lineno)
		werase(view->win);

	view->offset = view->p_offset;
	view->lineno = view->p_lineno;
	view->p_restore = FALSE;
	return TRUE;
	if (force)
		kill_io(view->pipe);
prepare_update(struct view *view, const char *argv[], const char *dir,
	       enum format_flags flags)
	if (view->pipe)
		end_update(view, TRUE);
	return init_io_rd(&view->io, argv, dir, flags);
}
static bool
prepare_update_file(struct view *view, const char *name)
{
	if (view->pipe)
		end_update(view, TRUE);
	return io_open(&view->io, name);
}
static bool
begin_update(struct view *view, bool refresh)
{
	if (view->pipe)
		end_update(view, TRUE);
	if (refresh) {
		if (!start_io(&view->io))
		if (view == VIEW(REQ_VIEW_TREE) && strcmp(view->vid, view->id))
			opt_path[0] = 0;
		if (!run_io_rd(&view->io, view->ops->argv, FORMAT_ALL))
	/* Clear the view and redraw everything since the tree sorting
	 * might have rearranged things. */
	bool redraw = view->lines == 0;
	bool can_read = TRUE;
	if (!io_can_read(view->pipe)) {
		if (view->lines == 0) {
			time_t secs = time(NULL) - view->start_time;
			if (secs > view->update_secs) {
				if (view->update_secs == 0)
					redraw_view(view);
				update_view_title(view);
				view->update_secs = secs;
			}
		}
		return TRUE;
	}
	for (; (line = io_get(view->pipe, '\n', can_read)); can_read = FALSE) {
			size_t inlen = strlen(line) + 1;
			if (ret != (size_t) -1)
		if (!view->ops->read(view, line)) {
			report("Allocation failure");
			end_update(view, TRUE);
			return FALSE;
		}
		unsigned long lines = view->lines;
			if (opt_line_number || view == VIEW(REQ_VIEW_BLAME))
				redraw = TRUE;
	if (restore_view_position(view))
		redraw = TRUE;

	if (redraw)
		redraw_view_from(view, 0);
	else
	struct line *line;

	if (!realloc_lines(view, view->lines + 1))
		return NULL;
	line = &view->line[view->lines++];
	line->dirty = 1;
static struct line *
add_line_format(struct view *view, enum line_type type, const char *fmt, ...)
{
	char buf[SIZEOF_STR];
	va_list args;

	va_start(args, fmt);
	if (vsnprintf(buf, sizeof(buf), fmt, args) >= sizeof(buf))
		buf[0] = 0;
	va_end(args);

	return buf[0] ? add_line_text(view, buf, type) : NULL;
}
	OPEN_PREPARED = 32,	/* Open already prepared command. */
	bool reload = !!(flags & (OPEN_RELOAD | OPEN_REFRESH | OPEN_PREPARED));
		if (view->pipe)
			end_update(view, TRUE);
		restore_view_position(view);
		   !begin_update(view, flags & (OPEN_REFRESH | OPEN_PREPARED))) {
		view->p_restore = flags & (OPEN_RELOAD | OPEN_REFRESH);
	redraw_display(TRUE);
	open_external_viewer(mergetool_argv, opt_cdup);
		if (!VIEW(REQ_VIEW_PAGER)->pipe && !VIEW(REQ_VIEW_PAGER)->lines) {
		toggle_view_option(&opt_line_number, "line numbers");
		toggle_view_option(&opt_date, "date display");
		toggle_view_option(&opt_author, "author display");
		toggle_view_option(&opt_rev_graph, "revision graph display");
		toggle_view_option(&opt_show_refs, "reference display");
		redraw_display(TRUE);
			redraw_display(FALSE);
 * View backend utilities
/* Parse author lines where the name may be empty:
 *	author  <email@address.tld> 1138474660 +0100
 */
static void
parse_author_line(char *ident, char *author, size_t authorsize, struct tm *tm)
	char *nameend = strchr(ident, '<');
	char *emailend = strchr(ident, '>');

	if (nameend && emailend)
		*nameend = *emailend = 0;
	ident = chomp_string(ident);
	if (!*ident) {
		if (nameend)
			ident = chomp_string(nameend + 1);
		if (!*ident)
			ident = "Unknown";
	}

	string_ncopy_do(author, authorsize, ident, strlen(ident));

	/* Parse epoch and timezone */
	if (emailend && emailend[1] == ' ') {
		char *secs = emailend + 2;
		char *zone = strchr(secs, ' ');
		time_t time = (time_t) atol(secs);

		if (zone && strlen(zone) == STRING_SIZE(" +0700")) {
			long tz;

			zone++;
			tz  = ('0' - zone[1]) * 60 * 60 * 10;
			tz += ('0' - zone[2]) * 60 * 60;
			tz += ('0' - zone[3]) * 60;
			tz += ('0' - zone[4]) * 60;

			if (zone[0] == '-')
				tz = -tz;

			time -= tz;
		}

		gmtime_r(&time, tm);
	}
}

static enum input_status
select_commit_parent_handler(void *data, char *buf, int c)
{
	size_t parents = *(size_t *) data;
	int parent = 0;

	if (!isdigit(c))
		return INPUT_SKIP;

	if (*buf)
		parent = atoi(buf) * 10;
	parent += c - '0';

	if (parent > parents)
		return INPUT_SKIP;
	return INPUT_OK;
}

static bool
select_commit_parent(const char *id, char rev[SIZEOF_REV])
{
	char buf[SIZEOF_STR * 4];
	const char *revlist_argv[] = {
		"git", "rev-list", "-1", "--parents", id, NULL
	};
	int parents;

	if (!run_io_buf(revlist_argv, buf, sizeof(buf)) ||
	    !*chomp_string(buf) ||
	    (parents = (strlen(buf) / 40) - 1) < 0) {
		report("Failed to get parent information");
		return FALSE;

	} else if (parents == 0) {
		report("The selected commit has no parents");
		return FALSE;
	}

	if (parents > 1) {
		char prompt[SIZEOF_STR];
		char *result;

		if (!string_format(prompt, "Which parent? [1..%d] ", parents))
			return FALSE;
		result = prompt_input(prompt, select_commit_parent_handler, &parents);
		if (!result)
			return FALSE;
		parents = atoi(result);
	}

	string_copy_rev(rev, &buf[41 * parents]);
	return TRUE;
}

/*
 * Pager backend
 */

static bool
pager_draw(struct view *view, struct line *line, unsigned int lineno)
{
	char *text = line->data;
	const char *describe_argv[] = { "git", "describe", commit_id, NULL };
	if (run_io_buf(describe_argv, refbuf, sizeof(refbuf)))
		ref = chomp_string(refbuf);
	NULL,
static const char *log_argv[SIZEOF_ARG] = {
	"git", "log", "--no-color", "--cc", "--stat", "-n100", "%(head)", NULL
};

	log_argv,
static const char *diff_argv[SIZEOF_ARG] = {
	"git", "show", "--pretty=fuller", "--no-color", "--root",
		"--patch-with-stat", "--find-copies-harder", "-C", "%(commit)", NULL
};

static struct view_ops diff_ops = {
	"line",
	diff_argv,
	NULL,
	pager_read,
	pager_draw,
	pager_request,
	pager_grep,
	pager_select,
};
		add_line_format(view, LINE_DEFAULT, "    %-25s %s",
				key, req_info[i].help);
		add_line_format(view, LINE_DEFAULT, "    %-10s %-14s `%s`",
				keymap_table[req->keymap].name, key, cmd);
	NULL,
#define SIZEOF_TREE_MODE \
	STRING_SIZE("100644 ")
#define TREE_ID_OFFSET \
	STRING_SIZE("100644 blob ")
struct tree_entry {
	char id[SIZEOF_REV];
	mode_t mode;
	struct tm time;			/* Date from the author ident. */
	char author[75];		/* Author of the commit. */
	char name[1];
};
	return ((struct tree_entry *) line->data)->name;
}


static int
tree_compare_entry(struct line *line1, struct line *line2)
{
	if (line1->type != line2->type)
		return line1->type == LINE_TREE_DIR ? -1 : 1;
	return strcmp(tree_path(line1), tree_path(line2));
}

static struct line *
tree_entry(struct view *view, enum line_type type, const char *path,
	   const char *mode, const char *id)
{
	struct tree_entry *entry = calloc(1, sizeof(*entry) + strlen(path));
	struct line *line = entry ? add_line_data(view, entry, type) : NULL;
	if (!entry || !line) {
		free(entry);
		return NULL;
	}

	strncpy(entry->name, path, strlen(path));
	if (mode)
		entry->mode = strtoul(mode, NULL, 8);
	if (id)
		string_copy_rev(entry->id, id);

	return line;
tree_read_date(struct view *view, char *text, bool *read_date)
	static char author_name[SIZEOF_STR];
	static struct tm author_time;
	if (!text && *read_date) {
		*read_date = FALSE;

	} else if (!text) {
		char *path = *opt_path ? opt_path : ".";
		/* Find next entry to process */
		const char *log_file[] = {
			"git", "log", "--no-color", "--pretty=raw",
				"--cc", "--raw", view->id, "--", path, NULL
		};
		struct io io = {};

		if (!run_io_rd(&io, log_file, FORMAT_NONE)) {
			report("Failed to load tree data");
			return TRUE;
		}

		done_io(view->pipe);
		view->io = io;
		*read_date = TRUE;
	} else if (*text == 'a' && get_line_type(text) == LINE_AUTHOR) {
		parse_author_line(text + STRING_SIZE("author "),
				  author_name, sizeof(author_name), &author_time);
	} else if (*text == ':') {
		char *pos;
		size_t annotated = 1;
		size_t i;
		pos = strchr(text, '\t');
		if (!pos)
			return TRUE;
		text = pos + 1;
		if (*opt_prefix && !strncmp(text, opt_prefix, strlen(opt_prefix)))
			text += strlen(opt_prefix);
		if (*opt_path && !strncmp(text, opt_path, strlen(opt_path)))
			text += strlen(opt_path);
		pos = strchr(text, '/');
		if (pos)
			*pos = 0;

		for (i = 1; i < view->lines; i++) {
			struct line *line = &view->line[i];
			struct tree_entry *entry = line->data;

			annotated += !!*entry->author;
			if (*entry->author || strcmp(entry->name, text))
				continue;

			string_copy(entry->author, author_name);
			memcpy(&entry->time, &author_time, sizeof(entry->time));
			line->dirty = 1;
			break;

		if (annotated == view->lines)
			kill_io(view->pipe);
	return TRUE;
}

static bool
tree_read(struct view *view, char *text)
{
	static bool read_date = FALSE;
	struct tree_entry *data;
	struct line *entry, *line;
	enum line_type type;
	size_t textlen = text ? strlen(text) : 0;
	char *path = text + SIZEOF_TREE_ATTR;

	if (read_date || !text)
		return tree_read_date(view, text, &read_date);

	if (textlen <= SIZEOF_TREE_ATTR)
		return FALSE;
	if (view->lines == 0 &&
	    !tree_entry(view, LINE_TREE_PARENT, opt_path, NULL, NULL))
		return FALSE;

		/* Insert "link" to parent directory. */
		if (view->lines == 1 &&
		    !tree_entry(view, LINE_TREE_DIR, "..", "040000", view->ref))
			return FALSE;
	type = text[SIZEOF_TREE_MODE] == 't' ? LINE_TREE_DIR : LINE_TREE_FILE;
	entry = tree_entry(view, type, path, text, text + TREE_ID_OFFSET);
	if (!entry)
		return FALSE;
	data = entry->data;
	/* Skip "Directory ..." and ".." line. */
	for (line = &view->line[1 + !!*opt_path]; line < entry; line++) {
		if (tree_compare_entry(line, entry) <= 0)
		memmove(line + 1, line, (entry - line) * sizeof(*entry));
		line->data = data;
		for (; line <= entry; line++)
			line->dirty = line->cleareol = 1;
static bool
tree_draw(struct view *view, struct line *line, unsigned int lineno)
{
	struct tree_entry *entry = line->data;

	if (line->type == LINE_TREE_PARENT) {
		if (draw_text(view, line->type, "Directory path /", TRUE))
			return TRUE;
	} else {
		char mode[11] = "-r--r--r--";

		if (S_ISDIR(entry->mode)) {
			mode[3] = mode[6] = mode[9] = 'x';
			mode[0] = 'd';
		}
		if (S_ISLNK(entry->mode))
			mode[0] = 'l';
		if (entry->mode & S_IWUSR)
			mode[2] = 'w';
		if (entry->mode & S_IXUSR)
			mode[3] = 'x';
		if (entry->mode & S_IXGRP)
			mode[6] = 'x';
		if (entry->mode & S_IXOTH)
			mode[9] = 'x';
		if (draw_field(view, LINE_TREE_MODE, mode, 11, TRUE))
			return TRUE;

		if (opt_author &&
		    draw_field(view, LINE_MAIN_AUTHOR, entry->author, opt_author_cols, TRUE))
			return TRUE;

		if (opt_date && draw_date(view, *entry->author ? &entry->time : NULL))
			return TRUE;
	}
	if (draw_text(view, line->type, entry->name, TRUE))
		return TRUE;
	return TRUE;
}

static void
open_blob_editor()
{
	char file[SIZEOF_STR] = "/tmp/tigblob.XXXXXX";
	int fd = mkstemp(file);

	if (fd == -1)
		report("Failed to create temporary file");
	else if (!run_io_append(blob_ops.argv, FORMAT_ALL, fd))
		report("Failed to save blob data to file");
	else
		open_editor(FALSE, file);
	if (fd != -1)
		unlink(file);
}

			open_blob_editor();
	case REQ_PARENT:
		return REQ_NONE;
	if (request == REQ_VIEW_TREE)
	struct tree_entry *entry = line->data;
		string_copy_rev(ref_blob, entry->id);
	string_copy_rev(view->ref, entry->id);
static const char *tree_argv[SIZEOF_ARG] = {
	"git", "ls-tree", "%(commit)", "%(directory)", NULL
};

	tree_argv,
	tree_draw,
static enum request
blob_request(struct view *view, enum request request, struct line *line)
{
	switch (request) {
	case REQ_EDIT:
		open_blob_editor();
		return REQ_NONE;
	default:
		return pager_request(view, request, line);
	}
}

static const char *blob_argv[SIZEOF_ARG] = {
	"git", "cat-file", "blob", "%(blob)", NULL
};

	blob_argv,
	blob_request,
static const char *blame_head_argv[] = {
	"git", "blame", "--incremental", "--", "%(file)", NULL
};

static const char *blame_ref_argv[] = {
	"git", "blame", "--incremental", "%(ref)", "--", "%(file)", NULL
};

static const char *blame_cat_file_argv[] = {
	"git", "cat-file", "blob", "%(ref):%(file)", NULL
};

	bool has_previous;		/* Was a "previous" line detected. */
	if (*opt_ref || !io_open(&view->io, opt_file)) {
		if (!run_io_rd(&view->io, blame_cat_file_argv, FORMAT_ALL))
		const char **argv = *opt_ref ? blame_ref_argv : blame_head_argv;
		if (view->lines == 0 || !run_io_rd(&io, argv, FORMAT_ALL)) {
			      view->lines ? blamed * 100 / view->lines : 0);
	} else if (match_blame_header("previous ", &line)) {
		commit->has_previous = TRUE;

static bool
check_blame_commit(struct blame *blame)
{
	if (!blame->commit)
		report("Commit data not loaded yet");
	else if (!strcmp(blame->commit->id, NULL_ID))
		report("No commit exist for the selected line");
	else
		return TRUE;
	return FALSE;
}

		if (check_blame_commit(blame)) {
			string_copy(opt_ref, blame->commit->id);
			open_view(view, REQ_VIEW_BLAME, OPEN_REFRESH);
		break;

	case REQ_PARENT:
		if (check_blame_commit(blame) &&
		    select_commit_parent(blame->commit->id, opt_ref))
			open_view(view, REQ_VIEW_BLAME, OPEN_REFRESH);
		break;
			struct view *diff = VIEW(REQ_VIEW_DIFF);
			const char *diff_index_argv[] = {
				"git", "diff-index", "--root", "--patch-with-stat",
					"-C", "-M", "HEAD", "--", view->vid, NULL
			};

			if (!blame->commit->has_previous) {
				diff_index_argv[1] = "diff";
				diff_index_argv[2] = "--no-color";
				diff_index_argv[6] = "--";
				diff_index_argv[7] = "/dev/null";
			}
			if (!prepare_update(diff, diff_index_argv, NULL, FORMAT_DASH)) {
				report("Failed to allocate diff command");
			}
			flags |= OPEN_PREPARED;
		if (VIEW(REQ_VIEW_DIFF)->pipe && !strcmp(blame->commit->id, NULL_ID))
			string_copy_rev(VIEW(REQ_VIEW_DIFF)->ref, NULL_ID);
	NULL,
	if (bufsize < 98 ||
status_run(struct view *view, const char *argv[], char status, enum line_type type)
	char *buf;
	struct io io = {};
	if (!run_io(&io, argv, NULL, IO_RD))
	while ((buf = io_get(&io, 0, TRUE))) {
		if (!file) {
			file = calloc(1, sizeof(*file));
			if (!file || !add_line_data(view, file, type))
				goto error_out;
		}
		/* Parse diff info part. */
		if (status) {
			file->status = status;
			if (status == 'A')
				string_copy(file->old.rev, NULL_ID);
		} else if (!file->status) {
			if (!status_get_diff(file, buf, strlen(buf)))
				goto error_out;
			buf = io_get(&io, 0, TRUE);
			if (!buf)
				break;
			/* Collapse all 'M'odified entries that follow a
			 * associated 'U'nmerged entry. */
			if (file->status == 'U') {
				unmerged = file;
			} else if (unmerged) {
				int collapse = !strcmp(buf, unmerged->new.name);
				unmerged = NULL;
				if (collapse) {
					free(file);
					file = NULL;
					view->lines--;
					continue;
		}
		/* Grab the old name for rename/copy. */
		if (!*file->old.name &&
		    (file->status == 'R' || file->status == 'C')) {
			string_ncopy(file->old.name, buf, strlen(buf));
			buf = io_get(&io, 0, TRUE);
			if (!buf)
				break;

		/* git-ls-files just delivers a NUL separated list of
		 * file names similar to the second half of the
		 * git-diff-* output. */
		string_ncopy(file->new.name, buf, strlen(buf));
		if (!*file->old.name)
			string_copy(file->old.name, file->new.name);
		file = NULL;
	if (io_error(&io)) {
		done_io(&io);
	done_io(&io);
static const char *status_diff_index_argv[] = {
	"git", "diff-index", "-z", "--diff-filter=ACDMRTXB",
			     "--cached", "-M", "HEAD", NULL
};
static const char *status_diff_files_argv[] = {
	"git", "diff-files", "-z", NULL
};
static const char *status_list_other_argv[] = {
	"git", "ls-files", "-z", "--others", "--exclude-standard", NULL
};
static const char *status_list_no_head_argv[] = {
	"git", "ls-files", "-z", "--cached", "--exclude-standard", NULL
};

static const char *update_index_argv[] = {
	"git", "update-index", "-q", "--unmerged", "--refresh", NULL
};

/* Restore the previous line number to stay in the context or select a
 * line with something that can be updated. */
static void
status_restore(struct view *view)
{
	if (view->p_lineno >= view->lines)
		view->p_lineno = view->lines - 1;
	while (view->p_lineno < view->lines && !view->line[view->p_lineno].data)
		view->p_lineno++;
	while (view->p_lineno > 0 && !view->line[view->p_lineno].data)
		view->p_lineno--;

	/* If the above fails, always skip the "On branch" line. */
	if (view->p_lineno < view->lines)
		view->lineno = view->p_lineno;
	else
		view->lineno = 1;

	if (view->lineno < view->offset)
		view->offset = view->lineno;
	else if (view->offset + view->height <= view->lineno)
		view->offset = view->lineno - view->height + 1;

	view->p_restore = FALSE;
}
	run_io_bg(update_index_argv);
		if (!status_run(view, status_list_no_head_argv, 'A', LINE_STAT_STAGED))
	} else if (!status_run(view, status_diff_index_argv, 0, LINE_STAT_STAGED)) {
	if (!status_run(view, status_diff_files_argv, 0, LINE_STAT_UNSTAGED) ||
	    !status_run(view, status_list_other_argv, '?', LINE_STAT_UNTRACKED))
	/* Restore the exact position or use the specialized restore
	 * mode? */
	if (!view->p_restore)
		status_restore(view);
	const char *oldpath = status ? status->old.name : NULL;
	/* Diffs for unmerged entries are empty when passing the new
	 * path, so leave it empty. */
	const char *newpath = status && status->status != 'U' ? status->new.name : NULL;
	struct view *stage = VIEW(REQ_VIEW_STAGE);
			const char *no_head_diff_argv[] = {
				"git", "diff", "--no-color", "--patch-with-stat",
					"--", "/dev/null", newpath, NULL
			};

			if (!prepare_update(stage, no_head_diff_argv, opt_cdup, FORMAT_DASH))
			const char *index_show_argv[] = {
				"git", "diff-index", "--root", "--patch-with-stat",
					"-C", "-M", "--cached", "HEAD", "--",
					oldpath, newpath, NULL
			};

			if (!prepare_update(stage, index_show_argv, opt_cdup, FORMAT_DASH))
	{
		const char *files_show_argv[] = {
			"git", "diff-files", "--root", "--patch-with-stat",
				"-C", "-M", "--", oldpath, newpath, NULL
		};

		if (!prepare_update(stage, files_show_argv, opt_cdup, FORMAT_DASH))
	}
		if (!newpath) {
		if (!prepare_update_file(stage, newpath))
			return REQ_QUIT;
	open_view(view, REQ_VIEW_STAGE, OPEN_PREPARED | split);
	unsigned long lineno;
	for (lineno = 0; lineno < view->lines; lineno++) {
		struct line *line = &view->line[lineno];
		if (line->type != type)
			continue;
		if (!pos && (!status || !status->status) && line[1].data) {
			select_view_line(view, lineno);
		}
		if (pos && !strcmp(status->new.name, pos->new.name)) {
			select_view_line(view, lineno);
			return TRUE;
		}
static bool
status_update_prepare(struct io *io, enum line_type type)
	const char *staged_argv[] = {
		"git", "update-index", "-z", "--index-info", NULL
	};
	const char *others_argv[] = {
		"git", "update-index", "-z", "--add", "--remove", "--stdin", NULL
	};
		return run_io(io, staged_argv, opt_cdup, IO_WR);
		return run_io(io, others_argv, opt_cdup, IO_WR);

		return run_io(io, others_argv, NULL, IO_WR);
		return FALSE;
status_update_write(struct io *io, struct status *status, enum line_type type)
	return io_write(io, buf, bufsize);
	struct io io = {};
	if (!status_update_prepare(&io, type))
	result = status_update_write(&io, status, type);
	done_io(&io);
	struct io io = {};
	if (!status_update_prepare(&io, line->type))
		result = status_update_write(&io, line->data, line->type);
	done_io(&io);
		const char *checkout_argv[] = {
			"git", "checkout", "--", status->old.name, NULL
		};
		if (!prompt_yesno("Are you sure you want to overwrite any changes?"))
		return run_io_fg(checkout_argv, opt_cdup);
	NULL,
stage_diff_write(struct io *io, struct line *line, struct line *end)
		if (!io_write(io, line->data, strlen(line->data)) ||
		    !io_write(io, "\n", 1))
		line++;
	const char *apply_argv[SIZEOF_ARG] = {
		"git", "apply", "--whitespace=nowarn", NULL
	};
	struct io io = {};
	int argc = 3;
	if (!revert)
		apply_argv[argc++] = "--cached";
	if (revert || stage_line_type == LINE_STAT_STAGED)
		apply_argv[argc++] = "-R";
	apply_argv[argc++] = "-";
	apply_argv[argc++] = NULL;
	if (!run_io(&io, apply_argv, opt_cdup, IO_WR))
	if (!stage_diff_write(&io, diff_hdr, chunk) ||
	    !stage_diff_write(&io, chunk, view->line + view->lines))
	done_io(&io);
	run_io_bg(update_index_argv);
	VIEW(REQ_VIEW_STATUS)->p_restore = TRUE;
	if (!status_exists(&stage_status, stage_line_type)) {
		status_restore(VIEW(REQ_VIEW_STATUS));
	}
		if (!prepare_update_file(view, stage_status.new.name)) {
			report("Failed to open file: %s", strerror(errno));
			return REQ_NONE;
		}
	NULL,
update_rev_graph(struct view *view, struct rev_graph *graph)
	if (view->lines > 2)
		view->line[view->lines - 3].dirty = 1;
	if (view->lines > 1)
		view->line[view->lines - 2].dirty = 1;
static const char *main_argv[SIZEOF_ARG] = {
	"git", "log", "--no-color", "--pretty=raw", "--parents",
		      "--topo-order", "%(head)", NULL
};

			view->line[view->lines - 1].dirty = 1;
		update_rev_graph(view, graph);
		parse_author_line(line + STRING_SIZE("author "),
				  commit->author, sizeof(commit->author),
				  &commit->time);
		update_rev_graph(view, graph);

		view->line[view->lines - 1].dirty = 1;
	main_argv,
static int
get_input(bool prompting)
	struct view *view;
	int i, key;
	if (prompting)
	while (true) {
		/* wgetch() with nodelay() enabled returns ERR when
		 * there's no input. */
		if (key == ERR) {
			doupdate();
		} else if (key == KEY_RESIZE) {
			int height, width;
			getmaxyx(stdscr, height, width);

			/* Resize the status view and let the view driver take
			 * care of resizing the displayed views. */
			resize_display();
			redraw_display(TRUE);
			wresize(status_win, 1, width);
			mvwin(status_win, height - 1, 0);
			wrefresh(status_win);
		} else {
			input_mode = FALSE;
			return key;
		}
	}
prompt_input(const char *prompt, input_handler handler, void *data)
	enum input_status status = INPUT_OK;
	static char buf[SIZEOF_STR];
	size_t pos = 0;
	buf[pos] = 0;
	while (status == INPUT_OK || status == INPUT_SKIP) {
		int key;
		key = get_input(TRUE);
			status = pos ? INPUT_STOP : INPUT_CANCEL;
				buf[--pos] = 0;
				status = INPUT_CANCEL;
			status = INPUT_CANCEL;
			status = handler(data, buf, key);
			if (status == INPUT_OK)
	if (status == INPUT_CANCEL)
static enum input_status
prompt_yesno_handler(void *data, char *buf, int c)
{
	if (c == 'y' || c == 'Y')
		return INPUT_STOP;
	if (c == 'n' || c == 'N')
		return INPUT_CANCEL;
	return INPUT_SKIP;
}

static bool
prompt_yesno(const char *prompt)
{
	char prompt2[SIZEOF_STR];

	if (!string_format(prompt2, "%s [Yy/Nn]", prompt))
		return FALSE;

	return !!prompt_input(prompt2, prompt_yesno_handler, NULL);
}

static enum input_status
read_prompt_handler(void *data, char *buf, int c)
{
	return isprint(c) ? INPUT_OK : INPUT_SKIP;
}

static char *
read_prompt(const char *prompt)
{
	return prompt_input(prompt, read_prompt_handler, NULL);
}

 * Repository properties
static int
git_properties(const char **argv, const char *separators,
	       int (*read_property)(char *, size_t, char *, size_t))
{
	struct io io = {};

	if (init_io_rd(&io, argv, NULL, FORMAT_NONE))
		return read_properties(&io, separators, read_property);
	return ERR;
}

	static const char *ls_remote_argv[SIZEOF_ARG] = {
		"git", "ls-remote", ".", NULL
	};
	static bool init = FALSE;

	if (!init) {
		argv_from_env(ls_remote_argv, "TIG_LS_REMOTE");
		init = TRUE;
	}
	return git_properties(ls_remote_argv, "\t", read_ref);
	const char *config_list_argv[] = { "git", GIT_CONFIG, "--list", NULL };

	return git_properties(config_list_argv, "=", read_repo_config_option);
	} else if (*name == '.') {

		string_ncopy(opt_prefix, name, namelen);
	const char *head_argv[] = {
		"git", "symbolic-ref", "HEAD", NULL
	};
	const char *rev_parse_argv[] = {
		"git", "rev-parse", "--git-dir", "--is-inside-work-tree",
			"--show-cdup", "--show-prefix", NULL
	};
	if (run_io_buf(head_argv, opt_head, sizeof(opt_head))) {
		chomp_string(opt_head);
		if (!prefixcmp(opt_head, "refs/heads/")) {
			char *offset = opt_head + STRING_SIZE("refs/heads/");
			memmove(opt_head, offset, strlen(offset) + 1);
		}
	}
	return git_properties(rev_parse_argv, "=", read_repo_info);
read_properties(struct io *io, const char *separators,
	if (!start_io(io))
	while (state == OK && (name = io_get(io, '\n', TRUE))) {
	if (state != ERR && io_error(io))
	done_io(io);
	const char **run_argv = NULL;
	request = parse_options(argc, argv, &run_argv);
		argv_from_env(view->ops->argv, view->cmd_env);
	if (request == REQ_VIEW_PAGER || run_argv) {
		if (request == REQ_VIEW_PAGER)
			io_open(&VIEW(request)->io, "");
		else if (!prepare_update(VIEW(request), run_argv, NULL, FORMAT_NONE))
			die("Failed to format arguments");
		open_view(NULL, request, OPEN_PREPARED);
		request = REQ_NONE;
	}

		int key = get_input(FALSE);
			if (cmd) {
				struct view *next = VIEW(REQ_VIEW_PAGER);
				const char *argv[SIZEOF_ARG] = { "git" };
				int argc = 1;

				/* When running random commands, initially show the
				 * command in the title. However, it maybe later be
				 * overwritten if a commit line is selected. */
				string_ncopy(next->ref, cmd, strlen(cmd));

				if (!argv_from_string(argv, &argc, cmd)) {
					report("Too many arguments");
				} else if (!prepare_update(next, argv, NULL, FORMAT_DASH)) {
					report("Failed to format command");
					open_view(view, REQ_VIEW_PAGER, OPEN_PREPARED);