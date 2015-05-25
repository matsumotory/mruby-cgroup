/*
** mrb_cgroup - cgroup module for mruby
**
** Copyright (c) mod_mruby developers 2012-
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#include <libcgroup.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/class.h"

#define BLKIO_STRING_SIZE 64
#define DONE mrb_gc_arena_restore(mrb, 0);

typedef enum {
    MRB_CGROUP_cpu,
    MRB_CGROUP_cpuset,
    MRB_CGROUP_cpuacct,
    MRB_CGROUP_blkio,
    MRB_CGROUP_memory
} group_type_t;
typedef struct cgroup cgroup_t;
typedef struct cgroup_controller cgroup_controller_t;
typedef struct {
    int already_exist;
    mrb_value group_name;
    group_type_t type;
    cgroup_t *cg;
    cgroup_controller_t *cgc;
} mrb_cgroup_context;

//
// private
//

static void mrb_cgroup_context_free(mrb_state *mrb, void *p)
{
    mrb_cgroup_context *ctx = p;
//    cgroup_free_controllers(c->cg);
    cgroup_free(&ctx->cg);
}

static const struct mrb_data_type mrb_cgroup_context_type = {
    "mrb_cgroup_context", mrb_cgroup_context_free,
};

static mrb_cgroup_context *mrb_cgroup_get_context(mrb_state *mrb,  mrb_value self, const char *ctx_flag)
{
    mrb_cgroup_context *c;
    mrb_value context;

    context = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, ctx_flag));
    Data_Get_Struct(mrb, context, &mrb_cgroup_context_type, c);
    if (!c)
        mrb_raise(mrb, E_RUNTIME_ERROR, "get mrb_cgroup_context failed");

    return c;
}

//
// group
//

static mrb_value mrb_cgroup_create(mrb_state *mrb, mrb_value self)
{
    int code;
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");

    // BUG1 : cgroup_create_cgroup returns an error(Invalid argument:50016:ECGOTHER), despite actually succeeding
    // BUG2 : cgroup_delete_cgroup returns an error(This kernel does not support this feature:50029:ECGCANTSETVALUE), despite actually succeeding
    // REFS : libcgroup/src/api.c 1620 - 1630 comments
    //
    //        error = cg_set_control_value(path,
    //            cgroup->controller[k]->values[j]->value);
    //        /*
    //         * Should we undo, what we've done in the loops above?
    //         * An error should not be treated as fatal, since we
    //         * have several read-only files and several files that
    //         * are only conditionally created in the child.
    //         *
    //         * A middle ground would be to track that there
    //         * was an error and return a diagnostic value--
    //         * callers don't get context for the error, but can
    //         * ignore it specifically if they wish.
    //         */
    //        if (error) {
    //            cgroup_dbg("failed to set %s: %s (%d)\n",
    //                path,
    //                cgroup_strerror(error), error);
    //            retval = ECGCANTSETVALUE;
    //            continue;
    //        }
    //

    if ((code = cgroup_create_cgroup(mrb_cg_cxt->cg, 1)) && code != ECGOTHER && code != ECGCANTSETVALUE) {
        mrb_raisef(mrb
            , E_RUNTIME_ERROR
            , "cgroup_create failed: %S(%S)"
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))
            , mrb_fixnum_value(code)
        );
    }
    mrb_cg_cxt->already_exist = 1;
    mrb_iv_set(mrb
        , self
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &mrb_cgroup_context_type
            , (void *)mrb_cg_cxt)
        )
    );

    return self;
}

static mrb_value mrb_cgroup_delete(mrb_state *mrb, mrb_value self)
{
    int code;
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");

    // BUG1 : cgroup_delete_cgroup returns an error(No such file or directory:50016:ECGOTHER), despite actually succeeding
    if ((code = cgroup_delete_cgroup(mrb_cg_cxt->cg, 1)) && code != ECGOTHER) {
        mrb_raisef(mrb
            , E_RUNTIME_ERROR
            , "cgroup_delete faild: %S(%S)"
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))
            , mrb_fixnum_value(code)
        );
    }

    return self;
}

static mrb_value mrb_cgroup_exist_p(mrb_state *mrb, mrb_value self)
{
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");
    return (mrb_cg_cxt->already_exist) ? mrb_true_value(): mrb_false_value();
}

//
//task
//

static mrb_value mrb_cgroup_attach(mrb_state *mrb, mrb_value self)
{
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");
    mrb_value pid = mrb_nil_value();
    mrb_get_args(mrb, "|i", &pid);

    if (mrb_nil_p(pid)) {
        cgroup_attach_task(mrb_cg_cxt->cg);
    } else {
        cgroup_attach_task_pid(mrb_cg_cxt->cg, mrb_fixnum(pid));
    }
    mrb_iv_set(mrb
        , self
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &mrb_cgroup_context_type
            , (void *)mrb_cg_cxt)
        )
    );

    return self;
}

//
// init
//
#define SET_MRB_CGROUP_INIT_GROUP(gname) \
static mrb_value mrb_cgroup_##gname##_init(mrb_state *mrb, mrb_value self)                                      \
{                                                                                                               \
    mrb_cgroup_context *mrb_cg_cxt = (mrb_cgroup_context *)mrb_malloc(mrb, sizeof(mrb_cgroup_context));         \
                                                                                                                \
    mrb_cg_cxt->type = MRB_CGROUP_##gname;                                                                      \
    if (cgroup_init()) {                                                                                        \
        mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_init " #gname " failed");                                        \
    }                                                                                                           \
    mrb_get_args(mrb, "o", &mrb_cg_cxt->group_name);                                                            \
    mrb_cg_cxt->cg = cgroup_new_cgroup(RSTRING_PTR(mrb_cg_cxt->group_name));                                    \
    if (mrb_cg_cxt->cg == NULL) {                                                                               \
        mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_new_cgroup failed");                                             \
    }                                                                                                           \
                                                                                                                \
    if (cgroup_get_cgroup(mrb_cg_cxt->cg)) {                                                                    \
        mrb_cg_cxt->already_exist = 0;                                                                          \
        mrb_cg_cxt->cgc = cgroup_add_controller(mrb_cg_cxt->cg, #gname);                                        \
        if (mrb_cg_cxt->cgc == NULL) {                                                                          \
            mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_add_controller " #gname " failed");                          \
        }                                                                                                       \
    } else {                                                                                                    \
        mrb_cg_cxt->already_exist = 1;                                                                          \
        mrb_cg_cxt->cgc = cgroup_get_controller(mrb_cg_cxt->cg, #gname);                                        \
        if (mrb_cg_cxt->cgc == NULL) {                                                                          \
            mrb_cg_cxt->cgc = cgroup_add_controller(mrb_cg_cxt->cg, #gname);                                    \
            if (mrb_cg_cxt->cgc == NULL) {                                                                      \
                mrb_raise(mrb, E_RUNTIME_ERROR, "get_cgroup success, but add_controller "  #gname " failed");   \
            }                                                                                                   \
        }                                                                                                       \
    }                                                                                                           \
    mrb_iv_set(mrb                                                                                              \
        , self                                                                                                  \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                                                 \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                                    \
            , mrb->object_class                                                                                 \
            , &mrb_cgroup_context_type                                                                          \
            , (void *)mrb_cg_cxt)                                                                               \
        )                                                                                                       \
    );                                                                                                          \
                                                                                                                \
    return self;                                                                                                \
}

SET_MRB_CGROUP_INIT_GROUP(cpu);
SET_MRB_CGROUP_INIT_GROUP(cpuset);
SET_MRB_CGROUP_INIT_GROUP(cpuacct);
SET_MRB_CGROUP_INIT_GROUP(blkio);
SET_MRB_CGROUP_INIT_GROUP(memory);

//
// cgroup_set_value_int64
//
#define SET_VALUE_INT64_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_set_##gname##_##key(mrb_state *mrb, mrb_value self)                      \
{                                                                                             \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
    mrb_int val;                                                                              \
    int code;                                                                                 \
    mrb_get_args(mrb, "i", &val);                                                             \
                                                                                              \
    if ((code = cgroup_set_value_int64(mrb_cg_cxt->cgc, #gname "." #key, val))) {                      \
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_set_value_int64 " #gname "." #key " failed: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code))); \
    }                                                                                         \
    mrb_iv_set(mrb                                                                            \
        , self                                                                                \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                               \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                  \
            , mrb->object_class                                                               \
            , &mrb_cgroup_context_type                                                        \
            , (void *)mrb_cg_cxt)                                                             \
        )                                                                                     \
    );                                                                                        \
                                                                                              \
    return self;                                                                              \
}

SET_VALUE_INT64_MRB_CGROUP(cpu, cfs_quota_us);
SET_VALUE_INT64_MRB_CGROUP(cpu, cfs_period_us);
SET_VALUE_INT64_MRB_CGROUP(cpu, rt_period_us);
SET_VALUE_INT64_MRB_CGROUP(cpu, rt_runtime_us);
SET_VALUE_INT64_MRB_CGROUP(cpu, shares);
SET_VALUE_INT64_MRB_CGROUP(cpuacct, usage);
SET_VALUE_INT64_MRB_CGROUP(memory, limit_in_bytes);

#define GET_VALUE_INT64_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_get_##gname##_##key(mrb_state *mrb, mrb_value self)                      \
{                                                                                             \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
    int64_t val;                                                                              \
    int code;                                                                                 \
    if ((code = cgroup_get_value_int64(mrb_cg_cxt->cgc, #gname "." #key, &val)) && code != ECGROUPVALUENOTEXIST) {            \
        mrb_raisef(mrb \
            , E_RUNTIME_ERROR \
            , "cgroup_get_value_int64 " #gname "." #key " failed: %S(%S)" \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code)) \
            , mrb_fixnum_value(code) \
        ); \
    }                                                                                         \
    if (code == ECGROUPVALUENOTEXIST) {\
        return mrb_nil_value();\
    } else {\
        return mrb_fixnum_value(val);                                                             \
    }\
}

GET_VALUE_INT64_MRB_CGROUP(cpu, cfs_quota_us);
GET_VALUE_INT64_MRB_CGROUP(cpu, cfs_period_us);
GET_VALUE_INT64_MRB_CGROUP(cpu, rt_period_us);
GET_VALUE_INT64_MRB_CGROUP(cpu, rt_runtime_us);
GET_VALUE_INT64_MRB_CGROUP(cpu, shares);
GET_VALUE_INT64_MRB_CGROUP(cpuacct, usage);
GET_VALUE_INT64_MRB_CGROUP(memory, limit_in_bytes);
GET_VALUE_INT64_MRB_CGROUP(memory, usage_in_bytes);
GET_VALUE_INT64_MRB_CGROUP(memory, max_usage_in_bytes);

//
// cgroup_get_value_string
//
#define GET_VALUE_STRING_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_get_##gname##_##key(mrb_state *mrb, mrb_value self)               \
{                                                                                             \
    int code;                                                                                 \
    char *val;                                                                                \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
                                                                                              \
    if ((code = cgroup_get_value_string(mrb_cg_cxt->cgc , #gname "." #key, &val)) != 0 && code != ECGROUPVALUENOTEXIST) {     \
        mrb_raisef(mrb                                                                        \
            , E_RUNTIME_ERROR                                                                 \
            , "cgroup_get_value_string " #gname "." #key " failed: %S(%S)"                        \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                    \
            , mrb_fixnum_value(code)                                    \
        );                                                                                    \
    }                                                                                         \
    if (strcmp(val, "")) {                                                                    \
        return mrb_str_new_cstr(mrb, val);                                                    \
    } else {                                                                                  \
        return  mrb_nil_value();                                                              \
    }                                                                                         \
}

GET_VALUE_STRING_MRB_CGROUP(cpu, stat);
GET_VALUE_STRING_MRB_CGROUP(cpuset, cpus);
GET_VALUE_STRING_MRB_CGROUP(cpuset, mems);
GET_VALUE_STRING_MRB_CGROUP(cpuacct, stat);
GET_VALUE_STRING_MRB_CGROUP(cpuacct, usage_percpu);

//
// cgroup_get_value_string (a number of keys are 2)
//
#define GET_VALUE_STRING_MRB_CGROUP_KEY2(gname, key1, key2) \
static mrb_value mrb_cgroup_get_##gname##_##key1##_##key2(mrb_state *mrb, mrb_value self)               \
{                                                                                                       \
    int code;                                                                                           \
    char *val;                                                                                          \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");           \
                                                                                                        \
    if ((code = cgroup_get_value_string(mrb_cg_cxt->cgc , #gname "." #key1 "." #key2, &val)) != 0 && code != ECGROUPVALUENOTEXIST) {    \
        mrb_raisef(mrb                                                                                  \
            , E_RUNTIME_ERROR                                                                           \
            , "cgroup_get_value_string " #gname "." #key1 "." #key2 " failed: %S(%S)"                   \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                              \
            , mrb_fixnum_value(code)                                    \
        );                                                                                              \
    }                                                                                                   \
    if (strcmp(val, "")) {                                                                              \
        return mrb_str_new_cstr(mrb, val);                                                              \
    } else {                                                                                            \
        return  mrb_nil_value();                                                                        \
    }                                                                                                   \
}

GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_bps_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_bps_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_iops_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_iops_device);

//
// cgroup_set_value_string
//
#define SET_VALUE_STRING_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_set_##gname##_##key(mrb_state *mrb, mrb_value self)                           \
{                                                                                                         \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");             \
    int code;                                                                                             \
    char *val;                                                                                            \
    mrb_get_args(mrb, "z", &val);                                                                         \
    if ((code = cgroup_set_value_string(mrb_cg_cxt->cgc , #gname "." #key , val))) {                      \
        mrb_raisef(mrb                                                                                    \
            , E_RUNTIME_ERROR                                                                             \
            , "cgroup_set_value_string " #gname "." #key " failed: %S(%S)"                                \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                                \
            , mrb_fixnum_value(code)                                                                      \
        );                                                                                                \
    }                                                                                                     \
    mrb_iv_set(mrb                                                                                        \
        , self                                                                                            \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                                           \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                              \
            , mrb->object_class                                                                           \
            , &mrb_cgroup_context_type                                                                    \
            , (void *)mrb_cg_cxt)                                                                         \
        )                                                                                                 \
    );                                                                                                    \
    return self;                                                                                          \
}

SET_VALUE_STRING_MRB_CGROUP(cpuset, cpus);
SET_VALUE_STRING_MRB_CGROUP(cpuset, mems);

//
// cgroup_set_value_string (a number of keys are 2)
//
#define SET_VALUE_STRING_MRB_CGROUP_KEY2(gname, key1, key2) \
static mrb_value mrb_cgroup_set_##gname##_##key1##_##key2(mrb_state *mrb, mrb_value self)                 \
{                                                                                                         \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");             \
    int code;                                                                                             \
    char *val;                                                                                            \
    mrb_get_args(mrb, "z", &val);                                                                         \
    if ((code = cgroup_set_value_string(mrb_cg_cxt->cgc , #gname "." #key1 "." #key2 , val)) != 0) {      \
        mrb_raisef(mrb                                                                                    \
            , E_RUNTIME_ERROR                                                                             \
            , "cgroup_set_value_string " #gname "." #key1 "." #key2 " failed: %S(%S)"                     \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                                \
            , mrb_fixnum_value(code)                                                                      \
        );                                                                                                \
    }                                                                                                     \
    mrb_iv_set(mrb                                                                                        \
        , self                                                                                            \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                                           \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                              \
            , mrb->object_class                                                                           \
            , &mrb_cgroup_context_type                                                                    \
            , (void *)mrb_cg_cxt)                                                                         \
        )                                                                                                 \
    );                                                                                                    \
    return self;                                                                                          \
}

SET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_bps_device);
SET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_bps_device);
SET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_iops_device);
SET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_iops_device);

//
// cgroup_set_value_string (a number of keys are 2)
//
#define SET_VALUE_STRING_MRB_CGROUP_KEY2_NOT_USE_GNAME(gname, key1, key2) \
static mrb_value mrb_cgroup_set_##gname##_##key1##_##key2(mrb_state *mrb, mrb_value self)                 \
{                                                                                                         \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");             \
    int code;                                                                                             \
    char *val;                                                                                            \
    mrb_get_args(mrb, "z", &val);                                                                         \
    if ((code = cgroup_set_value_string(mrb_cg_cxt->cgc , #key1 "." #key2 , val)) != 0) {                 \
        mrb_raisef(mrb                                                                                    \
            , E_RUNTIME_ERROR                                                                             \
            , "cgroup_set_value_string " #key1 "." #key2 " failed: %S(%S)"                                \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                                \
            , mrb_fixnum_value(code)                                                                      \
        );                                                                                                \
    }                                                                                                     \
    mrb_iv_set(mrb                                                                                        \
        , self                                                                                            \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                                      \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                              \
            , mrb->object_class                                                                           \
            , &mrb_cgroup_context_type                                                                    \
            , (void *)mrb_cg_cxt)                                                                         \
        )                                                                                                 \
    );                                                                                                    \
    return self;                                                                                          \
}

SET_VALUE_STRING_MRB_CGROUP_KEY2_NOT_USE_GNAME(memory, cgroup, event_control);

//
// cgroup_set_bool
//
#define SET_VALUE_BOOL_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_set_##gname##_##key(mrb_state *mrb, mrb_value self)                      \
{                                                                                             \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
    mrb_bool val;                                                                              \
    int code;                                                                                 \
    mrb_get_args(mrb, "b", &val);                                                             \
                                                                                              \
    if ((code = cgroup_set_value_bool(mrb_cg_cxt->cgc, #gname "." #key, (bool)val))) {                      \
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_set_value_book " #gname "." #key " failed: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code))); \
    }                                                                                         \
    mrb_iv_set(mrb                                                                            \
        , self                                                                                \
        , mrb_intern_cstr(mrb, "mrb_cgroup_context")                                               \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                  \
            , mrb->object_class                                                               \
            , &mrb_cgroup_context_type                                                        \
            , (void *)mrb_cg_cxt)                                                             \
        )                                                                                     \
    );                                                                                        \
                                                                                              \
    return self;                                                                              \
}

SET_VALUE_BOOL_MRB_CGROUP(memory, oom_control);

#define GET_VALUE_BOOL_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_get_##gname##_##key(mrb_state *mrb, mrb_value self)                      \
{                                                                                             \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
    bool val;                                                                              \
    int code;                                                                                 \
    if ((code = cgroup_get_value_bool(mrb_cg_cxt->cgc, #gname "." #key, &val)) && code != ECGROUPVALUENOTEXIST) {            \
        mrb_raisef(mrb \
            , E_RUNTIME_ERROR \
            , "cgroup_get_value_bool " #gname "." #key " failed: %S(%S)" \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code)) \
            , mrb_fixnum_value(code) \
        ); \
    }                                                                                         \
    if (code == ECGROUPVALUENOTEXIST) {\
        return mrb_nil_value();\
    } else {\
        return mrb_bool_value(val);                                                             \
    }\
}

GET_VALUE_BOOL_MRB_CGROUP(memory, oom_control);

static mrb_value mrb_cgroup_get_cpuacct_obj(mrb_state *mrb, mrb_value self)
{
    mrb_value cpuacct_value;
    struct RClass *cpuacct_class, *cgroup_class;
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");

    cpuacct_value = mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "cpuacct_obj"));
    if (mrb_nil_p(cpuacct_value)) {
        cgroup_class = mrb_class_get(mrb, "Cgroup");
        cpuacct_class = (struct RClass*)mrb_class_ptr(mrb_const_get(mrb, mrb_obj_value(cgroup_class), mrb_intern_cstr(mrb, "CPUACCT")));
        cpuacct_value = mrb_obj_new(mrb, cpuacct_class, 1, &mrb_cg_cxt->group_name);
        mrb_iv_set(mrb, self, mrb_intern_cstr(mrb, "cpuacct_obj"), cpuacct_value);
    }
    return cpuacct_value;
}

void mrb_mruby_cgroup_gem_init(mrb_state *mrb)
{
    struct RClass *cgroup;
    struct RClass *cpu;
    struct RClass *cpuacct;
    struct RClass *cpuset;
    struct RClass *blkio;
    struct RClass *memory;

    cgroup = mrb_define_module(mrb, "Cgroup");
    mrb_define_module_function(mrb, cgroup, "create", mrb_cgroup_create, ARGS_NONE());
    // BUG? cgroup_modify_cgroup fail fclose on cg_set_control_value: line:1389 when get existing cgroup controller
    //mrb_define_module_function(mrb, cgroup, "modify", mrb_cgroup_modify, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "modify", mrb_cgroup_create, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "open", mrb_cgroup_create, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "delete", mrb_cgroup_delete, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "close", mrb_cgroup_delete, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "attach", mrb_cgroup_attach, ARGS_OPT(1));
    //mrb_define_module_function(mrb, cgroup, "path", mrb_cgroup_get_current_path, ARGS_OPT(1));
    mrb_define_module_function(mrb, cgroup, "exist?", mrb_cgroup_exist_p, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "attach", mrb_cgroup_attach, ARGS_ANY());
    DONE;

    cpu = mrb_define_class_under(mrb, cgroup, "CPU", mrb->object_class);
    mrb_include_module(mrb, cpu, mrb_module_get(mrb, "Cgroup"));
    mrb_define_method(mrb, cpu, "initialize", mrb_cgroup_cpu_init, ARGS_ANY());
    mrb_define_method(mrb, cpu, "cfs_quota_us=", mrb_cgroup_set_cpu_cfs_quota_us, ARGS_ANY());
    mrb_define_method(mrb, cpu, "cfs_quota_us", mrb_cgroup_get_cpu_cfs_quota_us, ARGS_NONE());
    mrb_define_method(mrb, cpu, "cfs_period_us=", mrb_cgroup_set_cpu_cfs_period_us, ARGS_ANY());
    mrb_define_method(mrb, cpu, "cfs_period_us",  mrb_cgroup_get_cpu_cfs_period_us, ARGS_NONE());
    mrb_define_method(mrb, cpu, "rt_period_us=", mrb_cgroup_set_cpu_rt_period_us, ARGS_ANY());
    mrb_define_method(mrb, cpu, "rt_period_us",  mrb_cgroup_get_cpu_rt_period_us, ARGS_NONE());
    mrb_define_method(mrb, cpu, "rt_runtime_us=", mrb_cgroup_set_cpu_rt_runtime_us, ARGS_ANY());
    mrb_define_method(mrb, cpu, "rt_runtime_us",  mrb_cgroup_get_cpu_rt_runtime_us, ARGS_NONE());
    mrb_define_method(mrb, cpu, "shares=", mrb_cgroup_set_cpu_shares, ARGS_ANY());
    mrb_define_method(mrb, cpu, "shares", mrb_cgroup_get_cpu_shares, ARGS_NONE());
    mrb_define_method(mrb, cpu, "stat", mrb_cgroup_get_cpu_stat, ARGS_NONE());
    mrb_define_method(mrb, cpu, "cpuacct", mrb_cgroup_get_cpuacct_obj, ARGS_NONE());
    DONE;

    cpuacct = mrb_define_class_under(mrb, cgroup, "CPUACCT", mrb->object_class);
    mrb_include_module(mrb, cpuacct, mrb_module_get(mrb, "Cgroup"));
    mrb_define_method(mrb, cpuacct, "initialize", mrb_cgroup_cpuacct_init, ARGS_REQ(1));
    mrb_define_method(mrb, cpuacct, "stat", mrb_cgroup_get_cpuacct_stat, ARGS_NONE());
    mrb_define_method(mrb, cpuacct, "usage", mrb_cgroup_get_cpuacct_usage, ARGS_NONE());
    mrb_define_method(mrb, cpuacct, "usage=", mrb_cgroup_set_cpuacct_usage, ARGS_REQ(1));
    mrb_define_method(mrb, cpuacct, "usage_percpu",  mrb_cgroup_get_cpuacct_usage_percpu, ARGS_NONE());
    DONE;

    cpuset = mrb_define_class_under(mrb, cgroup, "CPUSET", mrb->object_class);
    mrb_include_module(mrb, cpuset, mrb_module_get(mrb, "Cgroup"));
    mrb_define_method(mrb, cpuset, "initialize", mrb_cgroup_cpuset_init, ARGS_ANY());
    mrb_define_method(mrb, cpuset, "cpus=", mrb_cgroup_set_cpuset_cpus, ARGS_REQ(1));
    mrb_define_method(mrb, cpuset, "cpus",  mrb_cgroup_get_cpuset_cpus, ARGS_NONE());
    mrb_define_method(mrb, cpuset, "mems=", mrb_cgroup_set_cpuset_mems, ARGS_REQ(1));
    mrb_define_method(mrb, cpuset, "mems",  mrb_cgroup_get_cpuset_mems, ARGS_NONE());
    DONE;

    blkio = mrb_define_class_under(mrb, cgroup, "BLKIO", mrb->object_class);
    mrb_include_module(mrb, blkio, mrb_module_get(mrb, "Cgroup"));
    mrb_define_method(mrb, blkio, "initialize", mrb_cgroup_blkio_init, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_read_bps_device=", mrb_cgroup_set_blkio_throttle_read_bps_device, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_read_bps_device", mrb_cgroup_get_blkio_throttle_read_bps_device, ARGS_NONE());
    mrb_define_method(mrb, blkio, "throttle_write_bps_device=", mrb_cgroup_set_blkio_throttle_write_bps_device, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_write_bps_device", mrb_cgroup_get_blkio_throttle_write_bps_device, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_read_iops_device=", mrb_cgroup_set_blkio_throttle_read_iops_device, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_read_iops_device", mrb_cgroup_get_blkio_throttle_read_iops_device, ARGS_NONE());
    mrb_define_method(mrb, blkio, "throttle_write_iops_device=", mrb_cgroup_set_blkio_throttle_write_iops_device, ARGS_ANY());
    mrb_define_method(mrb, blkio, "throttle_write_iops_device", mrb_cgroup_get_blkio_throttle_write_iops_device, ARGS_NONE());
    DONE;

    memory = mrb_define_class_under(mrb, cgroup, "MEMORY", mrb->object_class);
    mrb_include_module(mrb, memory, mrb_module_get(mrb, "Cgroup"));
    mrb_define_method(mrb, memory, "initialize", mrb_cgroup_memory_init, ARGS_ANY());
    mrb_define_method(mrb, memory, "limit_in_bytes=", mrb_cgroup_set_memory_limit_in_bytes, ARGS_ANY());
    mrb_define_method(mrb, memory, "limit_in_bytes", mrb_cgroup_get_memory_limit_in_bytes, ARGS_NONE());
    mrb_define_method(mrb, memory, "usage_in_bytes", mrb_cgroup_get_memory_usage_in_bytes, ARGS_NONE());
    mrb_define_method(mrb, memory, "max_usage_in_bytes", mrb_cgroup_get_memory_max_usage_in_bytes, ARGS_NONE());
    mrb_define_method(mrb, memory, "cgroup_event_control=", mrb_cgroup_set_memory_cgroup_event_control, ARGS_ANY());
    mrb_define_method(mrb, memory, "oom_control=", mrb_cgroup_set_memory_oom_control, ARGS_ANY());
    mrb_define_method(mrb, memory, "oom_control", mrb_cgroup_get_memory_oom_control, ARGS_NONE());
    DONE;
}

void mrb_mruby_cgroup_gem_final(mrb_state *mrb)
{
}

