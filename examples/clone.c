#include "common.h"

typedef struct progress_data {
	git_indexer_progress fetch_progress;
	size_t completed_steps;
	size_t total_steps;
	const char *path;
} progress_data;

static void print_progress(const progress_data *pd)
{
	int network_percent = pd->fetch_progress.total_objects > 0 ?
		(100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects :
		0;
	int index_percent = pd->fetch_progress.total_objects > 0 ?
		(100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects :
		0;

	int checkout_percent = pd->total_steps > 0
		? (int)((100 * pd->completed_steps) / pd->total_steps)
		: 0;
	size_t kbytes = pd->fetch_progress.received_bytes / 1024;

	if (pd->fetch_progress.total_objects &&
		pd->fetch_progress.received_objects == pd->fetch_progress.total_objects) {
		printf("Resolving deltas %u/%u\r",
		       pd->fetch_progress.indexed_deltas,
		       pd->fetch_progress.total_deltas);
	} else {
		printf("net %3d%% (%4" PRIuZ " kb, %5u/%5u)  /  idx %3d%% (%5u/%5u)  /  chk %3d%% (%4" PRIuZ "/%4" PRIuZ")%s\n",
		   network_percent, kbytes,
		   pd->fetch_progress.received_objects, pd->fetch_progress.total_objects,
		   index_percent, pd->fetch_progress.indexed_objects, pd->fetch_progress.total_objects,
		   checkout_percent,
		   pd->completed_steps, pd->total_steps,
		   pd->path);
	}
}

static int sideband_progress(const char *str, int len, void *payload)
{
	(void)payload; /* unused */

	printf("remote: %.*s", len, str);
	fflush(stdout);
	return 0;
}

static int fetch_progress(const git_indexer_progress *stats, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->fetch_progress = *stats;
	print_progress(pd);
	return 0;
}
static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->completed_steps = cur;
	pd->total_steps = tot;
	pd->path = path;
	print_progress(pd);
}


int lg2_clone(git_repository *repo, int argc, char **argv)
{
	progress_data pd = {{0}};
	git_repository *cloned_repo = NULL;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	const char *url = argv[1];
	const char *path = argv[2];
	int error;

	(void)repo; /* unused */

	/* Validate args */
	if (argc < 3) {
		printf ("USAGE: %s <url> <path>\n", argv[0]);
		return -1;
	}

	/* Set up options */
	checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	checkout_opts.progress_cb = checkout_progress;
	checkout_opts.progress_payload = &pd;
	clone_opts.checkout_opts = checkout_opts;
	clone_opts.fetch_opts.callbacks.sideband_progress = sideband_progress;
	clone_opts.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	clone_opts.fetch_opts.callbacks.credentials = cred_acquire_cb;
	clone_opts.fetch_opts.callbacks.payload = &pd;

	// chenkx : add proxy in WSL env
	// clone_opts.fetch_opts.proxy_opts.type = GIT_PROXY_SPECIFIED;
	// clone_opts.fetch_opts.proxy_opts.url = "http://172.28.80.1:7890";

	/* Do the clone */
	error = git_clone(&cloned_repo, url, path, &clone_opts);
	printf("\n");
	if (error != 0) {
		const git_error *err = git_error_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}
	else if (cloned_repo) git_repository_free(cloned_repo);
	return error;
}
