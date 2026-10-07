/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_MAC_GNB_SLICING_H_
#define NR_MAC_GNB_SLICING_H_

#include <stdbool.h>
#include "common/5g_platform_types.h"
#include "common/utils/ds/seq_arr.h"

/// Maximum number of slices
#define NR_MAX_SLICES 512

/// Resource type of an RRM policy, cf. TS 28.541 resourceType (PRB, PRB_DL, PRB_UL)
typedef enum nr_slice_resource_type_e {
  NR_SLICE_RESOURCE_BOTH,
  NR_SLICE_RESOURCE_DL,
  NR_SLICE_RESOURCE_UL,
} nr_slice_resource_type_t;

/// RRM policy member, cf. TS 28.541 RRMPolicyMember
typedef struct nr_slice_policy_member_s {
  plmn_id_t plmn;
  nssai_t nssai;
} nr_slice_policy_member_t;

/// RRM policy ratio, cf. TS 28.541 RRMPolicyRatio. Ratios in percent [0,100].
typedef struct nr_slice_policy_s {
  nr_slice_resource_type_t resource_type;
  /// list of nr_slice_policy_member_t
  seq_arr_t members;
  float dedicated_ratio;
  float min_ratio;
  float max_ratio;
} nr_slice_policy_t;

#endif /* NR_MAC_GNB_SLICING_H_ */
