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

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/variable.h" 
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/class.h"

#define BLKIO_STRING_SIZE 64
#define DONE mrb_gc_arena_restore(mrb, 0);

typedef struct cgroup cgroup_t;
typedef struct cgroup_controller cgroup_controller_t;
typedef struct {
    int new;
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

static mrb_cgroup_context *mrb_cgroup_get_context(mrb_state *mrb,  mrb_value self, char *ctx_flag)
{
    mrb_cgroup_context *c;
    mrb_value context;

    context = mrb_iv_get(mrb, self, mrb_intern(mrb, ctx_flag));
    Data_Get_Struct(mrb, context, &mrb_cgroup_context_type, c);
    if (!c)
        mrb_raise(mrb, E_RUNTIME_ERROR, "get mrb_cgroup_context failed");

    return c;
}

//
// group
//

static mrb_value mrb_cgroup_modify(mrb_state *mrb, mrb_value self)
{   
    int code;
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");

    // BUG? : cgroup_modify_cgroup returns an error(Invalid argument), when run modify method from other instance.
    if ((code = cgroup_modify_cgroup(mrb_cg_cxt->cg))) {
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_modify faild: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code)));
    }
    mrb_iv_set(mrb
        , self
        , mrb_intern(mrb, "mrb_cgroup_context")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &mrb_cgroup_context_type
            , (void *)mrb_cg_cxt)
        )
    );

    return self;
}

static mrb_value mrb_cgroup_create(mrb_state *mrb, mrb_value self)
{   
    int code;
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");

    // BUG? : cgroup_create_cgroup returns an error(This kernel does not support this feature), despite actually succeeding
    if ((code = cgroup_create_cgroup(mrb_cg_cxt->cg, 1))) {
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_create faild: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code)));
    }
    mrb_iv_set(mrb
        , self
        , mrb_intern(mrb, "mrb_cgroup_context")
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

    // BUG? : cgroup_delete_cgroup returns an error(No such file or directory), despite actually succeeding
    if ((code = cgroup_delete_cgroup(mrb_cg_cxt->cg, 1))) {
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_delete faild: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code)));
    }

    return self;
}

static mrb_value mrb_cgroup_exist_p(mrb_state *mrb, mrb_value self)
{
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");
    return (mrb_cg_cxt->new) ? mrb_false_value(): mrb_true_value();
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
        , mrb_intern(mrb, "mrb_cgroup_context")
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
//
#define SET_MRB_CGROUP_INIT_GROUP(gname) \
static mrb_value mrb_cgroup_##gname##_init(mrb_state *mrb, mrb_value self)                                     \
{                                                                                                       \
    mrb_cgroup_context *mrb_cg_cxt = (mrb_cgroup_context *)mrb_malloc(mrb, sizeof(mrb_cgroup_context)); \
    mrb_value group_name;                                                                               \
                                                                                                        \
    if (cgroup_init()) {                                                                                \
        mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_init " #gname " failed");                                \
    }                                                                                                   \
    mrb_get_args(mrb, "o", &group_name);                                                                \
    mrb_cg_cxt->cg = cgroup_new_cgroup(RSTRING_PTR(group_name));                                        \
    if (mrb_cg_cxt->cg == NULL) {                                                                       \
        mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_new_cgroup failed");                                     \
    }                                                                                                   \
                                                                                                        \
    if (cgroup_get_cgroup(mrb_cg_cxt->cg)) {                                                            \
        mrb_cg_cxt->new = 1;                                                                            \
        mrb_cg_cxt->cgc = cgroup_add_controller(mrb_cg_cxt->cg, #gname);                                \
        if (mrb_cg_cxt->cgc == NULL) {                                                                  \
            mrb_raise(mrb, E_RUNTIME_ERROR, "cgoup_add_controller " #gname " failed");                  \
        }                                                                                               \
    } else {                                                                                            \
        mrb_cg_cxt->new = 0;                                                                            \
        mrb_cg_cxt->cgc = cgroup_get_controller(mrb_cg_cxt->cg, #gname);                                \
        if (mrb_cg_cxt->cgc == NULL) {                                                                  \
            mrb_cg_cxt->cgc = cgroup_add_controller(mrb_cg_cxt->cg, #gname);                            \
            if (mrb_cg_cxt->cgc == NULL) {                                                              \
                mrb_raise(mrb, E_RUNTIME_ERROR, "get_cgroup success, but add_controller "  #gname " failed"); \
            }                                                                                           \
        }                                                                                               \
    }                                                                                                   \
    mrb_iv_set(mrb                                                                                      \
        , self                                                                                          \
        , mrb_intern(mrb, "mrb_cgroup_context")                                                         \
        , mrb_obj_value(Data_Wrap_Struct(mrb                                                            \
            , mrb->object_class                                                                         \
            , &mrb_cgroup_context_type                                                                  \
            , (void *)mrb_cg_cxt)                                                                       \
        )                                                                                               \
    );                                                                                                  \
                                                                                                        \
    return self;                                                                                        \
}

SET_MRB_CGROUP_INIT_GROUP(cpu);
SET_MRB_CGROUP_INIT_GROUP(blkio);

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
        , mrb_intern(mrb, "mrb_cgroup_context")                                               \
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
SET_VALUE_INT64_MRB_CGROUP(cpu, shares);

#define GET_VALUE_INT64_MRB_CGROUP(gname, key) \
static mrb_value mrb_cgroup_get_##gname##_##key(mrb_state *mrb, mrb_value self)                      \
{                                                                                             \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
    int64_t val;                                                                              \
    int code;                                                                                 \
    if ((code = cgroup_get_value_int64(mrb_cg_cxt->cgc, #gname "." #key, &val))) {            \
        mrb_raisef(mrb, E_RUNTIME_ERROR, "cgroup_get_value_int64 " #gname "." #key " failed: %S", mrb_str_new_cstr(mrb, cgroup_strerror(code))); \
    }                                                                                         \
    return mrb_fixnum_value(val);                                                             \
}

GET_VALUE_INT64_MRB_CGROUP(cpu, cfs_quota_us);
GET_VALUE_INT64_MRB_CGROUP(cpu, shares);

static mrb_value mrb_cgroup_loop(mrb_state *mrb, mrb_value self)
{
    long i = 0;
    long n;
    mrb_value c;
    mrb_get_args(mrb, "o", &c);
    n = atol(RSTRING_PTR(c));
    printf("%ld\n", n);
    
    while (i < n + 1000000000000000) {
        i++;
    }
    printf("%ld\n", i);

    return self;
}

//
// cgroup_get_value_string (a number of keys are 2)
//
#define GET_VALUE_STRING_MRB_CGROUP_KEY2(gname, key1, key2) \
static mrb_value mrb_cgroup_get_##gname##_##key1##_##key2(mrb_state *mrb, mrb_value self)             \
{                                                                                             \
    int code;                                                                                 \
    char *val;                                                                                \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context"); \
                                                                                              \
    if ((code = cgroup_get_value_string(mrb_cg_cxt->cgc , #gname "." #key1 "." #key2, &val)) != 0) {    \
        mrb_raisef(mrb                                                                        \
            , E_RUNTIME_ERROR                                                                 \
            , "cgroup_get_value_string " #gname "." #key1 "." #key2 " failed: %S"             \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                    \
        );                                                                                    \
    }                                                                                         \
    if (strcmp(val, "")) {                                                                    \
        return mrb_str_new_cstr(mrb, val);                                                    \
    } else {                                                                                  \
        return  mrb_nil_value();                                                              \
    }                                                                                         \
}

GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_bps_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_bps_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, read_iops_device);
GET_VALUE_STRING_MRB_CGROUP_KEY2(blkio, throttle, write_iops_device);

#define SET_VALUE_STRING_MRB_CGROUP_KEY2(gname, key1, key2) \
static mrb_value mrb_cgroup_set_##gname##_##key1##_##key2(mrb_state *mrb, mrb_value self)                        \
{                                                                                                         \
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self, "mrb_cgroup_context");             \
    int code;                                                                                             \
    char *val;                                                                                            \
    mrb_get_args(mrb, "z", &val);                                                                         \
    if ((code = cgroup_add_value_string(mrb_cg_cxt->cgc , #gname "." #key1 "." #key2, val)) != 0) {       \
        if ((code = cgroup_set_value_string(mrb_cg_cxt->cgc , #gname "." #key1 "." #key2 , val)) != 0) {  \
            mrb_raisef(mrb                                                                                \
                , E_RUNTIME_ERROR                                                                         \
                , "cgroup_set_value_string " #gname "." #key1 "." #key2 " failed: %S"                     \
                , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                            \
            );                                                                                            \
        }                                                                                                 \
    } else {                                                                                              \
        mrb_raisef(mrb                                                                                    \
            , E_RUNTIME_ERROR                                                                             \
            , "cgroup_add_value_string " #gname "." #key1 "." #key2 " failed: %S"                         \
            , mrb_str_new_cstr(mrb, cgroup_strerror(code))                                                \
        );                                                                                                \
    }                                                                                                     \
    mrb_iv_set(mrb                                                                                        \
        , self                                                                                            \
        , mrb_intern(mrb, "mrb_cgroup_context")                                                           \
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

void mrb_mruby_cgroup_gem_init(mrb_state *mrb)
{
    struct RClass *cgroup;

    cgroup = mrb_define_module(mrb, "Cgroup");
    mrb_define_module_function(mrb, cgroup, "create", mrb_cgroup_create, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "open", mrb_cgroup_create, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "delete", mrb_cgroup_delete, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "close", mrb_cgroup_delete, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "attach", mrb_cgroup_attach, ARGS_ANY());
    mrb_define_module_function(mrb, cgroup, "modify", mrb_cgroup_modify, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "exist?", mrb_cgroup_exist_p, ARGS_NONE());
    mrb_define_module_function(mrb, cgroup, "attach", mrb_cgroup_attach, ARGS_ANY());
    mrb_define_module_function(mrb, cgroup, "loop", mrb_cgroup_loop, ARGS_ANY());
    DONE;

    struct RClass *cpu;
    cpu = mrb_define_class_under(mrb, cgroup, "CPU", mrb->object_class);
    mrb_include_module(mrb, cpu, mrb_class_get(mrb, "Cgroup"));
    mrb_define_method(mrb, cpu, "initialize", mrb_cgroup_cpu_init, ARGS_ANY());
    mrb_define_method(mrb, cpu, "cfs_quota_us=", mrb_cgroup_set_cpu_cfs_quota_us, ARGS_ANY());
    mrb_define_method(mrb, cpu, "cfs_quota_us", mrb_cgroup_get_cpu_cfs_quota_us, ARGS_NONE());
    mrb_define_method(mrb, cpu, "shares=", mrb_cgroup_set_cpu_shares, ARGS_ANY());
    mrb_define_method(mrb, cpu, "shares", mrb_cgroup_get_cpu_shares, ARGS_NONE());
    //mrb_define_method(mrb, cpu, "loop", mrb_cgroup_loop, ARGS_ANY());
    DONE;

    struct RClass *blkio;
    blkio = mrb_define_class_under(mrb, cgroup, "BLKIO", mrb->object_class);
    mrb_include_module(mrb, blkio, mrb_class_get(mrb, "Cgroup"));
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
}

void mrb_mruby_cgroup_gem_final(mrb_state *mrb)
{
}

