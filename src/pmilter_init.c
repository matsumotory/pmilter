#include <mruby.h>
#include <mruby/compile.h>

#include "pmilter.h"

#define GC_ARENA_RESTORE mrb_gc_arena_restore(mrb, 0);

extern void pmilter_mrb_core_class_init(mrb_state *mrb, struct RClass *calss);
extern void pmilter_mrb_session_class_init(mrb_state *mrb, struct RClass *calss);

void pmilter_mrb_class_init(mrb_state *mrb)
{
  struct RClass *class;

  class = mrb_define_class(mrb, "Pmilter", mrb->object_class);

  pmilter_mrb_core_class_init(mrb, class);
  GC_ARENA_RESTORE;
  pmilter_mrb_session_class_init(mrb, class);
  GC_ARENA_RESTORE;
}
