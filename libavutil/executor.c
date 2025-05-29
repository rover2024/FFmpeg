/****************************************************************************
** CFI wrapped code from reading C file 'executor__cfic_tmp_new__.c'
**
** Created by: Lorelei CFI compiler
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

//
// CFI declarations begin
//
enum LoreLib_Constants {
    LoreLib_CFI_Count = 28,
};

struct LoreLib_HostLibraryContext {
    void *AddressBoundary;

    void (*HrtSetThreadCallback)(void *callback);
    void *HrtPThreadCreate;
    void *HrtPThreadExit;

    void *CFIs[LoreLib_CFI_Count];
};

__attribute__((visibility("default"))) struct LoreLib_HostLibraryContext LoreLib_HostLibCtx;

#define LORELIB_CFI(INDEX, FP)                                                                       \
    ({                                                                                               \
        typedef __typeof__(FP) _LORELIB_CFI_TYPE;                                                    \
        void *_lorelib_cfi_ret = (void *) (FP);                                                      \
        if ((unsigned long) _lorelib_cfi_ret < (unsigned long) LoreLib_HostLibCtx.AddressBoundary) { \
            LoreLib_HostLibCtx.HrtSetThreadCallback(_lorelib_cfi_ret);                               \
            _lorelib_cfi_ret = (void *) LoreLib_HostLibCtx.CFIs[INDEX - 1];                          \
        }                                                                                            \
        (_LORELIB_CFI_TYPE) _lorelib_cfi_ret;                                                        \
    })

// decl: int (const struct AVTask *, const struct AVTask *)
#define LORELIB_CFI_27(FP) LORELIB_CFI(27, FP)

// decl: int (const struct AVTask *, void *)
#define LORELIB_CFI_28(FP) LORELIB_CFI(28, FP)

// decl: int (struct AVTask *, void *, void *)
#define LORELIB_CFI_5(FP) LORELIB_CFI(5, FP)

//
// CFI declarations end
//


//
// Original code begin
//
/*
 * Copyright (C) 2023 Nuo Mi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stdbool.h>

#include "mem.h"
#include "thread.h"

#include "executor.h"

#if !HAVE_THREADS

#define ExecutorThread  char

#define executor_thread_create(t, a, s, ar)      0
#define executor_thread_join(t, r)               do {} while(0)

#else

#define ExecutorThread  pthread_t

#define executor_thread_create(t, a, s, ar)      pthread_create(t, a, s, ar)
#define executor_thread_join(t, r)               pthread_join(t, r)

#endif //!HAVE_THREADS

typedef struct ThreadInfo {
    AVExecutor *e;
    ExecutorThread thread;
} ThreadInfo;

struct AVExecutor {
    AVTaskCallbacks cb;
    int thread_count;
    bool recursive;

    ThreadInfo *threads;
    uint8_t *local_contexts;

    AVMutex lock;
    AVCond cond;
    int die;

    AVTask *tasks;
};

static AVTask* remove_task(AVTask **prev, AVTask *t)
{
    *prev  = t->next;
    t->next = NULL;
    return t;
}

static void add_task(AVTask **prev, AVTask *t)
{
    t->next = *prev;
    *prev   = t;
}

static int run_one_task(AVExecutor *e, void *lc)
{
    AVTaskCallbacks *cb = &e->cb;
    AVTask **prev;

    for (prev = &e->tasks; *prev && !LORELIB_CFI_28(cb->ready)(*prev, cb->user_data); prev = &(*prev)->next)
        /* nothing */;
    if (*prev) {
        AVTask *t = remove_task(prev, *prev);
        if (e->thread_count > 0)
            ff_mutex_unlock(&e->lock);
        LORELIB_CFI_5(cb->run)(t, lc, cb->user_data);
        if (e->thread_count > 0)
            ff_mutex_lock(&e->lock);
        return 1;
    }
    return 0;
}

#if HAVE_THREADS
static void *executor_worker_task(void *data)
{
    ThreadInfo *ti = (ThreadInfo*)data;
    AVExecutor *e  = ti->e;
    void *lc       = e->local_contexts + (ti - e->threads) * e->cb.local_context_size;

    ff_mutex_lock(&e->lock);
    while (1) {
        if (e->die) break;

        if (!run_one_task(e, lc)) {
            //no task in one loop
            ff_cond_wait(&e->cond, &e->lock);
        }
    }
    ff_mutex_unlock(&e->lock);
    return NULL;
}
#endif

static void executor_free(AVExecutor *e, const int has_lock, const int has_cond)
{
    if (e->thread_count) {
        //signal die
        ff_mutex_lock(&e->lock);
        e->die = 1;
        ff_cond_broadcast(&e->cond);
        ff_mutex_unlock(&e->lock);

        for (int i = 0; i < e->thread_count; i++)
            executor_thread_join(e->threads[i].thread, NULL);
    }
    if (has_cond)
        ff_cond_destroy(&e->cond);
    if (has_lock)
        ff_mutex_destroy(&e->lock);

    av_free(e->threads);
    av_free(e->local_contexts);

    av_free(e);
}

AVExecutor* av_executor_alloc(const AVTaskCallbacks *cb, int thread_count)
{
    AVExecutor *e;
    int has_lock = 0, has_cond = 0;
    if (!cb || !cb->user_data || !cb->ready || !cb->run || !cb->priority_higher)
        return NULL;

    e = av_mallocz(sizeof(*e));
    if (!e)
        return NULL;
    e->cb = *cb;

    e->local_contexts = av_calloc(FFMAX(thread_count, 1), e->cb.local_context_size);
    if (!e->local_contexts)
        goto free_executor;

    e->threads = av_calloc(FFMAX(thread_count, 1), sizeof(*e->threads));
    if (!e->threads)
        goto free_executor;

    if (!thread_count)
        return e;

    has_lock = !ff_mutex_init(&e->lock, NULL);
    has_cond = !ff_cond_init(&e->cond, NULL);

    if (!has_lock || !has_cond)
        goto free_executor;

    for (/* nothing */; e->thread_count < thread_count; e->thread_count++) {
        ThreadInfo *ti = e->threads + e->thread_count;
        ti->e = e;
        if (executor_thread_create(&ti->thread, NULL, executor_worker_task, ti))
            goto free_executor;
    }
    return e;

free_executor:
    executor_free(e, has_lock, has_cond);
    return NULL;
}

void av_executor_free(AVExecutor **executor)
{
    int thread_count;

    if (!executor || !*executor)
        return;
    thread_count = (*executor)->thread_count;
    executor_free(*executor, thread_count, thread_count);
    *executor = NULL;
}

void av_executor_execute(AVExecutor *e, AVTask *t)
{
    AVTaskCallbacks *cb = &e->cb;
    AVTask **prev;

    if (e->thread_count)
        ff_mutex_lock(&e->lock);
    if (t) {
        for (prev = &e->tasks; *prev && LORELIB_CFI_27(cb->priority_higher)(*prev, t); prev = &(*prev)->next)
            /* nothing */;
        add_task(prev, t);
    }
    if (e->thread_count) {
        ff_cond_signal(&e->cond);
        ff_mutex_unlock(&e->lock);
    }

    if (!e->thread_count || !HAVE_THREADS) {
        if (e->recursive)
            return;
        e->recursive = true;
        // We are running in a single-threaded environment, so we must handle all tasks ourselves
        while (run_one_task(e, e->local_contexts))
            /* nothing */;
        e->recursive = false;
    }
}

//
// Original code end
//


