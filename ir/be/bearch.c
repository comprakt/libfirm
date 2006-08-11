/**
 * Processor architecture specification.
 * @author Sebastian Hack
 * @date 11.2.2005
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <string.h>

#include "bearch.h"
#include "ircons_t.h"
#include "irnode_t.h"

#include "bitset.h"
#include "pset.h"
#include "entity.h"

#include "irprintf.h"

/* Initialize the architecture environment struct. */
arch_env_t *arch_env_init(arch_env_t *env, const arch_isa_if_t *isa_if, FILE *file_handle)
{
  memset(env, 0, sizeof(*env));
  env->isa = isa_if->init(file_handle);
  return env;
}

arch_env_t *arch_env_push_irn_handler(arch_env_t *env,
    const arch_irn_handler_t *handler)
{
  assert(env->handlers_tos <= ARCH_MAX_HANDLERS);
  env->handlers[env->handlers_tos++] = handler;
  return env;
}

const arch_irn_handler_t *arch_env_pop_irn_handler(arch_env_t *env)
{
  assert(env->handlers_tos > 0 && env->handlers_tos <= ARCH_MAX_HANDLERS);
  return env->handlers[--env->handlers_tos];
}

static const arch_irn_ops_t *fallback_irn_ops = NULL;

int arch_register_class_put(const arch_register_class_t *cls, bitset_t *bs)
{
  if(bs) {
    int i, n;
    for(i = 0, n = cls->n_regs; i < n; ++i)
      bitset_set(bs, i);
  }

  return cls->n_regs;
}

/**
 * Get the isa responsible for a node.
 * @param env The arch environment with the isa stack.
 * @param irn The node to get the responsible isa for.
 * @return The irn operations given by the responsible isa.
 */
static INLINE const arch_irn_ops_t *
get_irn_ops(const arch_env_t *env, const ir_node *irn)
{
  int i;

  for(i = env->handlers_tos - 1; i >= 0; --i) {
    const arch_irn_handler_t *handler = env->handlers[i];
    const arch_irn_ops_t *ops = handler->get_irn_ops(handler, irn);

    if(ops)
      return ops;
  }

  return fallback_irn_ops;
}

const arch_register_req_t *arch_get_register_req(const arch_env_t *env,
    arch_register_req_t *req, const ir_node *irn, int pos)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  req->type = arch_register_req_type_none;
  return ops->impl->get_irn_reg_req(ops, req, irn, pos);
}

void arch_set_frame_offset(const arch_env_t *env, ir_node *irn, int offset)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  ops->impl->set_frame_offset(ops, irn, offset);
}

entity *arch_get_frame_entity(const arch_env_t *env, ir_node *irn)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  return ops->impl->get_frame_entity(ops, irn);
}

void arch_set_frame_entity(const arch_env_t *env, ir_node *irn, entity *ent)
{
	const arch_irn_ops_t *ops = get_irn_ops(env, irn);
	ops->impl->set_frame_entity(ops, irn, ent);
}

arch_inverse_t *arch_get_inverse(const arch_env_t *env, const ir_node *irn, int i, arch_inverse_t *inverse, struct obstack *obstack)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  if(ops->impl->get_inverse) {
    return ops->impl->get_inverse(ops, irn, i, inverse, obstack);
  } else {
    return NULL;
  }
}

int arch_possible_memory_operand(const arch_env_t *env, const ir_node *irn, unsigned int i) {
	const arch_irn_ops_t *ops = get_irn_ops(env, irn);
	if(ops->impl->possible_memory_operand) {
		return ops->impl->possible_memory_operand(ops, irn, i);
	} else {
		return 0;
	}
}

extern void arch_perform_memory_operand(const arch_env_t *env, ir_node *irn, ir_node *reload, unsigned int i) {
	const arch_irn_ops_t *ops = get_irn_ops(env, irn);
	if(ops->impl->perform_memory_operand) {
		ops->impl->perform_memory_operand(ops, irn, reload, i);
	} else {
		return;
	}
}

int arch_get_op_estimated_cost(const arch_env_t *env, const ir_node *irn)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  if(ops->impl->get_op_estimated_cost) {
    return ops->impl->get_op_estimated_cost(ops, irn);
  } else {
    return 1;
  }
}

int arch_is_possible_memory_operand(const arch_env_t *env, const ir_node *irn, int i)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  if(ops->impl->possible_memory_operand) {
    return ops->impl->possible_memory_operand(ops, irn, i);
  } else {
    return 0;
  }
}

int arch_get_allocatable_regs(const arch_env_t *env, const ir_node *irn, int pos, bitset_t *bs)
{
  arch_register_req_t local_req;
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  const arch_register_req_t *req = ops->impl->get_irn_reg_req(ops, &local_req, irn, pos);

  if(req->type == arch_register_req_type_none) {
	  bitset_clear_all(bs);
	  return 0;
  }

  if(arch_register_req_is(req, limited)) {
	  req->limited(req->limited_env, bs);
	  return bitset_popcnt(bs);
  }

  arch_register_class_put(req->cls, bs);
  return req->cls->n_regs;
}

void arch_put_non_ignore_regs(const arch_env_t *env, const arch_register_class_t *cls, bitset_t *bs)
{
	int i;

	for(i = 0; i < cls->n_regs; ++i) {
		if(!arch_register_type_is(&cls->regs[i], ignore))
			bitset_set(bs, i);
	}
}

int arch_count_non_ignore_regs(const arch_env_t *env, const arch_register_class_t *cls)
{
	int i;
	int result = 0;

	for(i = 0; i < cls->n_regs; ++i) {
		if(!arch_register_type_is(&cls->regs[i], ignore))
			result++;
	}

	return result;
}

int arch_is_register_operand(const arch_env_t *env,
    const ir_node *irn, int pos)
{
	arch_register_req_t local_req;
	const arch_irn_ops_t *ops = get_irn_ops(env, irn);
	const arch_register_req_t *req = ops->impl->get_irn_reg_req(ops, &local_req, irn, pos);
	return req != NULL;
}

int arch_reg_is_allocatable(const arch_env_t *env, const ir_node *irn,
    int pos, const arch_register_t *reg)
{
	arch_register_req_t req;

	arch_get_register_req(env, &req, irn, pos);

	if(req.type == arch_register_req_type_none)
		return 0;

	if(arch_register_req_is(&req, limited)) {
		bitset_t *bs = bitset_alloca(req.cls->n_regs);
		req.limited(req.limited_env, bs);
		return bitset_is_set(bs, arch_register_get_index(reg));
	}

	return req.cls == reg->reg_class;
}

const arch_register_class_t *
arch_get_irn_reg_class(const arch_env_t *env, const ir_node *irn, int pos)
{
  arch_register_req_t local_req;
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  const arch_register_req_t *req = ops->impl->get_irn_reg_req(ops, &local_req, irn, pos);
  return req ? req->cls : NULL;
}

extern const arch_register_t *
arch_get_irn_register(const arch_env_t *env, const ir_node *irn)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  return ops->impl->get_irn_reg(ops, irn);
}

extern void arch_set_irn_register(const arch_env_t *env,
    ir_node *irn, const arch_register_t *reg)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  ops->impl->set_irn_reg(ops, irn, reg);
}

extern arch_irn_class_t arch_irn_classify(const arch_env_t *env, const ir_node *irn)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  return ops->impl->classify(ops, irn);
}

extern arch_irn_flags_t arch_irn_get_flags(const arch_env_t *env, const ir_node *irn)
{
  const arch_irn_ops_t *ops = get_irn_ops(env, irn);
  return ops->impl->get_flags(ops, irn);
}

extern const char *arch_irn_flag_str(arch_irn_flags_t fl)
{
	switch(fl) {
#define XXX(x) case arch_irn_flags_ ## x: return #x;
		XXX(dont_spill);
		XXX(ignore);
		XXX(rematerializable);
		XXX(modify_sp);
		XXX(none);
#undef XXX
	}
	return "n/a";
}

extern char *arch_register_req_format(char *buf, size_t len, const arch_register_req_t *req)
{
	char tmp[128];
	snprintf(buf, len, "class: %s", req->cls->name);

	if(arch_register_req_is(req, limited)) {
		bitset_pos_t elm;
		bitset_t *bs = bitset_alloca(req->cls->n_regs);
		req->limited(req->limited_env, bs);
		strncat(buf, " limited:", len);
		bitset_foreach(bs, elm) {
			strncat(buf, " ", len);
			strncat(buf, req->cls->regs[elm].name, len);
		}
	}

	if(arch_register_req_is(req, should_be_same)) {
		ir_snprintf(tmp, sizeof(tmp), " same to: %+F", req->other_different);
		strncat(buf, tmp, len);
	}

	if(arch_register_req_is(req, should_be_different)) {
		ir_snprintf(tmp, sizeof(tmp), " different to: %+F", req->other_different);
		strncat(buf, tmp, len);
	}

	return buf;
}
