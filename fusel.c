/*
 * Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
 *
 * Distributed under the Boost Software License, Version 1.0.
 * See accompanying file LICENSE_1_0.txt or copy at
 *   http://www.boost.org/LICENSE_1_0.txt
 */

#include <git2.h>
#include <karrot.h>
#include <stdio.h>

struct progress_data
  {
  git_transfer_progress fetch_progress;
  size_t completed_steps;
  size_t total_steps;
  const char *path;
  };

void print_progress(struct progress_data const *pd)
  {
  int network_percent = (100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects;
  int index_percent = (100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects;
  int checkout_percent = pd->total_steps > 0 ? (100 * pd->completed_steps) / pd->total_steps : 0.f;
  int kbytes = pd->fetch_progress.received_bytes / 1024;
  printf("net %3d%% (%4d kb, %5d/%5d)  /  idx %3d%% (%5d/%5d)  /  chk %3d%% (%4zu/%4zu) %s\n",
      network_percent,
      kbytes,
      pd->fetch_progress.received_objects,
      pd->fetch_progress.total_objects,
      index_percent,
      pd->fetch_progress.indexed_objects,
      pd->fetch_progress.total_objects,
      checkout_percent,
      pd->completed_steps,
      pd->total_steps,
      pd->path);
  }

int fetch_progress(git_transfer_progress const *stats, void *payload)
  {
  struct progress_data *pd = (struct progress_data*) payload;
  pd->fetch_progress = *stats;
  print_progress(pd);
  return 0;
  }

void checkout_progress(char const *path, size_t cur, size_t tot, void *payload)
  {
  struct progress_data *pd = (struct progress_data*) payload;
  pd->completed_steps = cur;
  pd->total_steps = tot;
  pd->path = path;
  print_progress(pd);
  }

int cred_acquire(
    git_cred **cred,
    const char *url,
    const char *username_from_url,
    unsigned int allowed_types,
    void *payload)
  {
  char username[128] = {0};
  char password[128] = {0};
  printf("cred required for %s\n", url);
  printf("Username: ");
  scanf("%s", username);
  printf("Password: ");
  scanf("%s", password);
  return git_cred_userpass_plaintext_new(cred, username, password);
  }

void download(KImplementation const *impl, int requested, KError *error, void *ctx)
  {
  KDictionary const *values = k_implementation_get_values(impl);
  char const *href = k_dictionary_lookup(values, "href");
  char const *tag = k_dictionary_lookup(values, "tag");
  char const *path = k_implementation_get_name(impl);

  git_repository *repo = NULL;
  git_remote *origin = NULL;
  git_object *object = NULL;

  struct progress_data progress_data = {{0}};
  git_checkout_opts checkout_opts = GIT_CHECKOUT_OPTS_INIT;
  checkout_opts.version = GIT_CHECKOUT_OPTS_VERSION;
  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkout_opts.progress_cb = checkout_progress;
  checkout_opts.progress_payload = &progress_data;

  if (git_repository_open(&repo, path) == GIT_OK)
    {
    if (git_remote_load(&origin, repo, "origin") != GIT_OK)
      {
      k_error_set(error, giterr_last()->message);
      goto repository_free;
      }
    if (strcmp(git_remote_url(origin), href) != 0)
      {
      k_error_set(error, "different origin");
      goto remote_free;
      }
    }
  else
    {
    if (git_repository_init(&repo, path, 0) != GIT_OK)
      {
      k_error_set(error, giterr_last()->message);
      return;
      }
    if (git_remote_create(&origin, repo, "origin", href) != GIT_OK)
      {
      k_error_set(error, giterr_last()->message);
      goto repository_free;
      }
    }
  git_remote_set_update_fetchhead(origin, 0);
  git_remote_set_cred_acquire_cb(origin, cred_acquire, NULL);
  if (git_remote_connect(origin, GIT_DIRECTION_FETCH) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    goto remote_free;
    }
  if (git_remote_download(origin, fetch_progress, &progress_data) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    goto remote_disconnect;
    }
  if (git_remote_update_tips(origin) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    goto remote_disconnect;
    }
  if (git_revparse_single(&object, repo, tag) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    goto remote_disconnect;
    }
  if (git_repository_set_head_detached(repo, git_object_id(object)) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    goto remote_disconnect;
    }
  if (git_checkout_tree(repo, object, &checkout_opts) != GIT_OK)
    {
    k_error_set(error, giterr_last()->message);
    }
remote_disconnect:
  git_remote_disconnect(origin);
remote_free:
  git_remote_free(origin);
repository_free:
  git_repository_free(repo);
  }

void close_file(void *ctx)
  {
  fclose((FILE*) ctx);
  }

int main(int argc, char* argv[])
  {
  int i, result;
  KDriver driver = {"git"};
  KEngine *engine = k_engine_new("http://purplekarrot.net/2013/");
  driver.download = download;
  //driver.download_target_destroy_notify = close_file;
  k_engine_add_driver(engine, &driver);
  for (i = 1; i < argc; ++i)
    {
    k_engine_add_request(engine, argv[i], 1);
    }
  result = k_engine_run(engine);
  if (result > 0)
    {
    puts("The request is not satisfiable!\n");
    result = 0;
    }
  if (result < 0)
    {
    printf("Error: %s\n", k_engine_error_message(engine));
    }
  k_engine_free(engine);
  return result;
  }
