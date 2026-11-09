/***************************************************************************
 # Copyright (c) 2026, NVIDIA CORPORATION.  All rights reserved.
 #
 # NVIDIA CORPORATION and its licensors retain all intellectual property
 # and proprietary rights in and to this software, related documentation
 # and any modifications thereto.  Any use, reproduction, disclosure or
 # distribution of this software and related documentation without an express
 # license agreement from NVIDIA CORPORATION is strictly prohibited.
 **************************************************************************/

#ifndef RTXDI_SPARSE_NEIGHBOR_SEARCH_PARAMETERS_H
#define RTXDI_SPARSE_NEIGHBOR_SEARCH_PARAMETERS_H

#ifdef __cplusplus
#include <stdint.h>
#endif

struct RTXDI_SparseNeighborSearchParameters
{
  uint32_t temporalCandidateCount;
  uint32_t spatialCandidateCount;
  float temporalSearchRadius;
  float spatialSearchRadius;
};

#endif // RTXDI_SPARSE_NEIGHBOR_SEARCH_PARAMETERS_H
