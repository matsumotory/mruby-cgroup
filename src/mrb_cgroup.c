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

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/variable.h" 
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/class.h"


typedef struct cgroup cgroup_t;
typedef struct cgroup_controller cgroup_controller_t;
typedef struct {
    cgroup_t *cg;
    cgroup_controller_t *cgc;
} mrb_cgroup_context;

static void mrb_cgroup_context_free(mrb_state *mrb, mrb_cgroup_context *c)
{
//    cgroup_free_controllers(c->cg);
    cgroup_free(&c->cg);
}

static const struct mrb_data_type mrb_cgroup_context_type = {
    "mrb_cgroup_context", mrb_cgroup_context_free,
};

static mrb_cgroup_context *mrb_cgroup_get_context(mrb_state *mrb,  mrb_value self)
{
    mrb_cgroup_context *c;
    mrb_value context;

    context = mrb_iv_get(mrb, self, mrb_intern(mrb, "mrb_cgroup_context"));
    Data_Get_Struct(mrb, context, &mrb_cgroup_context_type, c);
    if (!c)
        mrb_raise(mrb, E_RUNTIME_ERROR, "get mrb_cgroup_context failed");

    return c;
}

mrb_value mrb_cgroup_cpu_delete(mrb_state *mrb, mrb_value self)
{
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self);

    cgroup_delete_cgroup(mrb_cg_cxt->cg, 1);

    return self;
}

mrb_value mrb_cgroup_cpu_init(mrb_state *mrb, mrb_value self)
{   
    mrb_cgroup_context *mrb_cg_cxt = (mrb_cgroup_context *)malloc(sizeof(mrb_cgroup_context));
    mrb_value group_name;

    mrb_get_args(mrb, "o", &group_name);

    cgroup_init();
    mrb_cg_cxt->cg = cgroup_new_cgroup(RSTRING_PTR(group_name));
    mrb_cg_cxt->cgc = cgroup_add_controller(mrb_cg_cxt->cg, "cpu");
    mrb_iv_set(mrb
        , self
        , mrb_intern(mrb, "mrb_cgroup_context")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &mrb_cgroup_context_type
            , mrb_cg_cxt)
        )
    );

    return self;
}

mrb_value mrb_cgroup_cpu_rate(mrb_state *mrb, mrb_value self)
{   
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self);
    mrb_int cfs_quota_us;
    mrb_get_args(mrb, "i", &cfs_quota_us);

    cgroup_add_value_int64(mrb_cg_cxt->cgc, "cpu.cfs_quota_us", cfs_quota_us);
    cgroup_create_cgroup(mrb_cg_cxt->cg, 1);
    mrb_iv_set(mrb
        , self
        , mrb_intern(mrb, "mrb_cgroup_context")
        , mrb_obj_value(Data_Wrap_Struct(mrb
            , mrb->object_class
            , &mrb_cgroup_context_type
            , mrb_cg_cxt)
        )
    );

    return self;
}

mrb_value mrb_cgroup_loop(mrb_state *mrb, mrb_value self)
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

mrb_value mrb_cgroup_attach(mrb_state *mrb, mrb_value self)
{   
    mrb_cgroup_context *mrb_cg_cxt = mrb_cgroup_get_context(mrb, self);
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
            , mrb_cg_cxt)
        )
    );

    return self;
}

void mrb_mruby_cgroup_gem_init(mrb_state *mrb)
{
    struct RClass *cgroup;

    cgroup = mrb_define_module(mrb, "Cgroup");

    struct RClass *cpu;
    cpu = mrb_define_class_under(mrb, cgroup, "CPU", mrb->object_class);
    mrb_define_method(mrb, cpu, "initialize", mrb_cgroup_cpu_init, ARGS_ANY());
    mrb_define_method(mrb, cpu, "delete", mrb_cgroup_cpu_delete, ARGS_NONE());
    mrb_define_method(mrb, cpu, "close", mrb_cgroup_cpu_delete, ARGS_NONE());
    mrb_define_method(mrb, cpu, "rate=", mrb_cgroup_cpu_rate, ARGS_ANY());
    mrb_define_method(mrb, cpu, "attach", mrb_cgroup_attach, ARGS_ANY());
    mrb_define_method(mrb, cpu, "loop", mrb_cgroup_loop, ARGS_ANY());

}
