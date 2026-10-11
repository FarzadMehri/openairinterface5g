/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "slicing.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"

static const char *resource_type_str[] = {"both", "dl", "ul"};

static bool applies_to(nr_slice_resource_type_t rt, nr_slice_resource_type_t dir)
{
  return rt == NR_SLICE_RESOURCE_BOTH || rt == dir;
}

/** @brief Admission policy, cf. TS 28.541 RRMPolicyRatio: dedicatedRatio <= minRatio <= maxRatio, and
 * the sum of dedicatedRatio and of minRatio of all slices per direction must not exceed 100 */
static bool admit_slice(const seq_arr_t *slices, const nr_slice_policy_t *p)
{
  if (seq_arr_size(&p->members) == 0) {
    LOG_W(NR_MAC, "slice rejected: no policy member\n");
    return false;
  }
  if (p->dedicated_ratio > p->min_ratio || p->min_ratio > p->max_ratio) {
    LOG_W(NR_MAC,
          "slice rejected: need dedicatedRatio (%.2f) <= minRatio (%.2f) <= maxRatio (%.2f)\n",
          p->dedicated_ratio,
          p->min_ratio,
          p->max_ratio);
    return false;
  }
  const nr_slice_resource_type_t dirs[] = {NR_SLICE_RESOURCE_DL, NR_SLICE_RESOURCE_UL};
  for (int d = 0; d < sizeofArray(dirs); ++d) {
    if (!applies_to(p->resource_type, dirs[d]))
      continue;
    float sum_dedicated = p->dedicated_ratio;
    float sum_min = p->min_ratio;
    for (int i = 0; i < seq_arr_size(slices); ++i) {
      const nr_slice_policy_t *s = seq_arr_at(slices, i);
      if (!applies_to(s->resource_type, dirs[d]))
        continue;
      sum_dedicated += s->dedicated_ratio;
      sum_min += s->min_ratio;
    }
    if (sum_dedicated > 100.0f || sum_min > 100.0f) {
      LOG_W(NR_MAC,
            "slice rejected: sum of %s dedicatedRatio %.2f or minRatio %.2f exceeds 100\n",
            resource_type_str[dirs[d]],
            sum_dedicated,
            sum_min);
      return false;
    }
  }
  return true;
}

/** @brief Add a slice to the cell, if admitted. Members of policy are copied.
 * @return true if the slice has been admitted and added, false otherwise */
bool add_slice(nr_cell_sched_t *cell, const nr_slice_policy_t *policy)
{
  if (!admit_slice(&cell->slices, policy))
    return false;

  nr_slice_policy_t s = *policy;
  s.id = ++cell->last_slice_id;
  seq_arr_init(&s.members, sizeof(nr_slice_policy_member_t));
  for (int m = 0; m < seq_arr_size(&policy->members); ++m)
    seq_arr_push_back(&s.members, seq_arr_at(&policy->members, m), sizeof(nr_slice_policy_member_t));
  seq_arr_push_back(&cell->slices, &s, sizeof(s));

  LOG_I(NR_MAC,
        "added slice %d: resource type %s, dedicated %.2f%% min %.2f%% max %.2f%%\n",
        s.id,
        resource_type_str[s.resource_type],
        s.dedicated_ratio,
        s.min_ratio,
        s.max_ratio);
  for (int m = 0; m < seq_arr_size(&s.members); ++m) {
    const nr_slice_policy_member_t *pm = seq_arr_at(&s.members, m);
    LOG_I(NR_MAC,
          "  member %d: PLMN %03d.%0*d SST %d SD 0x%06x\n",
          m,
          pm->plmn.mcc,
          pm->plmn.mnc_digit_length,
          pm->plmn.mnc,
          pm->nssai.sst,
          pm->nssai.sd);
  }
  return true;
}

static void free_slice(void *it)
{
  nr_slice_policy_t *s = it;
  seq_arr_free(&s->members, NULL);
}

/** @brief Remove a slice from the cell, if no connected UE is associated to it anymore
 * @param slice pointer to the slice in cell->slices
 * @return true if the slice has been removed, false otherwise */
bool remove_slice(const NR_UEs_t *UEs, nr_cell_sched_t *cell, nr_slice_policy_t *slice)
{
  DevAssert(seq_arr_dist(&cell->slices, seq_arr_front(&cell->slices), slice) < seq_arr_size(&cell->slices));
  UE_iterator ((NR_UE_info_t **)UEs->connected_ue_list, UE) {
    if (UE->pcell == cell && (UE->dl_slice_id == slice->id || UE->ul_slice_id == slice->id)) {
      LOG_W(NR_MAC, "cannot remove slice %d: UE %04x still associated\n", slice->id, UE->rnti);
      return false;
    }
  }
  LOG_I(NR_MAC, "removing slice %d\n", slice->id);
  seq_arr_erase_deep(&cell->slices, slice, free_slice);
  return true;
}

static bool eq_plmn(const plmn_id_t *a, const plmn_id_t *b)
{
  return a->mcc == b->mcc && a->mnc == b->mnc && a->mnc_digit_length == b->mnc_digit_length;
}

/** @brief Get the slice for PLMN and S-NSSAI in direction dir (DL or UL)
 * @return the first slice containing a matching policy member, or NULL if none */
const nr_slice_policy_t *get_slice(const nr_cell_sched_t *cell,
                                   const plmn_id_t *plmn,
                                   const nssai_t *nssai,
                                   nr_slice_resource_type_t dir)
{
  DevAssert(dir == NR_SLICE_RESOURCE_DL || dir == NR_SLICE_RESOURCE_UL);
  for (int i = 0; i < seq_arr_size(&cell->slices); ++i) {
    const nr_slice_policy_t *s = seq_arr_at(&cell->slices, i);
    if (!applies_to(s->resource_type, dir))
      continue;
    for (int m = 0; m < seq_arr_size(&s->members); ++m) {
      const nr_slice_policy_member_t *pm = seq_arr_at(&s->members, m);
      if (eq_plmn(&pm->plmn, plmn) && pm->nssai.sst == nssai->sst && pm->nssai.sd == nssai->sd)
        return s;
    }
  }
  return NULL;
}
