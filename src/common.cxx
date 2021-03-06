﻿/*
 * Copyright 2016-2017 libfptu authors: please see AUTHORS file.
 *
 * This file is part of libfptu, aka "Fast Positive Tuples".
 *
 * libfptu is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libfptu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfptu.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fast_positive/tuples_internal.h"

__hot size_t fptu_field_units(const fptu_field *pf) {
  unsigned type = fptu_get_type(pf->ct);
  if (likely(type < fptu_cstr)) {
    // fixed length type
    return fptu_internal_map_t2u[type];
  }

  // variable length type
  const fptu_payload *payload = fptu_field_payload(pf);
  if (type == fptu_cstr) {
    // length is not stored, but zero terminated
    return bytes2units(strlen(payload->cstr) + 1);
  }

  // length is stored
  return payload->other.varlen.brutto + (size_t)1;
}

//----------------------------------------------------------------------------

__hot const fptu_field *fptu_lookup_ro(fptu_ro ro, unsigned column,
                                       int type_or_filter) {
  if (unlikely(ro.total_bytes < fptu_unit_size))
    return nullptr;
  if (unlikely(ro.total_bytes !=
               fptu_unit_size +
                   fptu_unit_size * (size_t)ro.units[0].varlen.brutto))
    return nullptr;
  if (unlikely(column > fptu_max_cols))
    return nullptr;

  const fptu_field *begin = &ro.units[1].field;
  const fptu_field *end =
      begin + (ro.units[0].varlen.tuple_items & fptu_lt_mask);

  if (fptu_lx_mask & ro.units[0].varlen.tuple_items) {
    // TODO: support for ordered tuples
  }

  if (type_or_filter & fptu_filter) {
    for (const fptu_field *pf = begin; pf < end; ++pf) {
      if (fptu_ct_match(pf, column, type_or_filter))
        return pf;
    }
  } else {
    uint_fast16_t ct = fptu_pack_coltype(column, type_or_filter);
    for (const fptu_field *pf = begin; pf < end; ++pf) {
      if (pf->ct == ct)
        return pf;
    }
  }
  return nullptr;
}

__hot fptu_field *fptu_lookup_ct(fptu_rw *pt, uint_fast16_t ct) {
  const fptu_field *begin = &pt->units[pt->head].field;
  const fptu_field *pivot = &pt->units[pt->pivot].field;
  for (const fptu_field *pf = begin; pf < pivot; ++pf) {
    if (pf->ct == ct)
      return (fptu_field *)pf;
  }
  return nullptr;
}

__hot fptu_field *fptu_lookup(fptu_rw *pt, unsigned column,
                              int type_or_filter) {
  if (unlikely(column > fptu_max_cols))
    return nullptr;

  if (type_or_filter & fptu_filter) {
    const fptu_field *begin = &pt->units[pt->head].field;
    const fptu_field *pivot = &pt->units[pt->pivot].field;
    for (const fptu_field *pf = begin; pf < pivot; ++pf) {
      if (fptu_ct_match(pf, column, type_or_filter))
        return (fptu_field *)pf;
    }
    return nullptr;
  }

  return fptu_lookup_ct(pt, fptu_pack_coltype(column, type_or_filter));
}

//----------------------------------------------------------------------------

__hot fptu_ro fptu_take_noshrink(const fptu_rw *pt) {
  static_assert(offsetof(fptu_ro, units) == offsetof(iovec, iov_base) &&
                    offsetof(fptu_ro, total_bytes) ==
                        offsetof(iovec, iov_len) &&
                    sizeof(fptu_ro) == sizeof(iovec),
                "unexpected struct iovec");

  fptu_ro tuple;
  assert(pt->head > 0);
  assert(pt->tail - pt->head <= UINT16_MAX);
  fptu_payload *payload = (fptu_payload *)&pt->units[pt->head - 1];
  payload->other.varlen.brutto = (uint16_t)(pt->tail - pt->head);
  payload->other.varlen.tuple_items = (uint16_t)(pt->pivot - pt->head);
  // TODO: support for ordered tuples
  tuple.units = (const fptu_unit *)payload;
  tuple.total_bytes = (size_t)((char *)&pt->units[pt->tail] - (char *)payload);
  return tuple;
}
