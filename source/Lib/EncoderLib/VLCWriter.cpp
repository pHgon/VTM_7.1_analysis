/* The copyright in this software is being made available under the BSD
* License, included below. This software may be subject to other third party
* and contributor rights, including patent rights, and no such rights are
* granted under this license.
*
* Copyright (c) 2010-2019, ITU/ISO/IEC
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*  * Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
*    be used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

/** \file     VLCWriter.cpp
 *  \brief    Writer for high level syntax
 */

#include "VLCWriter.h"
#include "SEIwrite.h"

#include "CommonLib/CommonDef.h"
#include "CommonLib/Unit.h"
#include "CommonLib/Picture.h" // th remove this
#include "CommonLib/dtrace_next.h"
#include "EncAdaptiveLoopFilter.h"
#include "CommonLib/AdaptiveLoopFilter.h"

//! \ingroup EncoderLib
//! \{

#if ENABLE_TRACING

void  VLCWriter::xWriteCodeTr (uint32_t value, uint32_t  length, const char *pSymbolName)
{
  xWriteCode (value,length);

  if( g_HLSTraceEnable )
  {
    if( length < 10 )
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d)  : %d\n", pSymbolName, length, value );
    }
    else
    {
      DTRACE( g_trace_ctx, D_HEADER, "%-50s u(%d) : %d\n", pSymbolName, length, value );
    }
  }
}

void  VLCWriter::xWriteUvlcTr (uint32_t value, const char *pSymbolName)
{
  xWriteUvlc (value);
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s ue(v) : %d\n", pSymbolName, value );
  }
}

void  VLCWriter::xWriteSvlcTr (int value, const char *pSymbolName)
{
  xWriteSvlc(value);
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s se(v) : %d\n", pSymbolName, value );
  }
}

void  VLCWriter::xWriteFlagTr(uint32_t value, const char *pSymbolName)
{
  xWriteFlag(value);
  if( g_HLSTraceEnable )
  {
    DTRACE( g_trace_ctx, D_HEADER, "%-50s u(1)  : %d\n", pSymbolName, value );
  }
}

bool g_HLSTraceEnable = true;

#endif


void VLCWriter::xWriteCode     ( uint32_t uiCode, uint32_t uiLength )
{
  CHECK( uiLength == 0, "Code of length '0' not supported" );
  m_pcBitIf->write( uiCode, uiLength );
}

void VLCWriter::xWriteUvlc     ( uint32_t uiCode )
{
  uint32_t uiLength = 1;
  uint32_t uiTemp = ++uiCode;

  CHECK( !uiTemp, "Integer overflow" );

  while( 1 != uiTemp )
  {
    uiTemp >>= 1;
    uiLength += 2;
  }
  // Take care of cases where uiLength > 32
  m_pcBitIf->write( 0, uiLength >> 1);
  m_pcBitIf->write( uiCode, (uiLength+1) >> 1);
}

void VLCWriter::xWriteSvlc     ( int iCode )
{
  uint32_t uiCode = uint32_t( iCode <= 0 ? (-iCode)<<1 : (iCode<<1)-1);
  xWriteUvlc( uiCode );
}

void VLCWriter::xWriteFlag( uint32_t uiCode )
{
  m_pcBitIf->write( uiCode, 1 );
}

void VLCWriter::xWriteRbspTrailingBits()
{
  WRITE_FLAG( 1, "rbsp_stop_one_bit");
  int cnt = 0;
  while (m_pcBitIf->getNumBitsUntilByteAligned())
  {
    WRITE_FLAG( 0, "rbsp_alignment_zero_bit");
    cnt++;
  }
  CHECK(cnt>=8, "More than '8' alignment bytes read");
}

void AUDWriter::codeAUD(OutputBitstream& bs, const int pictureType)
{
#if ENABLE_TRACING
  xTraceAccessUnitDelimiter();
#endif

  CHECK(pictureType >= 3, "Invalid picture type");
  setBitstream(&bs);
  WRITE_CODE(pictureType, 3, "pic_type");
  xWriteRbspTrailingBits();
}

void HLSWriter::xCodeRefPicList( const ReferencePictureList* rpl, bool isLongTermPresent, uint32_t ltLsbBitsCount, const bool isForbiddenZeroDeltaPoc )
{
  WRITE_UVLC(rpl->getNumberOfShorttermPictures() + rpl->getNumberOfLongtermPictures(), "num_ref_entries[ listIdx ][ rplsIdx ]");
  uint32_t numRefPic = rpl->getNumberOfShorttermPictures() + rpl->getNumberOfLongtermPictures();
  if (isLongTermPresent)
  {
    WRITE_FLAG(rpl->getLtrpInSliceHeaderFlag(), "ltrp_in_slice_header_flag[ listIdx ][ rplsIdx ]");
  }
  int prevDelta = MAX_INT;
  int deltaValue = 0;
  bool firstSTRP = true;
  for (int ii = 0; ii < numRefPic; ii++)
  {
    if (isLongTermPresent)
      WRITE_FLAG(!rpl->isRefPicLongterm(ii), "st_ref_pic_flag[ listIdx ][ rplsIdx ][ i ]");
    if (!rpl->isRefPicLongterm(ii))
    {
      if (firstSTRP)
      {
        firstSTRP = false;
        deltaValue = prevDelta = rpl->getRefPicIdentifier(ii);
      }
      else
      {
        deltaValue = rpl->getRefPicIdentifier(ii) - prevDelta;
        prevDelta = rpl->getRefPicIdentifier(ii);
      }
      unsigned int absDeltaValue = (deltaValue < 0) ? 0 - deltaValue : deltaValue;
      if( isForbiddenZeroDeltaPoc )
      {
        CHECK( !absDeltaValue, "Zero delta POC is not used without WP" );
        WRITE_UVLC( absDeltaValue - 1, "abs_delta_poc_st[ listIdx ][ rplsIdx ][ i ]" );
      }
      else
      WRITE_UVLC(absDeltaValue, "abs_delta_poc_st[ listIdx ][ rplsIdx ][ i ]");
      if (absDeltaValue > 0)
        WRITE_FLAG((deltaValue < 0) ? 0 : 1, "strp_entry_sign_flag[ listIdx ][ rplsIdx ][ i ]");  //0  means negative delta POC : 1 means positive
    }
    else if (!rpl->getLtrpInSliceHeaderFlag())
    {
      WRITE_CODE(rpl->getRefPicIdentifier(ii), ltLsbBitsCount, "poc_lsb_lt[listIdx][rplsIdx][i]");
    }
  }
}

void HLSWriter::codePPS( const PPS* pcPPS, const SPS* pcSPS )
{
#if ENABLE_TRACING
  xTracePPSHeader ();
#endif

  WRITE_UVLC( pcPPS->getPPSId(),                             "pps_pic_parameter_set_id" );
#if JVET_P0244_SPS_CLEAN_UP
  WRITE_CODE( pcPPS->getSPSId(), 4,                          "pps_seq_parameter_set_id" );
#else
  WRITE_UVLC( pcPPS->getSPSId(),                             "pps_seq_parameter_set_id" );
#endif

  WRITE_UVLC( pcPPS->getPicWidthInLumaSamples(), "pic_width_in_luma_samples" );
  WRITE_UVLC( pcPPS->getPicHeightInLumaSamples(), "pic_height_in_luma_samples" );
  Window conf = pcPPS->getConformanceWindow();

  WRITE_FLAG( conf.getWindowEnabledFlag(), "conformance_window_flag" );
  if( conf.getWindowEnabledFlag() )
  {
    WRITE_UVLC( conf.getWindowLeftOffset(),   "conf_win_left_offset" );
    WRITE_UVLC( conf.getWindowRightOffset(),  "conf_win_right_offset" );
    WRITE_UVLC( conf.getWindowTopOffset(),    "conf_win_top_offset" );
    WRITE_UVLC( conf.getWindowBottomOffset(), "conf_win_bottom_offset" );
  }

  WRITE_FLAG( pcPPS->getOutputFlagPresentFlag() ? 1 : 0,     "output_flag_present_flag" );
#if JVET_P1006_PICTURE_HEADER 
  WRITE_FLAG(pcPPS->getSubPicIdSignallingPresentFlag(), "pps_subpic_id_signalling_present_flag");
  if( pcPPS->getSubPicIdSignallingPresentFlag() )
  {
    WRITE_UVLC( pcPPS->getNumSubPics() - 1, "pps_num_subpics_minus1" );

    WRITE_UVLC( pcPPS->getSubPicIdLen() - 1, "pps_subpic_id_len_minus1" );

    for( int picIdx = 0; picIdx < pcPPS->getNumSubPics( ); picIdx++ )
    {
      WRITE_CODE( pcPPS->getSubPicId(picIdx), pcPPS->getSubPicIdLen( ), "pps_subpic_id[i]" );
    }
  }
#endif
  WRITE_FLAG( pcPPS->getCabacInitPresentFlag() ? 1 : 0,   "cabac_init_present_flag" );
  WRITE_UVLC( pcPPS->getNumRefIdxL0DefaultActive()-1,     "num_ref_idx_l0_default_active_minus1");
  WRITE_UVLC( pcPPS->getNumRefIdxL1DefaultActive()-1,     "num_ref_idx_l1_default_active_minus1");
  WRITE_FLAG(pcPPS->getRpl1IdxPresentFlag() ? 1 : 0, "rpl1IdxPresentFlag");

  WRITE_FLAG( pcPPS->getConstantSliceHeaderParamsEnabledFlag(),              "constant_slice_header_params_enabled_flag");
  if ( pcPPS->getConstantSliceHeaderParamsEnabledFlag() ) {
    WRITE_CODE( pcPPS->getPPSDepQuantEnabledIdc(), 2,                        "pps_dep_quant_enabled_idc");
    WRITE_CODE( pcPPS->getPPSRefPicListSPSIdc0(), 2,                         "pps_ref_pic_list_sps_idc[0]");
    WRITE_CODE( pcPPS->getPPSRefPicListSPSIdc1(), 2,                         "pps_ref_pic_list_sps_idc[1]");
#if !JVET_P0206_TMVP_flags
    WRITE_CODE( pcPPS->getPPSTemporalMVPEnabledIdc(), 2,                     "pps_temporal_mvp_enabled_idc");
#endif
    WRITE_CODE( pcPPS->getPPSMvdL1ZeroIdc(), 2,                              "pps_mvd_l1_zero_idc");
    WRITE_CODE( pcPPS->getPPSCollocatedFromL0Idc(), 2,                       "pps_collocated_from_l0_idc");
    WRITE_UVLC( pcPPS->getPPSSixMinusMaxNumMergeCandPlus1(),                 "pps_six_minus_max_num_merge_cand_plus1");
#if !JVET_P0152_REMOVE_PPS_NUM_SUBBLOCK_MERGE_CAND
    WRITE_UVLC( pcPPS->getPPSFiveMinusMaxNumSubblockMergeCandPlus1(),        "pps_five_minus_max_num_subblock_merge_cand_plus1");
#endif
    WRITE_UVLC( pcPPS->getPPSMaxNumMergeCandMinusMaxNumTriangleCandPlus1(),  "pps_max_num_merge_cand_minus_max_num_triangle_cand_plus1");
  }

  WRITE_SVLC( pcPPS->getPicInitQPMinus26(),                  "init_qp_minus26");
  if (pcSPS->getTransformSkipEnabledFlag())
  {
    WRITE_UVLC(pcPPS->getLog2MaxTransformSkipBlockSize() - 2, "log2_max_transform_skip_block_size_minus2");
  }
  WRITE_FLAG( pcPPS->getUseDQP() ? 1 : 0, "cu_qp_delta_enabled_flag" );
#if !JVET_P1006_PICTURE_HEADER 
  if ( pcPPS->getUseDQP() )
  {
    WRITE_UVLC( pcPPS->getCuQpDeltaSubdiv(), "cu_qp_delta_subdiv" );
  }
#endif

  WRITE_SVLC( pcPPS->getQpOffset(COMPONENT_Cb), "pps_cb_qp_offset" );
  WRITE_SVLC( pcPPS->getQpOffset(COMPONENT_Cr), "pps_cr_qp_offset" );
#if JVET_P0667_QP_OFFSET_TABLE_SIGNALING_JCCR
  if (pcSPS->getJointCbCrEnabledFlag() == false || pcSPS->getChromaFormatIdc() == CHROMA_400)
  {
    CHECK(pcPPS->getJointCbCrQpOffsetPresentFlag(), "pps_jcbcr_qp_offset_present_flag should be false");
  }
  WRITE_FLAG(pcPPS->getJointCbCrQpOffsetPresentFlag() ? 1 : 0, "pps_joint_cbcr_qp_offset_present_flag");
  if (pcPPS->getJointCbCrQpOffsetPresentFlag())
  {
    WRITE_SVLC(pcPPS->getQpOffset(JOINT_CbCr), "pps_joint_cbcr_qp_offset");
  }
#else
  WRITE_SVLC( pcPPS->getQpOffset(JOINT_CbCr),   "pps_joint_cbcr_qp_offset" );
#endif

  WRITE_FLAG( pcPPS->getSliceChromaQpFlag() ? 1 : 0,          "pps_slice_chroma_qp_offsets_present_flag" );

  WRITE_FLAG(uint32_t(pcPPS->getCuChromaQpOffsetEnabledFlag()),         "cu_chroma_qp_offset_enabled_flag" );
  if (pcPPS->getCuChromaQpOffsetEnabledFlag())
  {
#if !JVET_P1006_PICTURE_HEADER 
    WRITE_UVLC(pcPPS->getCuChromaQpOffsetSubdiv(),                      "cu_chroma_qp_offset_subdiv");
#endif
    WRITE_UVLC(pcPPS->getChromaQpOffsetListLen() - 1,                   "chroma_qp_offset_list_len_minus1");
    /* skip zero index */
    for (int cuChromaQpOffsetIdx = 0; cuChromaQpOffsetIdx < pcPPS->getChromaQpOffsetListLen(); cuChromaQpOffsetIdx++)
    {
      WRITE_SVLC(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx+1).u.comp.CbOffset,     "cb_qp_offset_list[i]");
      WRITE_SVLC(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx+1).u.comp.CrOffset,     "cr_qp_offset_list[i]");
#if JVET_P0667_QP_OFFSET_TABLE_SIGNALING_JCCR
      if (pcPPS->getJointCbCrQpOffsetPresentFlag())
      {
        WRITE_SVLC(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1).u.comp.JointCbCrOffset, "joint_cbcr_qp_offset_list[i]");
      }
#else
      WRITE_SVLC(pcPPS->getChromaQpOffsetListEntry(cuChromaQpOffsetIdx + 1).u.comp.JointCbCrOffset, "joint_cbcr_qp_offset_list[i]");
#endif
    }
  }

  WRITE_FLAG( pcPPS->getUseWP() ? 1 : 0,  "weighted_pred_flag" );   // Use of Weighting Prediction (P_SLICE)
  WRITE_FLAG( pcPPS->getWPBiPred() ? 1 : 0, "weighted_bipred_flag" );  // Use of Weighting Bi-Prediction (B_SLICE)
  WRITE_FLAG( pcPPS->getTransquantBypassEnabledFlag()  ? 1 : 0, "transquant_bypass_enabled_flag" );

  WRITE_FLAG( pcPPS->getSingleTileInPicFlag() ? 1 : 0, "single_tile_in_pic_flag" );
  if (!pcPPS->getSingleTileInPicFlag())
  {
    WRITE_FLAG( pcPPS->getUniformTileSpacingFlag() ? 1 : 0, "uniform_tile_spacing_flag" );
    if (pcPPS->getUniformTileSpacingFlag())
    {
      WRITE_UVLC( pcPPS->getTileColsWidthMinus1(),   "tile_cols_width_minus1" );
      WRITE_UVLC( pcPPS->getTileRowsHeightMinus1(),  "tile_rows_height_minus1" );
    }
    else
    {
      WRITE_UVLC( pcPPS->getNumTileColumnsMinus1(), "num_tile_columns_minus1" );
      WRITE_UVLC( pcPPS->getNumTileRowsMinus1(),    "num_tile_rows_minus1" );

      CHECK( ((pcPPS->getNumTileColumnsMinus1() + 1) * (pcPPS->getNumTileRowsMinus1() + 1)) < 2, "tile colums * rows must be > 1 when explicitly signalled.");

      for (int i = 0; i < pcPPS->getNumTileColumnsMinus1(); i++)
      {
        WRITE_UVLC( pcPPS->getTileColumnWidth(i) - 1, "tile_column_width_minus1" );
      }
      for (int i = 0; i < pcPPS->getNumTileRowsMinus1(); i++)
      {
        WRITE_UVLC( pcPPS->getTileRowHeight(i) - 1, "tile_row_height_minus1" );
      }
    }
    WRITE_FLAG( pcPPS->getBrickSplittingPresentFlag() ? 1 : 0, "brick_splitting_present_flag" );
    if (pcPPS->getBrickSplittingPresentFlag())
    {
      CHECK(pcPPS->getRectSliceFlag() != true, "rect_slice_flag must be equal to 1 for brick_splitting_present_flag equal to 1");
    }

    int numTilesInPic = (pcPPS->getNumTileColumnsMinus1() + 1) * (pcPPS->getNumTileRowsMinus1() + 1);
    if (pcPPS->getUniformTileSpacingFlag() && pcPPS->getBrickSplittingPresentFlag())
    {
      WRITE_UVLC(numTilesInPic - 1, "num_tiles_in_pic_minus1");
    }

    for( int i = 0; pcPPS->getBrickSplittingPresentFlag()  &&  i < numTilesInPic; i++ )
    {
      if (pcPPS->getTileHeight(i) > 1)
      {
        WRITE_FLAG(pcPPS->getBrickSplitFlag(i) ? 1 : 0, "brick_split_flag [i]");
      }
      if( pcPPS->getBrickSplitFlag(i) )
      {
        if (pcPPS->getTileHeight(i) > 2)
        {
          WRITE_FLAG(pcPPS->getUniformBrickSpacingFlag(i) ? 1 : 0, "uniform_brick_spacing_flag [i]");
        }
        if( pcPPS->getUniformBrickSpacingFlag(i) )
          WRITE_UVLC( pcPPS->getBrickHeightMinus1(i), "brick_height_minus1" );
        else
        {
          WRITE_UVLC(pcPPS->getNumBrickRowsMinus2(i), "num_brick_rows_minus2 [i]");
          for (int j = 0; j <= pcPPS->getNumBrickRowsMinus2(i); j++)
            WRITE_UVLC(pcPPS->getBrickRowHeightMinus1(i, j), "brick_row_height_minus1 [i][j]");
        }
      }
    }

    WRITE_FLAG( pcPPS->getSingleBrickPerSliceFlag() ? 1 : 0, "single_brick_per_slice_flag" );
    if (!pcPPS->getSingleBrickPerSliceFlag())
    {
      WRITE_FLAG( pcPPS->getRectSliceFlag() ? 1 : 0, "rect_slice_flag" );
    }
    else
    {
      // make sure rect_slice_flag is set
      CHECK (pcPPS->getRectSliceFlag()!=true, "RectSliceFlag must be equal to 1 for single_brick_per_slice_flag equal to 1");
    }

    if (pcPPS->getRectSliceFlag() && !pcPPS->getSingleBrickPerSliceFlag())
    {
      WRITE_UVLC( pcPPS->getNumSlicesInPicMinus1(), "num_slices_in_pic_minus1" );
      int numSlicesInPic = pcPPS->getNumSlicesInPicMinus1() + 1;
      int codeLength = ceilLog2(numTilesInPic);
      WRITE_UVLC(codeLength, "bottom_right_brick_idx_length_minus1 ");
      for (int i = 0; i < numSlicesInPic; ++i)
      {
        int delta = (i == 0) ? pcPPS->getBottomRightBrickIdx(i) : pcPPS->getBottomRightBrickIdx(i) - pcPPS->getBottomRightBrickIdx(i - 1);
        int sign = (delta > 0) ? 1 : 0;
        WRITE_CODE(delta, codeLength, "bottom_right_brick_idx_delta");
        WRITE_FLAG(sign, "brick_idx_delta_sign_flag");
      }
    }

    WRITE_FLAG( pcPPS->getLoopFilterAcrossBricksEnabledFlag() ? 1 : 0, "loop_filter_across_bricks_enabled_flag" );
    if (pcPPS->getLoopFilterAcrossBricksEnabledFlag())
    {
      WRITE_FLAG( pcPPS->getLoopFilterAcrossSlicesEnabledFlag() ? 1 : 0, "loop_filter_across_slices_enabled_flag" );
    }
  }
  else
  {
    // make sure single brick per slice is set by encoder such that the behaviour is same as for setting it to true
    CHECK(pcPPS->getSingleBrickPerSliceFlag() != true, "SingleBrickPerSliceFlag must be set to 1 when not present");
    // make sure rect_slice_flag is set
    CHECK (pcPPS->getRectSliceFlag()!=true, "RectSliceFlag must be equalt to 1 for single_tile_in_pic_flag equal to 1");
  }

  if (pcPPS->getRectSliceFlag())
  {
    WRITE_FLAG( pcPPS->getSignalledSliceIdFlag() ? 1 : 0, "signalled_slice_id_flag" );
    if (pcPPS->getSignalledSliceIdFlag())
    {
      WRITE_UVLC( pcPPS->getSignalledSliceIdLengthMinus1(), "signalled_slice_id_length_minus1" );
      int signalledTileGroupIdLength = pcPPS->getSignalledSliceIdLengthMinus1() + 1;
      int numTileGroupsInPic = pcPPS->getNumSlicesInPicMinus1() + 1;
      for (int i = 0; i < numTileGroupsInPic; ++i)
      {
        WRITE_CODE (pcPPS->getSliceId(i), signalledTileGroupIdLength, "slice_id" );
      }
    }
  }


  WRITE_FLAG( pcPPS->getEntropyCodingSyncEnabledFlag() ? 1 : 0, "entropy_coding_sync_enabled_flag" );

  WRITE_FLAG( pcPPS->getDeblockingFilterControlPresentFlag()?1 : 0,       "deblocking_filter_control_present_flag");
  if(pcPPS->getDeblockingFilterControlPresentFlag())
  {
    WRITE_FLAG( pcPPS->getDeblockingFilterOverrideEnabledFlag() ? 1 : 0,  "deblocking_filter_override_enabled_flag" );
    WRITE_FLAG( pcPPS->getPPSDeblockingFilterDisabledFlag() ? 1 : 0,      "pps_deblocking_filter_disabled_flag" );
    if(!pcPPS->getPPSDeblockingFilterDisabledFlag())
    {
      WRITE_SVLC( pcPPS->getDeblockingFilterBetaOffsetDiv2(),             "pps_beta_offset_div2" );
      WRITE_SVLC( pcPPS->getDeblockingFilterTcOffsetDiv2(),               "pps_tc_offset_div2" );
    }
  }

#if !JVET_P1006_PICTURE_HEADER 
  WRITE_FLAG( pcPPS->getLoopFilterAcrossVirtualBoundariesDisabledFlag() ? 1 : 0,     "pps_loop_filter_across_virtual_boundaries_disabled_flag" );
  if( pcPPS->getLoopFilterAcrossVirtualBoundariesDisabledFlag() )
  {
    WRITE_CODE( pcPPS->getNumVerVirtualBoundaries(), 2,                              "pps_num_ver_virtual_boundaries");
    for( unsigned i = 0; i < pcPPS->getNumVerVirtualBoundaries(); i++ )
    {
      WRITE_CODE(pcPPS->getVirtualBoundariesPosX(i) >> 3, 13,                        "pps_virtual_boundaries_pos_x");
    }
    WRITE_CODE( pcPPS->getNumHorVirtualBoundaries(), 2,                              "pps_num_hor_virtual_boundaries");
    for( unsigned i = 0; i < pcPPS->getNumHorVirtualBoundaries(); i++ )
    {
      WRITE_CODE(pcPPS->getVirtualBoundariesPosY(i) >> 3, 13,                        "pps_virtual_boundaries_pos_y");
    }
  }
#endif

#if JVET_P1006_PICTURE_HEADER 
  WRITE_FLAG( pcPPS->getPictureHeaderExtensionPresentFlag() ? 1 : 0, "picture_header_extension_present_flag");
#endif
  WRITE_FLAG( pcPPS->getSliceHeaderExtensionPresentFlag() ? 1 : 0, "slice_segment_header_extension_present_flag");

  bool pps_extension_present_flag=false;
  bool pps_extension_flags[NUM_PPS_EXTENSION_FLAGS]={false};

  WRITE_FLAG( (pps_extension_present_flag?1:0), "pps_extension_present_flag" );

  if (pps_extension_present_flag)
  {
#if ENABLE_TRACING /*|| RExt__DECODER_DEBUG_BIT_STATISTICS*/
    static const char *syntaxStrings[]={ "pps_range_extension_flag",
      "pps_multilayer_extension_flag",
      "pps_extension_6bits[0]",
      "pps_extension_6bits[1]",
      "pps_extension_6bits[2]",
      "pps_extension_6bits[3]",
      "pps_extension_6bits[4]",
      "pps_extension_6bits[5]" };
#endif

    for(int i=0; i<NUM_PPS_EXTENSION_FLAGS; i++)
    {
      WRITE_FLAG( pps_extension_flags[i]?1:0, syntaxStrings[i] );
    }

    for(int i=0; i<NUM_PPS_EXTENSION_FLAGS; i++) // loop used so that the order is determined by the enum.
    {
      if (pps_extension_flags[i])
      {
        switch (PPSExtensionFlagIndex(i))
        {
        case PPS_EXT__REXT:
        {
          const PPSRExt &ppsRangeExtension = pcPPS->getPpsRangeExtension();

          WRITE_FLAG((ppsRangeExtension.getCrossComponentPredictionEnabledFlag() ? 1 : 0), "cross_component_prediction_enabled_flag" );

          WRITE_UVLC( ppsRangeExtension.getLog2SaoOffsetScale(CHANNEL_TYPE_LUMA),           "log2_sao_offset_scale_luma"   );
          WRITE_UVLC( ppsRangeExtension.getLog2SaoOffsetScale(CHANNEL_TYPE_CHROMA),         "log2_sao_offset_scale_chroma" );
        }
        break;
        default:
          CHECK(pps_extension_flags[i]==false, "Unknown PPS extension signalled"); // Should never get here with an active PPS extension flag.
          break;
        } // switch
      } // if flag present
    } // loop over PPS flags
  } // pps_extension_present_flag is non-zero
  xWriteRbspTrailingBits();
}

void HLSWriter::codeAPS( APS* pcAPS )
{
#if ENABLE_TRACING
  xTraceAPSHeader();
#endif

  WRITE_CODE(pcAPS->getAPSId(), 5, "adaptation_parameter_set_id");
  WRITE_CODE( (int)pcAPS->getAPSType(), 3, "aps_params_type" );

  if (pcAPS->getAPSType() == ALF_APS)
  {
    codeAlfAps(pcAPS);
  }
  else if (pcAPS->getAPSType() == LMCS_APS)
  {
    codeLmcsAps (pcAPS);
  }
  else if( pcAPS->getAPSType() == SCALING_LIST_APS )
  {
    codeScalingListAps( pcAPS );
  }
  WRITE_FLAG(0, "aps_extension_flag");   //Implementation when this flag is equal to 1 should be added when it is needed. Currently in the spec we don't have case when this flag is equal to 1
  xWriteRbspTrailingBits();
}

void HLSWriter::codeAlfAps( APS* pcAPS )
{
  AlfParam param = pcAPS->getAlfAPSParam();

  WRITE_FLAG(param.newFilterFlag[CHANNEL_TYPE_LUMA], "alf_luma_new_filter");
  WRITE_FLAG(param.newFilterFlag[CHANNEL_TYPE_CHROMA], "alf_chroma_new_filter");

  if (param.newFilterFlag[CHANNEL_TYPE_LUMA])
  {
    WRITE_FLAG( param.nonLinearFlag[CHANNEL_TYPE_LUMA][0], "alf_luma_clip" );

    WRITE_UVLC(param.numLumaFilters - 1, "alf_luma_num_filters_signalled_minus1");
    if (param.numLumaFilters > 1)
    {
      const int length =  ceilLog2( param.numLumaFilters);
      for (int i = 0; i < MAX_NUM_ALF_CLASSES; i++)
      {
        WRITE_CODE(param.filterCoeffDeltaIdx[i], length, "alf_luma_coeff_delta_idx" );
      }
    }
    alfFilter(param, false, 0);

  }
  if (param.newFilterFlag[CHANNEL_TYPE_CHROMA])
  {
    if( MAX_NUM_ALF_ALTERNATIVES_CHROMA > 1 )
      WRITE_UVLC( param.numAlternativesChroma - 1, "alf_chroma_num_alts_minus1" );
    for( int altIdx=0; altIdx < param.numAlternativesChroma; ++altIdx )
    {
      WRITE_FLAG( param.nonLinearFlag[CHANNEL_TYPE_CHROMA][altIdx], "alf_nonlinear_enable_flag_chroma" );
      alfFilter(param, true, altIdx);
    }
  }
}

void HLSWriter::codeLmcsAps( APS* pcAPS )
{
  SliceReshapeInfo param = pcAPS->getReshaperAPSInfo();
  WRITE_UVLC(param.reshaperModelMinBinIdx, "lmcs_min_bin_idx");
  WRITE_UVLC(PIC_CODE_CW_BINS - 1 - param.reshaperModelMaxBinIdx, "lmcs_delta_max_bin_idx");
  assert(param.maxNbitsNeededDeltaCW > 0);
  WRITE_UVLC(param.maxNbitsNeededDeltaCW - 1, "lmcs_delta_cw_prec_minus1");

  for (int i = param.reshaperModelMinBinIdx; i <= param.reshaperModelMaxBinIdx; i++)
  {
    int deltaCW = param.reshaperModelBinCWDelta[i];
    int signCW = (deltaCW < 0) ? 1 : 0;
    int absCW = (deltaCW < 0) ? (-deltaCW) : deltaCW;
    WRITE_CODE(absCW, param.maxNbitsNeededDeltaCW, "lmcs_delta_abs_cw[ i ]");
    if (absCW > 0)
    {
      WRITE_FLAG(signCW, "lmcs_delta_sign_cw_flag[ i ]");
    }
  }
#if JVET_P0371_CHROMA_SCALING_OFFSET
  int deltaCRS = param.chrResScalingOffset;
  int signCRS = (deltaCRS < 0) ? 1 : 0;
  int absCRS = (deltaCRS < 0) ? (-deltaCRS) : deltaCRS;
  WRITE_CODE(absCRS, 3, "lmcs_delta_crs_val");
  if (absCRS > 0)
  {
    WRITE_FLAG(signCRS, "lmcs_delta_crs_val_flag");
  }
#endif
}

void HLSWriter::codeScalingListAps( APS* pcAPS )
{
  ScalingList param = pcAPS->getScalingList();
  codeScalingList( param );
}

void HLSWriter::codeVUI( const VUI *pcVUI, const SPS* pcSPS )
{
#if ENABLE_TRACING
  DTRACE( g_trace_ctx, D_HEADER, "----------- vui_parameters -----------\n");
#endif


  WRITE_FLAG(pcVUI->getAspectRatioInfoPresentFlag(),            "aspect_ratio_info_present_flag");
  if (pcVUI->getAspectRatioInfoPresentFlag())
  {
    WRITE_CODE(pcVUI->getAspectRatioIdc(), 8,                   "aspect_ratio_idc" );
    if (pcVUI->getAspectRatioIdc() == 255)
    {
      WRITE_CODE(pcVUI->getSarWidth(), 16,                      "sar_width");
      WRITE_CODE(pcVUI->getSarHeight(), 16,                     "sar_height");
    }
  }
  WRITE_FLAG(pcVUI->getColourDescriptionPresentFlag(),        "colour_description_present_flag");
  if (pcVUI->getColourDescriptionPresentFlag())
  {
    WRITE_CODE(pcVUI->getColourPrimaries(), 8,                "colour_primaries");
    WRITE_CODE(pcVUI->getTransferCharacteristics(), 8,        "transfer_characteristics");
    WRITE_CODE(pcVUI->getMatrixCoefficients(), 8,             "matrix_coeffs");
  }
  WRITE_FLAG(pcVUI->getFieldSeqFlag(),                          "field_seq_flag");
  WRITE_FLAG(pcVUI->getChromaLocInfoPresentFlag(),              "chroma_loc_info_present_flag");
  if (pcVUI->getChromaLocInfoPresentFlag())
  {
    if(pcVUI->getFieldSeqFlag())
    {
      WRITE_UVLC(pcVUI->getChromaSampleLocTypeTopField(),         "chroma_sample_loc_type_top_field");
      WRITE_UVLC(pcVUI->getChromaSampleLocTypeBottomField(),      "chroma_sample_loc_type_bottom_field");
    }
    else
    {
      WRITE_UVLC(pcVUI->getChromaSampleLocType(),         "chroma_sample_loc_type");
    }
  }
  WRITE_FLAG(pcVUI->getOverscanInfoPresentFlag(),               "overscan_info_present_flag");
  if (pcVUI->getOverscanInfoPresentFlag())
  {
    WRITE_FLAG(pcVUI->getOverscanAppropriateFlag(),             "overscan_appropriate_flag");
  }
  WRITE_FLAG(pcVUI->getVideoSignalTypePresentFlag(),            "video_signal_type_present_flag");
  if (pcVUI->getVideoSignalTypePresentFlag())
  {
    WRITE_FLAG(pcVUI->getVideoFullRangeFlag(),                  "video_full_range_flag");
  }

}

void HLSWriter::codeHrdParameters( const HRDParameters *hrd, const uint32_t firstSubLayer, const uint32_t maxNumSubLayersMinus1)
{
  WRITE_FLAG( hrd->getNalHrdParametersPresentFlag() ? 1 : 0 ,  "general_nal_hrd_parameters_present_flag" );
  WRITE_FLAG( hrd->getVclHrdParametersPresentFlag() ? 1 : 0 ,  "general_vcl_hrd_parameters_present_flag" );
#if JVET_P0202_P0203_FIX_HRD_RELATED_SEI 
  WRITE_FLAG( hrd->getGeneralDecodingUnitHrdParamsPresentFlag() ? 1 : 0,  "general_decoding_unit_hrd_params_present_flag" );
  if( hrd->getGeneralDecodingUnitHrdParamsPresentFlag() )
  {
    WRITE_CODE( hrd->getTickDivisorMinus2(), 8,            "tick_divisor_minus2" );
  }
  WRITE_CODE( hrd->getBitRateScale(), 4,                     "bit_rate_scale" );
  WRITE_CODE( hrd->getCpbSizeScale(), 4,                     "cpb_size_scale" );
  if( hrd->getGeneralDecodingUnitHrdParamsPresentFlag() )
  {
    WRITE_CODE( hrd->getCpbSizeDuScale(), 4,               "cpb_size_du_scale" );
  }
#else
  if( hrd->getNalHrdParametersPresentFlag() || hrd->getVclHrdParametersPresentFlag() )
  {
    WRITE_FLAG( hrd->getDecodingUnitHrdParamsPresentFlag() ? 1 : 0,  "decoding_unit_hrd_params_present_flag" );
    if( hrd->getDecodingUnitHrdParamsPresentFlag() )
    {
      WRITE_CODE( hrd->getTickDivisorMinus2(), 8,            "tick_divisor_minus2" );
      WRITE_FLAG( hrd->getDecodingUnitCpbParamsInPicTimingSeiFlag() ? 1 : 0, "decoding_unit_cpb_params_in_pic_timing_sei_flag" );
    }
    WRITE_CODE( hrd->getBitRateScale(), 4,                     "bit_rate_scale" );
    WRITE_CODE( hrd->getCpbSizeScale(), 4,                     "cpb_size_scale" );
    if( hrd->getDecodingUnitHrdParamsPresentFlag() )
    {
      WRITE_CODE( hrd->getCpbSizeDuScale(), 4,               "cpb_size_du_scale" );
    }
  }
#endif

  for( int i = firstSubLayer; i <= maxNumSubLayersMinus1; i ++ )
  {
    WRITE_FLAG( hrd->getFixedPicRateFlag( i ) ? 1 : 0,          "fixed_pic_rate_general_flag");
    bool fixedPixRateWithinCvsFlag = true;
    if( !hrd->getFixedPicRateFlag( i ) )
    {
      fixedPixRateWithinCvsFlag = hrd->getFixedPicRateWithinCvsFlag( i );
      WRITE_FLAG( hrd->getFixedPicRateWithinCvsFlag( i ) ? 1 : 0, "fixed_pic_rate_within_cvs_flag");
    }
    if( fixedPixRateWithinCvsFlag )
    {
      WRITE_UVLC( hrd->getPicDurationInTcMinus1( i ),           "elemental_duration_in_tc_minus1");
    }
    else
    {
      WRITE_FLAG( hrd->getLowDelayHrdFlag( i ) ? 1 : 0,           "low_delay_hrd_flag");
    }
    if (!hrd->getLowDelayHrdFlag( i ))
    {
      WRITE_UVLC( hrd->getCpbCntMinus1( i ),                      "cpb_cnt_minus1");
    }

    for( int nalOrVcl = 0; nalOrVcl < 2; nalOrVcl ++ )
    {
      if( ( ( nalOrVcl == 0 ) && ( hrd->getNalHrdParametersPresentFlag() ) ) ||
          ( ( nalOrVcl == 1 ) && ( hrd->getVclHrdParametersPresentFlag() ) ) )
      {
        for( int j = 0; j <= ( hrd->getCpbCntMinus1( i ) ); j ++ )
        {
          WRITE_UVLC( hrd->getBitRateValueMinus1( i, j, nalOrVcl ), "bit_rate_value_minus1");
          WRITE_UVLC( hrd->getCpbSizeValueMinus1( i, j, nalOrVcl ), "cpb_size_value_minus1");
          WRITE_FLAG( hrd->getCbrFlag( i, j, nalOrVcl ) ? 1 : 0, "cbr_flag");
        }
      }
    }
  }
}


void HLSWriter::codeSPS( const SPS* pcSPS )
{
#if ENABLE_TRACING
  xTraceSPSHeader ();
#endif
  WRITE_CODE( pcSPS->getDecodingParameterSetId (), 4,       "sps_decoding_parameter_set_id" );
#if JVET_P0205_VPS_ID_0
  WRITE_CODE( pcSPS->getVPSId(), 4, "sps_video_parameter_set_id" );
#endif
  CHECK(pcSPS->getMaxTLayers() == 0, "Maximum number of temporal sub-layers is '0'");

  WRITE_CODE(pcSPS->getMaxTLayers() - 1, 3, "sps_max_sub_layers_minus1");
  WRITE_CODE(0,                          5, "sps_reserved_zero_5bits");

  codeProfileTierLevel( pcSPS->getProfileTierLevel(), pcSPS->getMaxTLayers() - 1 );
  WRITE_FLAG(pcSPS->getGDREnabledFlag(), "gdr_enabled_flag");

#if JVET_P0244_SPS_CLEAN_UP
  WRITE_CODE( pcSPS->getSPSId (), 4, "sps_seq_parameter_set_id" );
#else
  WRITE_UVLC(pcSPS->getSPSId (), "sps_seq_parameter_set_id");
#endif

#if JVET_P0244_SPS_CLEAN_UP
  WRITE_CODE(int(pcSPS->getChromaFormatIdc ()), 2, "chroma_format_idc");
#else
  WRITE_UVLC( int(pcSPS->getChromaFormatIdc ()),    "chroma_format_idc" );
#endif

  const ChromaFormat format                = pcSPS->getChromaFormatIdc();
  if( format == CHROMA_444 )
  {
    WRITE_FLAG( 0,                                  "separate_colour_plane_flag");
  }

  WRITE_UVLC( pcSPS->getMaxPicWidthInLumaSamples(), "pic_width_max_in_luma_samples" );
  WRITE_UVLC( pcSPS->getMaxPicHeightInLumaSamples(), "pic_height_max_in_luma_samples" );

#if JVET_P1006_PICTURE_HEADER
  WRITE_FLAG( pcSPS->getSubPicIdPresentFlag(), "sps_subpic_id_present_flag");
  if( pcSPS->getSubPicIdPresentFlag() )
  {
    WRITE_FLAG(pcSPS->getSubPicIdSignallingPresentFlag(), "sps_subpic_id_signalling_present_flag");
    if( pcSPS->getSubPicIdSignallingPresentFlag() )
    {
      WRITE_UVLC( pcSPS->getSubPicIdLen( ) - 1, "sps_subpic_id_len_minus1" );
      for( int picIdx = 0; picIdx < pcSPS->getNumSubPics( ); picIdx++ )
      {
        WRITE_CODE( pcSPS->getSubPicId(picIdx), pcSPS->getSubPicIdLen( ), "sps_subpic_id[i]" );
      }
    }
  }

#endif
#if JVET_P0243_SINGLE_BIT_DEPTH
  WRITE_UVLC( pcSPS->getBitDepth(CHANNEL_TYPE_LUMA) - 8,                      "bit_depth_minus8" );
#else
  WRITE_UVLC( pcSPS->getBitDepth(CHANNEL_TYPE_LUMA) - 8,                      "bit_depth_luma_minus8" );

  const bool         chromaEnabled         = isChromaEnabled(format);
  WRITE_UVLC( chromaEnabled ? (pcSPS->getBitDepth(CHANNEL_TYPE_CHROMA) - 8):0,  "bit_depth_chroma_minus8" );
#endif

  WRITE_UVLC( pcSPS->getMinQpPrimeTsMinus4(CHANNEL_TYPE_LUMA),                      "min_qp_prime_ts_minus4" );
  
  WRITE_FLAG( pcSPS->getUseWP() ? 1 : 0, "sps_weighted_pred_flag" );   // Use of Weighting Prediction (P_SLICE)
  WRITE_FLAG( pcSPS->getUseWPBiPred() ? 1 : 0, "sps_weighted_bipred_flag" );  // Use of Weighting Bi-Prediction (B_SLICE)

#if JVET_P0244_SPS_CLEAN_UP
  WRITE_CODE(pcSPS->getBitsForPOC()-4, 4, "log2_max_pic_order_cnt_lsb_minus4");
#else
  WRITE_UVLC( pcSPS->getBitsForPOC()-4,                 "log2_max_pic_order_cnt_lsb_minus4" );
#endif
  WRITE_FLAG( pcSPS->getIDRRefParamListPresent(),                 "sps_idr_rpl_present_flag" );
  // KJS: Marakech decision: sub-layers added back
  const bool subLayerOrderingInfoPresentFlag = 1;
  if (pcSPS->getMaxTLayers() > 1)
  {
    WRITE_FLAG(subLayerOrderingInfoPresentFlag,       "sps_sub_layer_ordering_info_present_flag");
  }
  for(uint32_t i=0; i <= pcSPS->getMaxTLayers()-1; i++)
  {
    WRITE_UVLC( pcSPS->getMaxDecPicBuffering(i) - 1,       "sps_max_dec_pic_buffering_minus1[i]" );
    WRITE_UVLC( pcSPS->getNumReorderPics(i),               "sps_max_num_reorder_pics[i]" );
    WRITE_UVLC( pcSPS->getMaxLatencyIncreasePlus1(i),      "sps_max_latency_increase_plus1[i]" );
    if (!subLayerOrderingInfoPresentFlag)
    {
      break;
    }
  }
  CHECK( pcSPS->getMaxCUWidth() != pcSPS->getMaxCUHeight(),                          "Rectangular CTUs not supported" );
  WRITE_FLAG(pcSPS->getLongTermRefsPresent() ? 1 : 0, "long_term_ref_pics_flag");
  WRITE_FLAG(pcSPS->getRPL1CopyFromRPL0Flag() ? 1 : 0, "rpl1_copy_from_rpl0_flag");

  const RPLList* rplList0 = pcSPS->getRPLList0();
  const RPLList* rplList1 = pcSPS->getRPLList1();

  //Write candidate for List0
  uint32_t numberOfRPL = pcSPS->getNumRPL0();
  WRITE_UVLC(numberOfRPL, "num_ref_pic_lists_in_sps[0]");
  for (int ii = 0; ii < numberOfRPL; ii++)
  {
    const ReferencePictureList* rpl = rplList0->getReferencePictureList(ii);
    xCodeRefPicList( rpl, pcSPS->getLongTermRefsPresent(), pcSPS->getBitsForPOC(), !pcSPS->getUseWP() && !pcSPS->getUseWPBiPred() );
  }

  //Write candidate for List1
  if (!pcSPS->getRPL1CopyFromRPL0Flag())
  {
    numberOfRPL = pcSPS->getNumRPL1();
    WRITE_UVLC(numberOfRPL, "num_ref_pic_lists_in_sps[1]");
    for (int ii = 0; ii < numberOfRPL; ii++)
    {
      const ReferencePictureList* rpl = rplList1->getReferencePictureList(ii);
      xCodeRefPicList( rpl, pcSPS->getLongTermRefsPresent(), pcSPS->getBitsForPOC(), !pcSPS->getUseWP() && !pcSPS->getUseWPBiPred() );
    }
  }
  WRITE_FLAG(pcSPS->getUseDualITree(), "qtbtt_dual_tree_intra_flag");
  WRITE_CODE(floorLog2(pcSPS->getCTUSize()) - 5, 2, "log2_ctu_size_minus5");
  WRITE_UVLC(pcSPS->getLog2MinCodingBlockSize() - 2, "log2_min_luma_coding_block_size_minus2");
  WRITE_FLAG(pcSPS->getSplitConsOverrideEnabledFlag(), "partition_constraints_override_enabled_flag");
  WRITE_UVLC(floorLog2(pcSPS->getMinQTSize(I_SLICE)) - pcSPS->getLog2MinCodingBlockSize(), "sps_log2_diff_min_qt_min_cb_intra_slice_luma");
  WRITE_UVLC(floorLog2(pcSPS->getMinQTSize(B_SLICE)) - pcSPS->getLog2MinCodingBlockSize(), "sps_log2_diff_min_qt_min_cb_inter_slice");
  WRITE_UVLC(pcSPS->getMaxMTTHierarchyDepth(), "sps_max_mtt_hierarchy_depth_inter_slice");
  WRITE_UVLC(pcSPS->getMaxMTTHierarchyDepthI(), "sps_max_mtt_hierarchy_depth_intra_slice_luma");
  if (pcSPS->getMaxMTTHierarchyDepthI() != 0)
  {
    WRITE_UVLC(floorLog2(pcSPS->getMaxBTSizeI()) - floorLog2(pcSPS->getMinQTSize(I_SLICE)), "sps_log2_diff_max_bt_min_qt_intra_slice_luma");
    WRITE_UVLC(floorLog2(pcSPS->getMaxTTSizeI()) - floorLog2(pcSPS->getMinQTSize(I_SLICE)), "sps_log2_diff_max_tt_min_qt_intra_slice_luma");
  }
  if (pcSPS->getMaxMTTHierarchyDepth() != 0)
  {
    WRITE_UVLC(floorLog2(pcSPS->getMaxBTSize()) - floorLog2(pcSPS->getMinQTSize(B_SLICE)), "sps_log2_diff_max_bt_min_qt_inter_slice");
    WRITE_UVLC(floorLog2(pcSPS->getMaxTTSize()) - floorLog2(pcSPS->getMinQTSize(B_SLICE)), "sps_log2_diff_max_tt_min_qt_inter_slice");
  }
  if (pcSPS->getUseDualITree())
  {
    WRITE_UVLC(floorLog2(pcSPS->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)) - pcSPS->getLog2MinCodingBlockSize(), "sps_log2_diff_min_qt_min_cb_intra_slice_chroma");
    WRITE_UVLC(pcSPS->getMaxMTTHierarchyDepthIChroma(), "sps_max_mtt_hierarchy_depth_intra_slice_chroma");
    if (pcSPS->getMaxMTTHierarchyDepthIChroma() != 0)
    {
      WRITE_UVLC(floorLog2(pcSPS->getMaxBTSizeIChroma()) - floorLog2(pcSPS->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)), "sps_log2_diff_max_bt_min_qt_intra_slice_chroma");
      WRITE_UVLC(floorLog2(pcSPS->getMaxTTSizeIChroma()) - floorLog2(pcSPS->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)), "sps_log2_diff_max_tt_min_qt_intra_slice_chroma");
    }
  }

  WRITE_FLAG( (pcSPS->getLog2MaxTbSize() - 5) ? 1 : 0,                       "sps_max_luma_transform_size_64_flag" );

#if JVET_P0667_QP_OFFSET_TABLE_SIGNALING_JCCR
  WRITE_FLAG(pcSPS->getJointCbCrEnabledFlag(), "sps_joint_cbcr_enabled_flag");
#endif
  if (pcSPS->getChromaFormatIdc() != CHROMA_400)
  {
    const ChromaQpMappingTable& chromaQpMappingTable = pcSPS->getChromaQpMappingTable();
    WRITE_FLAG(chromaQpMappingTable.getSameCQPTableForAllChromaFlag(), "same_qp_table_for_chroma");
#if JVET_P0667_QP_OFFSET_TABLE_SIGNALING_JCCR
    int numQpTables = chromaQpMappingTable.getSameCQPTableForAllChromaFlag() ? 1 : (pcSPS->getJointCbCrEnabledFlag() ? 3 : 2);
    CHECK(numQpTables != chromaQpMappingTable.getNumQpTables(), " numQpTables does not match at encoder side ");
    for (int i = 0; i < numQpTables; i++)
#else
    for (int i = 0; i < (chromaQpMappingTable.getSameCQPTableForAllChromaFlag() ? 1 : 3); i++)
#endif
    {
#if JVET_P0410_CHROMA_QP_MAPPING
      WRITE_SVLC(chromaQpMappingTable.getQpTableStartMinus26(i), "qp_table_starts_minus26");
#endif
      WRITE_UVLC(chromaQpMappingTable.getNumPtsInCQPTableMinus1(i), "num_points_in_qp_table_minus1");

      for (int j = 0; j <= chromaQpMappingTable.getNumPtsInCQPTableMinus1(i); j++)
      {
        WRITE_UVLC(chromaQpMappingTable.getDeltaQpInValMinus1(i,j),  "delta_qp_in_val_minus1");
#if JVET_P0469_QP_OUT_VAL
        WRITE_UVLC(chromaQpMappingTable.getDeltaQpOutVal(i, j) ^ chromaQpMappingTable.getDeltaQpInValMinus1(i, j),
                   "delta_qp_diff_val");
#else
        WRITE_UVLC(chromaQpMappingTable.getDeltaQpOutVal(i, j),       "delta_qp_out_val");
#endif
      }
    }
  }

  WRITE_FLAG( pcSPS->getSAOEnabledFlag(),                                            "sps_sao_enabled_flag");
  WRITE_FLAG( pcSPS->getALFEnabledFlag(),                                            "sps_alf_enabled_flag" );

  WRITE_FLAG(pcSPS->getTransformSkipEnabledFlag() ? 1 : 0, "sps_transform_skip_enabled_flag");
  if (pcSPS->getTransformSkipEnabledFlag())
  {
#if JVET_P0059_CHROMA_BDPCM
      WRITE_FLAG(pcSPS->getBDPCMEnabled() ? 1 : 0, "sps_bdpcm_enabled_flag");
      if (pcSPS->getBDPCMEnabled() && pcSPS->getChromaFormatIdc() == CHROMA_444)
      {
          WRITE_FLAG(pcSPS->getBDPCMEnabled() == BDPCM_LUMACHROMA ? 1 : 0, "sps_bdpcm_enabled_chroma_flag");
      }
      else 
      {
        CHECK(pcSPS->getBDPCMEnabled() == BDPCM_LUMACHROMA, "BDPCM for chroma can be used for 444 only.")
      }
#else
    WRITE_FLAG(pcSPS->getBDPCMEnabledFlag() ? 1 : 0, "sps_bdpcm_enabled_flag");
#endif
  }
  else
  {
#if JVET_P0059_CHROMA_BDPCM
    CHECK(pcSPS->getBDPCMEnabled()!=0, "BDPCM cannot be used when transform skip is disabled");
#else
    CHECK(pcSPS->getBDPCMEnabledFlag(), "BDPCM cannot be used when transform skip is disabled");
#endif
  }
#if !JVET_P0667_QP_OFFSET_TABLE_SIGNALING_JCCR
  WRITE_FLAG( pcSPS->getJointCbCrEnabledFlag(),                                           "sps_joint_cbcr_enabled_flag");
#endif

  WRITE_FLAG( pcSPS->getWrapAroundEnabledFlag() ? 1 : 0,                              "sps_ref_wraparound_enabled_flag" );
  if( pcSPS->getWrapAroundEnabledFlag() )
  {
    WRITE_UVLC( (pcSPS->getWrapAroundOffset()/(1 <<  pcSPS->getLog2MinCodingBlockSize()))-1,  "sps_ref_wraparound_offset_minus1" );
  }

  WRITE_FLAG( pcSPS->getSPSTemporalMVPEnabledFlag()  ? 1 : 0,                        "sps_temporal_mvp_enabled_flag" );

  if ( pcSPS->getSPSTemporalMVPEnabledFlag() )
  {
    WRITE_FLAG( pcSPS->getSBTMVPEnabledFlag() ? 1 : 0,                               "sps_sbtmvp_enabled_flag");
  }

  WRITE_FLAG( pcSPS->getAMVREnabledFlag() ? 1 : 0,                                   "sps_amvr_enabled_flag" );

  WRITE_FLAG( pcSPS->getBDOFEnabledFlag() ? 1 : 0,                                   "sps_bdof_enabled_flag" );
  WRITE_FLAG( pcSPS->getUseDMVR() ? 1 : 0,                                            "sps_dmvr_enable_flag" );
  WRITE_FLAG(pcSPS->getUseMMVD() ? 1 : 0,                                             "sps_mmvd_enable_flag");
  // KJS: sps_cclm_enabled_flag
  WRITE_FLAG( pcSPS->getUseLMChroma() ? 1 : 0,                                                 "lm_chroma_enabled_flag" );
  if ( pcSPS->getUseLMChroma() && pcSPS->getChromaFormatIdc() == CHROMA_420 )
  {
    WRITE_FLAG( pcSPS->getCclmCollocatedChromaFlag() ? 1 : 0,                                  "sps_cclm_collocated_chroma_flag" );
  }

  WRITE_FLAG( pcSPS->getUseMTS() ? 1 : 0,                                                      "mts_enabled_flag" );
  if ( pcSPS->getUseMTS() )
  {
    WRITE_FLAG( pcSPS->getUseIntraMTS() ? 1 : 0,                                               "mts_intra_enabled_flag" );
    WRITE_FLAG( pcSPS->getUseInterMTS() ? 1 : 0,                                               "mts_inter_enabled_flag" );
  }
  WRITE_FLAG( pcSPS->getUseLFNST() ? 1 : 0,                                                    "lfnst_enabled_flag" );
  WRITE_FLAG( pcSPS->getUseSMVD() ? 1 : 0,                                                     "smvd_flag" );
  // KJS: sps_affine_enabled_flag
  WRITE_FLAG( pcSPS->getUseAffine() ? 1 : 0,                                                   "affine_flag" );
  if ( pcSPS->getUseAffine() )
  {
    WRITE_FLAG( pcSPS->getUseAffineType() ? 1 : 0,                                             "affine_type_flag" );
    WRITE_FLAG( pcSPS->getUsePROF() ? 1 : 0,                                                   "sps_prof_enabled_flag" );
    WRITE_FLAG( pcSPS->getAffineAmvrEnabledFlag() ? 1 : 0,                                     "sps_affine_amvr_enabled_flag" );
  }
  WRITE_FLAG( pcSPS->getUseGBi() ? 1 : 0,                                                      "gbi_flag" );
#if JVET_P0517_ADAPTIVE_COLOR_TRANSFORM
  if (pcSPS->getChromaFormatIdc() == CHROMA_444)
  {
    WRITE_FLAG(pcSPS->getUseColorTrans() ? 1 : 0, "act_flag");
  }
#endif
  if (pcSPS->getChromaFormatIdc() == CHROMA_444)
  {
    WRITE_FLAG(pcSPS->getPLTMode() ? 1 : 0,                                                    "plt_flag" );
  }
  WRITE_FLAG(pcSPS->getIBCFlag() ? 1 : 0,                                                      "ibc_flag");

  // KJS: sps_ciip_enabled_flag
  WRITE_FLAG( pcSPS->getUseMHIntra() ? 1 : 0,                                                  "mhintra_flag" );

  if ( pcSPS->getUseMMVD() )
  {
    WRITE_FLAG( pcSPS->getFpelMmvdEnabledFlag() ? 1 : 0,                            "sps_fpel_mmvd_enabled_flag" );
  }
#if JVET_P0314_PROF_BDOF_DMVR_HLS
  if (pcSPS->getBDOFEnabledFlag())
  {
    WRITE_FLAG(pcSPS->getBdofControlPresentFlag() ? 1 : 0,                              "sps_bdof_picture_level_present_flag");
  }
  if (pcSPS->getUseDMVR())
  {
    WRITE_FLAG(pcSPS->getDmvrControlPresentFlag() ? 1 : 0,                              "sps_dmvr_picture_level_present_flag");
  }
  if (pcSPS->getUsePROF())
  {
    WRITE_FLAG(pcSPS->getProfControlPresentFlag() ? 1 : 0,                              "sps_prof_picture_level_present_flag");
  }
#else
  if(pcSPS->getBDOFEnabledFlag() || pcSPS->getUseDMVR())
  {
    WRITE_FLAG(pcSPS->getBdofDmvrSlicePresentFlag() ? 1 : 0,                            "sps_bdof_dmvr_slice_level_present_flag");
  }
#endif
  WRITE_FLAG( pcSPS->getUseTriangle() ? 1: 0,                                                  "triangle_flag" );

  WRITE_FLAG( pcSPS->getUseMIP() ? 1: 0,                                                       "sps_mip_flag" );
  // KJS: not in draft yet
  WRITE_FLAG( pcSPS->getUseSBT() ? 1 : 0,                                             "sbt_enable_flag");
#if !JVET_P0983_REMOVE_SPS_SBT_MAX_SIZE_FLAG
  if( pcSPS->getUseSBT() )
  {
    WRITE_FLAG(pcSPS->getMaxSbtSize() == 64 ? 1 : 0,                                  "max_sbt_size_64_flag");
  }
#endif
  // KJS: not in draft yet
  WRITE_FLAG(pcSPS->getUseReshaper() ? 1 : 0, "sps_reshaper_enable_flag");
  WRITE_FLAG( pcSPS->getUseISP() ? 1 : 0,                                             "isp_enable_flag");

#if LUMA_ADAPTIVE_DEBLOCKING_FILTER_QP_OFFSET
  WRITE_FLAG( pcSPS->getLadfEnabled() ? 1 : 0,                                                 "sps_ladf_enabled_flag" );
  if ( pcSPS->getLadfEnabled() )
  {
    WRITE_CODE( pcSPS->getLadfNumIntervals() - 2, 2,                                           "sps_num_ladf_intervals_minus2" );
    WRITE_SVLC( pcSPS->getLadfQpOffset( 0 ),                                                   "sps_ladf_lowest_interval_qp_offset");
    for ( int k = 1; k< pcSPS->getLadfNumIntervals(); k++ )
    {
      WRITE_SVLC( pcSPS->getLadfQpOffset( k ),                                                 "sps_ladf_qp_offset" );
      WRITE_UVLC( pcSPS->getLadfIntervalLowerBound( k ) - pcSPS->getLadfIntervalLowerBound( k - 1 ) - 1, "sps_ladf_delta_threshold_minus1" );
    }
  }
#endif

  // KJS: reference picture sets to be replaced


  // KJS: remove scaling lists?
  WRITE_FLAG( pcSPS->getScalingListFlag() ? 1 : 0,                                   "scaling_list_enabled_flag" );

#if JVET_P1006_PICTURE_HEADER
  WRITE_FLAG( pcSPS->getLoopFilterAcrossVirtualBoundariesDisabledFlag(), "sps_loop_filter_across_virtual_boundaries_disabled_present_flag" );
  if( pcSPS->getLoopFilterAcrossVirtualBoundariesDisabledFlag() )
  {
    WRITE_CODE( pcSPS->getNumVerVirtualBoundaries(), 2, "sps_num_ver_virtual_boundaries");
    for( unsigned i = 0; i < pcSPS->getNumVerVirtualBoundaries(); i++ )
    {
      WRITE_CODE((pcSPS->getVirtualBoundariesPosX(i)>>3), 13, "sps_virtual_boundaries_pos_x");
    }
    WRITE_CODE(pcSPS->getNumHorVirtualBoundaries(), 2, "sps_num_hor_virtual_boundaries");
    for( unsigned i = 0; i < pcSPS->getNumHorVirtualBoundaries(); i++ )
    {
      WRITE_CODE((pcSPS->getVirtualBoundariesPosY(i)>>3), 13, "sps_virtual_boundaries_pos_y");
    }
  }

#endif
  const TimingInfo *timingInfo = pcSPS->getTimingInfo();
  WRITE_FLAG(pcSPS->getHrdParametersPresentFlag(),          "general_hrd_parameters_present_flag");
    if( pcSPS->getHrdParametersPresentFlag() )
  {
    WRITE_CODE(timingInfo->getNumUnitsInTick(), 32,           "num_units_in_tick");
    WRITE_CODE(timingInfo->getTimeScale(),      32,           "time_scale");
    WRITE_FLAG(pcSPS->getSubLayerParametersPresentFlag(), "sub_layer_cpb_parameters_present_flag");
    if (pcSPS->getSubLayerParametersPresentFlag())
    {
      codeHrdParameters(pcSPS->getHrdParameters(), 0, pcSPS->getMaxTLayers() - 1);
    }
    else
    {
      codeHrdParameters(pcSPS->getHrdParameters(), pcSPS->getMaxTLayers() - 1, pcSPS->getMaxTLayers() - 1);
    }
  }

  WRITE_FLAG( pcSPS->getVuiParametersPresentFlag(),            "vui_parameters_present_flag" );
  if (pcSPS->getVuiParametersPresentFlag())
  {
    codeVUI(pcSPS->getVuiParameters(), pcSPS);
  }

  bool sps_extension_present_flag=false;
  bool sps_extension_flags[NUM_SPS_EXTENSION_FLAGS]={false};

  sps_extension_flags[SPS_EXT__REXT] = pcSPS->getSpsRangeExtension().settingsDifferFromDefaults();

  // Other SPS extension flags checked here.

  for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++)
  {
    sps_extension_present_flag|=sps_extension_flags[i];
  }

  WRITE_FLAG( (sps_extension_present_flag?1:0), "sps_extension_present_flag" );

  if (sps_extension_present_flag)
  {
#if ENABLE_TRACING /*|| RExt__DECODER_DEBUG_BIT_STATISTICS*/
    static const char *syntaxStrings[]={ "sps_range_extension_flag",
      "sps_multilayer_extension_flag",
      "sps_extension_6bits[0]",
      "sps_extension_6bits[1]",
      "sps_extension_6bits[2]",
      "sps_extension_6bits[3]",
      "sps_extension_6bits[4]",
      "sps_extension_6bits[5]" };
#endif

    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++)
    {
      WRITE_FLAG( sps_extension_flags[i]?1:0, syntaxStrings[i] );
    }

    for(int i=0; i<NUM_SPS_EXTENSION_FLAGS; i++) // loop used so that the order is determined by the enum.
    {
      if (sps_extension_flags[i])
      {
        switch (SPSExtensionFlagIndex(i))
        {
        case SPS_EXT__REXT:
        {
          const SPSRExt &spsRangeExtension=pcSPS->getSpsRangeExtension();

          WRITE_FLAG( (spsRangeExtension.getTransformSkipRotationEnabledFlag() ? 1 : 0),      "transform_skip_rotation_enabled_flag");
          WRITE_FLAG( (spsRangeExtension.getTransformSkipContextEnabledFlag() ? 1 : 0),       "transform_skip_context_enabled_flag");
          WRITE_FLAG( (spsRangeExtension.getRdpcmEnabledFlag(RDPCM_SIGNAL_IMPLICIT) ? 1 : 0), "implicit_rdpcm_enabled_flag" );
          WRITE_FLAG( (spsRangeExtension.getRdpcmEnabledFlag(RDPCM_SIGNAL_EXPLICIT) ? 1 : 0), "explicit_rdpcm_enabled_flag" );
          WRITE_FLAG( (spsRangeExtension.getExtendedPrecisionProcessingFlag() ? 1 : 0),       "extended_precision_processing_flag" );
          WRITE_FLAG( (spsRangeExtension.getIntraSmoothingDisabledFlag() ? 1 : 0),            "intra_smoothing_disabled_flag" );
          WRITE_FLAG( (spsRangeExtension.getHighPrecisionOffsetsEnabledFlag() ? 1 : 0),       "high_precision_offsets_enabled_flag" );
          WRITE_FLAG( (spsRangeExtension.getPersistentRiceAdaptationEnabledFlag() ? 1 : 0),   "persistent_rice_adaptation_enabled_flag" );
          WRITE_FLAG( (spsRangeExtension.getCabacBypassAlignmentEnabledFlag() ? 1 : 0),       "cabac_bypass_alignment_enabled_flag" );
          break;
        }
        default:
          CHECK(sps_extension_flags[i]!=false, "Unknown PPS extension signalled"); // Should never get here with an active SPS extension flag.
          break;
        }
      }
    }
  }
  xWriteRbspTrailingBits();
}

void HLSWriter::codeDPS( const DPS* dps )
{
#if ENABLE_TRACING
  xTraceDPSHeader();
#endif
  WRITE_CODE( dps->getDecodingParameterSetId(),     4,        "dps_decoding_parameter_set_id" );
  WRITE_CODE( dps->getMaxSubLayersMinus1(),         3,        "dps_max_sub_layers_minus1" );
  WRITE_FLAG( 0,                                              "dps_reserved_zero_bit" );

  ProfileTierLevel ptl = dps->getProfileTierLevel();
  codeProfileTierLevel( &ptl, dps->getMaxSubLayersMinus1() );

  WRITE_FLAG( 0,                                              "dps_extension_flag" );
  xWriteRbspTrailingBits();
}

void HLSWriter::codeVPS(const VPS* pcVPS)
{
#if ENABLE_TRACING
  xTraceVPSHeader();
#endif
  WRITE_CODE(pcVPS->getVPSId(), 4, "vps_video_parameter_set_id");
  WRITE_CODE(pcVPS->getMaxLayers() - 1, 8, "vps_max_layers_minus1");
  for (uint32_t i = 0; i <= pcVPS->getMaxLayers() - 1; i++)
  {
    WRITE_CODE(pcVPS->getVPSIncludedLayerId(i), 7, "vps_included_layer_id");
    WRITE_FLAG(0, "vps_reserved_zero_1bit");
  }

  WRITE_FLAG(0, "vps_extension_flag");

  //future extensions here..
  xWriteRbspTrailingBits();
}

#if JVET_P1006_PICTURE_HEADER
void HLSWriter::codePictureHeader( PicHeader* picHeader )
{
  const PPS*  pps = NULL;
  const SPS*  sps = NULL;
  
#if ENABLE_TRACING
  xTracePictureHeader ();
#endif

  CodingStructure& cs = *picHeader->getPic()->cs;

  WRITE_FLAG(picHeader->getNonReferencePictureFlag(), "non_reference_picture_flag");
  WRITE_FLAG(picHeader->getGdrPicFlag(), "gdr_pic_flag");
  WRITE_FLAG(picHeader->getNoOutputOfPriorPicsFlag(), "no_output_of_prior_pics_flag");
  if( picHeader->getGdrPicFlag() ) 
  {
    WRITE_UVLC(picHeader->getRecoveryPocCnt(), "recovery_poc_cnt");
  }
  else 
  {
    picHeader->setRecoveryPocCnt( 0 );
  }
  
  // parameter sets
  WRITE_UVLC(picHeader->getPPSId(), "ph_pic_parameter_set_id");
  pps = cs.slice->getPPS();
  CHECK(pps==0, "Invalid PPS");  
  sps = cs.slice->getSPS();
  CHECK(sps==0, "Invalid SPS");
  
  // sub-picture IDs
  if( sps->getSubPicIdPresentFlag() ) 
  {
    if( sps->getSubPicIdSignallingPresentFlag() ) 
    {
      for( int picIdx = 0; picIdx < sps->getNumSubPics( ); picIdx++ )
      {
        picHeader->setSubPicId( picIdx, sps->getSubPicId( picIdx ) );
      }
    }
    else 
    {
      WRITE_FLAG(picHeader->getSubPicIdSignallingPresentFlag(), "ph_subpic_id_signalling_present_flag");
      if( picHeader->getSubPicIdSignallingPresentFlag() )
      {
        WRITE_UVLC( picHeader->getSubPicIdLen() - 1, "ph_subpic_id_len_minus1" );
        for( int picIdx = 0; picIdx < sps->getNumSubPics( ); picIdx++ )
        {
          WRITE_CODE(picHeader->getSubPicId(picIdx), picHeader->getSubPicIdLen( ), "ph_subpic_id[i]" );
        }
      }
      else 
      {
        for( int picIdx = 0; picIdx < pps->getNumSubPics( ); picIdx++ )
        {
          picHeader->setSubPicId( picIdx, pps->getSubPicId( picIdx ) );
        }
      }
    }
  }
  else 
  {
    for( int picIdx = 0; picIdx < sps->getNumSubPics( ); picIdx++ )
    {
      picHeader->setSubPicId( picIdx, picIdx );
    }
  }

  // virtual boundaries
  if( !sps->getLoopFilterAcrossVirtualBoundariesDisabledFlag() )
  {
    WRITE_FLAG( picHeader->getLoopFilterAcrossVirtualBoundariesDisabledFlag(), "ph_loop_filter_across_virtual_boundaries_disabled_present_flag" );
    if( picHeader->getLoopFilterAcrossVirtualBoundariesDisabledFlag() )
    {
      WRITE_CODE(picHeader->getNumVerVirtualBoundaries(), 2, "ph_num_ver_virtual_boundaries");
      for( unsigned i = 0; i < picHeader->getNumVerVirtualBoundaries(); i++ )
      {
        WRITE_CODE(picHeader->getVirtualBoundariesPosX(i) >> 3, 13, "ph_virtual_boundaries_pos_x");
      }
      WRITE_CODE(picHeader->getNumHorVirtualBoundaries(), 2, "ph_num_hor_virtual_boundaries");
      for( unsigned i = 0; i < picHeader->getNumHorVirtualBoundaries(); i++ )
      {
        WRITE_CODE(picHeader->getVirtualBoundariesPosY(i)>>3, 13, "ph_virtual_boundaries_pos_y");
      }
    }
    else
    {
      picHeader->setLoopFilterAcrossVirtualBoundariesDisabledFlag( 0 );
      picHeader->setNumVerVirtualBoundaries( 0 );
      picHeader->setNumHorVirtualBoundaries( 0 );
    }
  }
  else
  {
    picHeader->setLoopFilterAcrossVirtualBoundariesDisabledFlag( sps->getLoopFilterAcrossVirtualBoundariesDisabledFlag() );
    picHeader->setNumVerVirtualBoundaries( sps->getNumVerVirtualBoundaries() );
    picHeader->setNumHorVirtualBoundaries( sps->getNumHorVirtualBoundaries() );
    for( unsigned i = 0; i < 3; i++ ) 
    {
      picHeader->setVirtualBoundariesPosX( sps->getVirtualBoundariesPosX(i), i );
      picHeader->setVirtualBoundariesPosY( sps->getVirtualBoundariesPosY(i), i );
    }
  }
  
  // 4:4:4 colour plane ID
  if( sps->getSeparateColourPlaneFlag() )
  {
    WRITE_CODE( picHeader->getColourPlaneId(), 2, "colour_plane_id" );
  }
  else 
  {
    picHeader->setColourPlaneId( 0 );
  }
  
  // picture output flag
  if( pps->getOutputFlagPresentFlag() )
  {
    WRITE_FLAG( picHeader->getPicOutputFlag(), "pic_output_flag" );
  }
  else 
  {
    picHeader->setPicOutputFlag(true);
  }

  // reference picture lists
  WRITE_FLAG( picHeader->getPicRplPresentFlag(), "pic_rpl_present_flag" ); 
  if( picHeader->getPicRplPresentFlag() )
  {
    // List0 and List1
    for(int listIdx = 0; listIdx < 2; listIdx++) 
    {                 
      // copy L1 index from L0 index
      if (listIdx == 1 && !pps->getRpl1IdxPresentFlag())
      {
        picHeader->setRPL1idx(picHeader->getRPL0idx());
      }      
      // RPL in picture header or SPS
      else if (sps->getNumRPL(listIdx) > 0)
      {
        if (!pps->getPPSRefPicListSPSIdc(listIdx))
        {
          WRITE_FLAG(picHeader->getRPLIdx(listIdx) != -1 ? 1 : 0, "pic_rpl_sps_flag[i]");
        }
        else if (pps->getPPSRefPicListSPSIdc( listIdx ) == 1)
        {
          picHeader->setRPLIdx( listIdx, -1);
        }
      }
      else 
      {
          picHeader->setRPLIdx( listIdx, -1 );
      }

      // use list from SPS
      if (picHeader->getRPLIdx(listIdx) != -1)
      {
        if (listIdx == 1 && !pps->getRpl1IdxPresentFlag())
        {
        }
        else if (sps->getNumRPL( listIdx ) > 1)
        {
          int numBits = ceilLog2(sps->getNumRPL( listIdx ));
          WRITE_CODE(picHeader->getRPLIdx(listIdx), numBits, "pic_rpl_idx[i]");
        }
        else
        {
          picHeader->setRPLIdx( listIdx, 0 );
        }
        picHeader->setRPL( listIdx, sps->getRPLList( listIdx )->getReferencePictureList(picHeader->getRPLIdx(listIdx)));
      }
      // explicit RPL in picture header
      else
      {
        xCodeRefPicList( picHeader->getRPL(listIdx), sps->getLongTermRefsPresent(), sps->getBitsForPOC(), !sps->getUseWP() && !sps->getUseWPBiPred() );
      }

      // POC MSB cycle signalling for LTRP
      if (picHeader->getRPL(listIdx)->getNumberOfLongtermPictures())
      {
        for (int i = 0; i < picHeader->getRPL(listIdx)->getNumberOfLongtermPictures() + picHeader->getRPL(listIdx)->getNumberOfShorttermPictures(); i++)
        {
          if (picHeader->getRPL(listIdx)->isRefPicLongterm(i))
          {
            if (picHeader->getRPL(listIdx)->getLtrpInSliceHeaderFlag())
            { 
              WRITE_CODE(picHeader->getRPL(listIdx)->getRefPicIdentifier(i), sps->getBitsForPOC(),
                         "pic_poc_lsb_lt[listIdx][rplsIdx][j]");
            }
            WRITE_FLAG(picHeader->getLocalRPL(listIdx)->getDeltaPocMSBPresentFlag(i) ? 1 : 0, "pic_delta_poc_msb_present_flag[i][j]");
            if (picHeader->getLocalRPL(listIdx)->getDeltaPocMSBPresentFlag(i))
            {
              WRITE_UVLC(picHeader->getLocalRPL(listIdx)->getDeltaPocMSBCycleLT(i), "pic_delta_poc_msb_cycle_lt[i][j]");
            }
          }
        }
      }
    }
  }

  // partitioning constraint overrides
  if (sps->getSplitConsOverrideEnabledFlag())
  {
    WRITE_FLAG(picHeader->getSplitConsOverrideFlag(), "partition_constraints_override_flag");
    if (picHeader->getSplitConsOverrideFlag())
    {
      WRITE_UVLC(floorLog2(picHeader->getMinQTSize(I_SLICE)) - sps->getLog2MinCodingBlockSize(), "pic_log2_diff_min_qt_min_cb_intra_slice_luma");
      WRITE_UVLC(floorLog2(picHeader->getMinQTSize(P_SLICE)) - sps->getLog2MinCodingBlockSize(), "pic_log2_diff_min_qt_min_cb_inter_slice");
      WRITE_UVLC(picHeader->getMaxMTTHierarchyDepth(P_SLICE),  "pic_max_mtt_hierarchy_depth_inter_slice");
      WRITE_UVLC(picHeader->getMaxMTTHierarchyDepth(I_SLICE), "pic_max_mtt_hierarchy_depth_intra_slice_luma");
      if (picHeader->getMaxMTTHierarchyDepth(I_SLICE) != 0)
      {
        WRITE_UVLC(floorLog2(picHeader->getMaxBTSize(I_SLICE)) - floorLog2(picHeader->getMinQTSize(I_SLICE)), "pic_log2_diff_max_bt_min_qt_intra_slice_luma");
        WRITE_UVLC(floorLog2(picHeader->getMaxTTSize(I_SLICE)) - floorLog2(picHeader->getMinQTSize(I_SLICE)), "pic_log2_diff_max_tt_min_qt_intra_slice_luma");
      }
      if (picHeader->getMaxMTTHierarchyDepth(P_SLICE) != 0)
      {
        WRITE_UVLC(floorLog2(picHeader->getMaxBTSize(P_SLICE)) - floorLog2(picHeader->getMinQTSize(P_SLICE)), "pic_log2_diff_max_bt_min_qt_inter_slice");
        WRITE_UVLC(floorLog2(picHeader->getMaxTTSize(P_SLICE)) - floorLog2(picHeader->getMinQTSize(P_SLICE)), "pic_log2_diff_max_tt_min_qt_inter_slice");
      }
      if (sps->getUseDualITree())
      {
        WRITE_UVLC(floorLog2(picHeader->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)) - sps->getLog2MinCodingBlockSize(), "pic_log2_diff_min_qt_min_cb_intra_slice_chroma");
        WRITE_UVLC(picHeader->getMaxMTTHierarchyDepth(I_SLICE, CHANNEL_TYPE_CHROMA), "pic_max_mtt_hierarchy_depth_intra_slice_chroma");
        if (picHeader->getMaxMTTHierarchyDepth(I_SLICE, CHANNEL_TYPE_CHROMA) != 0)
        {
          WRITE_UVLC(floorLog2(picHeader->getMaxBTSize(I_SLICE, CHANNEL_TYPE_CHROMA)) - floorLog2(picHeader->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)), "pic_log2_diff_max_bt_min_qt_intra_slice_chroma");
          WRITE_UVLC(floorLog2(picHeader->getMaxTTSize(I_SLICE, CHANNEL_TYPE_CHROMA)) - floorLog2(picHeader->getMinQTSize(I_SLICE, CHANNEL_TYPE_CHROMA)), "pic_log2_diff_max_tt_min_qt_intra_slice_chroma");
        }
      }
    }
  }
  else 
  {
    picHeader->setSplitConsOverrideFlag(0);
  }

  // inherit constraint values from SPS
  if (!sps->getSplitConsOverrideEnabledFlag() || !picHeader->getSplitConsOverrideFlag()) 
  {
      picHeader->setMinQTSizes(sps->getMinQTSizes());
      picHeader->setMaxMTTHierarchyDepths(sps->getMaxMTTHierarchyDepths());
      picHeader->setMaxBTSizes(sps->getMaxBTSizes());
      picHeader->setMaxTTSizes(sps->getMaxTTSizes());
  }

  // delta quantization and chrom and chroma offset
  if (pps->getUseDQP())
  {
    WRITE_UVLC( picHeader->getCuQpDeltaSubdivIntra(), "pic_cu_qp_delta_subdiv_intra_slice" );
    WRITE_UVLC( picHeader->getCuQpDeltaSubdivInter(), "pic_cu_qp_delta_subdiv_inter_slice" );
  }
  else 
  {
    picHeader->setCuQpDeltaSubdivIntra( 0 );
    picHeader->setCuQpDeltaSubdivInter( 0 );
  }
  if (pps->getCuChromaQpOffsetEnabledFlag())
  {
    WRITE_UVLC( picHeader->getCuChromaQpOffsetSubdivIntra(), "pic_cu_chroma_qp_offset_subdiv_intra_slice" );
    WRITE_UVLC( picHeader->getCuChromaQpOffsetSubdivInter(), "pic_cu_chroma_qp_offset_subdiv_inter_slice" );
  }
  else 
  {
    picHeader->setCuChromaQpOffsetSubdivIntra( 0 );
    picHeader->setCuChromaQpOffsetSubdivInter( 0 );
  }
  
  // temporal motion vector prediction
  if (sps->getSPSTemporalMVPEnabledFlag())
  {
    WRITE_FLAG( picHeader->getEnableTMVPFlag(), "pic_temporal_mvp_enabled_flag" );
  }
  else
  {
    picHeader->setEnableTMVPFlag(false);
  }

  // mvd L1 zero flag
  if (!pps->getPPSMvdL1ZeroIdc())
  {
    WRITE_FLAG(picHeader->getMvdL1ZeroFlag(), "pic_mvd_l1_zero_flag");
  }
  else
  {
    picHeader->setMvdL1ZeroFlag( pps->getPPSMvdL1ZeroIdc() - 1 );
  }
   
  // merge candidate list size
  if (!pps->getPPSSixMinusMaxNumMergeCandPlus1())
  {
    CHECK(picHeader->getMaxNumMergeCand() > MRG_MAX_NUM_CANDS, "More merge candidates signalled than supported");
    WRITE_UVLC(MRG_MAX_NUM_CANDS - picHeader->getMaxNumMergeCand(), "pic_six_minus_max_num_merge_cand");
  }
  else
  {
    picHeader->setMaxNumMergeCand(MRG_MAX_NUM_CANDS - (pps->getPPSSixMinusMaxNumMergeCandPlus1() - 1));
  }

  // subblock merge candidate list size
  if ( sps->getUseAffine() )
  {
    CHECK( picHeader->getMaxNumAffineMergeCand() > AFFINE_MRG_MAX_NUM_CANDS, "More affine merge candidates signalled than supported" );
    WRITE_UVLC(AFFINE_MRG_MAX_NUM_CANDS - picHeader->getMaxNumAffineMergeCand(), "pic_five_minus_max_num_subblock_merge_cand");
  }
  else
  {
    picHeader->setMaxNumAffineMergeCand( sps->getSBTMVPEnabledFlag() && picHeader->getEnableTMVPFlag() );
  }

  // full-pel MMVD flag
  if (sps->getFpelMmvdEnabledFlag())
  {
    WRITE_FLAG( picHeader->getDisFracMMVD(), "pic_fpel_mmvd_enabled_flag" );
  }
  else
  {
    picHeader->setDisFracMMVD(false);
  }
  
#if JVET_P0314_PROF_BDOF_DMVR_HLS
  // picture level BDOF disable flags
  if (sps->getBdofControlPresentFlag())
  {
    WRITE_FLAG(picHeader->getDisBdofFlag(), "pic_disable_bdof_flag");
  }
  else
  {
    picHeader->setDisBdofFlag(0);
  }

  // picture level DMVR disable flags
  if (sps->getDmvrControlPresentFlag())
  {
    WRITE_FLAG(picHeader->getDisDmvrFlag(), "pic_disable_dmvr_flag");
  }
  else
  {
    picHeader->setDisDmvrFlag(0);
  }

  // picture level PROF disable flags
  if (sps->getProfControlPresentFlag())
  {
    WRITE_FLAG(picHeader->getDisProfFlag(), "pic_disable_prof_flag");
  }
  else
  {
    picHeader->setDisProfFlag(0);
  }
#else
  // picture level BDOF/DMVR/PROF disable flags
  if (sps->getBdofDmvrSlicePresentFlag())
  {
    WRITE_FLAG(picHeader->getDisBdofDmvrFlag(), "pic_disable_bdof_dmvr_flag");
  }
  else
  {
    picHeader->setDisBdofDmvrFlag(0);
  }
#endif

  // triangle merge candidate list size
  if (sps->getUseTriangle() && picHeader->getMaxNumMergeCand() >= 2)
  {
    if (!pps->getPPSMaxNumMergeCandMinusMaxNumTriangleCandPlus1())
    {
      CHECK(picHeader->getMaxNumMergeCand() < picHeader->getMaxNumTriangleCand(), "Incorrrect max number of triangle candidates!");
      WRITE_UVLC(picHeader->getMaxNumMergeCand() - picHeader->getMaxNumTriangleCand(), "pic_max_num_merge_cand_minus_max_num_triangle_cand");
    }
    else
    {
      picHeader->setMaxNumTriangleCand((uint32_t)(picHeader->getMaxNumMergeCand() - (pps->getPPSMaxNumMergeCandMinusMaxNumTriangleCandPlus1() - 1)));
    }    
  }

  // ibc merge candidate list size
  if (sps->getIBCFlag())
  {
    CHECK( picHeader->getMaxNumIBCMergeCand() > IBC_MRG_MAX_NUM_CANDS, "More IBC merge candidates signalled than supported" );
    WRITE_UVLC(IBC_MRG_MAX_NUM_CANDS - picHeader->getMaxNumIBCMergeCand(), "pic_six_minus_max_num_ibc_merge_cand");
  }

  // joint Cb/Cr sign flag
  if (sps->getJointCbCrEnabledFlag())
  {
    WRITE_FLAG( picHeader->getJointCbCrSignFlag(), "pic_joint_cbcr_sign_flag" );
  }
  else
  {
    picHeader->setJointCbCrSignFlag(false);
  }

  // sao enable flags
  if(sps->getSAOEnabledFlag())
  {
    WRITE_FLAG(picHeader->getSaoEnabledPresentFlag(), "pic_sao_enabled_present_flag");
    if (picHeader->getSaoEnabledPresentFlag())
    {    
      WRITE_FLAG(picHeader->getSaoEnabledFlag(CHANNEL_TYPE_LUMA), "slice_sao_luma_flag");  
      if (sps->getChromaFormatIdc() != CHROMA_400)
      {
        WRITE_FLAG(picHeader->getSaoEnabledFlag(CHANNEL_TYPE_CHROMA), "slice_sao_chroma_flag");
      }
    }
    else 
    {
      picHeader->setSaoEnabledFlag(CHANNEL_TYPE_LUMA,   true);
      picHeader->setSaoEnabledFlag(CHANNEL_TYPE_CHROMA, true);
    }
  }
  else 
  {
    picHeader->setSaoEnabledFlag(CHANNEL_TYPE_LUMA,   false);
    picHeader->setSaoEnabledFlag(CHANNEL_TYPE_CHROMA, false);
  }
  
  // alf enable flags and aps IDs
  if( sps->getALFEnabledFlag() )
  {
    WRITE_FLAG(picHeader->getAlfEnabledPresentFlag(), "pic_alf_enabled_present_flag");
    if (picHeader->getAlfEnabledPresentFlag()) 
    {
      WRITE_FLAG(picHeader->getAlfEnabledFlag(COMPONENT_Y), "pic_alf_enabled_flag");
      if (picHeader->getAlfEnabledFlag(COMPONENT_Y))
      {
        WRITE_CODE(picHeader->getNumAlfAps(), 3, "pic_num_alf_aps_ids_luma");
        const std::vector<int>&   apsId = picHeader->getAlfAPSs();
        for (int i = 0; i < picHeader->getNumAlfAps(); i++)
        {
          WRITE_CODE(apsId[i], 3, "pic_alf_aps_id_luma");
        }
        
        const int alfChromaIdc = picHeader->getAlfEnabledFlag(COMPONENT_Cb) + picHeader->getAlfEnabledFlag(COMPONENT_Cr) * 2 ;
        if (sps->getChromaFormatIdc() != CHROMA_400)
        {
          WRITE_CODE(alfChromaIdc, 2, "pic_alf_chroma_idc");
        }
        if (alfChromaIdc)
        {
          WRITE_CODE(picHeader->getAlfApsIdChroma(), 3, "pic_alf_aps_id_chroma");
        }
      }
    }
    else 
    {
      picHeader->setAlfEnabledFlag(COMPONENT_Y,  true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cb, true);
      picHeader->setAlfEnabledFlag(COMPONENT_Cr, true);
    }
  }
  else 
  {
    picHeader->setAlfEnabledFlag(COMPONENT_Y,  false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cb, false);
    picHeader->setAlfEnabledFlag(COMPONENT_Cr, false);
  }

  // dependent quantization
  if (!pps->getPPSDepQuantEnabledIdc())
  {
    WRITE_FLAG(picHeader->getDepQuantEnabledFlag(), "pic_dep_quant_enabled_flag");
  }
  else
  {
    picHeader->setDepQuantEnabledFlag( pps->getPPSDepQuantEnabledIdc() - 1 );
  }

  // sign data hiding
  if( !picHeader->getDepQuantEnabledFlag() )
  {
    WRITE_FLAG( picHeader->getSignDataHidingEnabledFlag(), "pic_sign_data_hiding_enabled_flag" );
  }
  else
  {
    picHeader->setSignDataHidingEnabledFlag(false);
  }

  // deblocking filter controls
  if (pps->getDeblockingFilterControlPresentFlag())
  {
    if(pps->getDeblockingFilterOverrideEnabledFlag())
    {    
      WRITE_FLAG ( picHeader->getDeblockingFilterOverridePresentFlag(), "pic_deblocking_filter_override_present_flag" );    
      if( picHeader->getDeblockingFilterOverridePresentFlag() ) 
      {
        WRITE_FLAG ( picHeader->getDeblockingFilterOverrideFlag(), "pic_deblocking_filter_override_flag" );
      }
      else
      {    
        picHeader->setDeblockingFilterOverrideFlag(false);
      }
    }
    else
    {
      picHeader->setDeblockingFilterOverridePresentFlag(false);
      picHeader->setDeblockingFilterOverrideFlag(false);
    }

    if(picHeader->getDeblockingFilterOverrideFlag())
    {
      WRITE_FLAG ( picHeader->getDeblockingFilterDisable(), "pic_deblocking_filter_disabled_flag" );
      if(!picHeader->getDeblockingFilterDisable())
      {
        WRITE_SVLC( picHeader->getDeblockingFilterBetaOffsetDiv2(), "pic_beta_offset_div2" );
        WRITE_SVLC( picHeader->getDeblockingFilterTcOffsetDiv2(), "pic_tc_offset_div2" );
      }
    }
    else
    {
      picHeader->setDeblockingFilterDisable       ( pps->getPPSDeblockingFilterDisabledFlag() );
      picHeader->setDeblockingFilterBetaOffsetDiv2( pps->getDeblockingFilterBetaOffsetDiv2() );
      picHeader->setDeblockingFilterTcOffsetDiv2  ( pps->getDeblockingFilterTcOffsetDiv2() );
    }
  }
  else
  {
    picHeader->setDeblockingFilterDisable       ( false );
    picHeader->setDeblockingFilterBetaOffsetDiv2( 0 );
    picHeader->setDeblockingFilterTcOffsetDiv2  ( 0 );
  }

  // luma mapping / chroma scaling controls
  if (sps->getUseReshaper())
  {
    WRITE_FLAG(picHeader->getLmcsEnabledFlag(), "pic_lmcs_enabled_flag");
    if (picHeader->getLmcsEnabledFlag())
    {
      WRITE_CODE(picHeader->getLmcsAPSId(), 2, "pic_lmcs_aps_id");
      if (sps->getChromaFormatIdc() != CHROMA_400)
      {
        WRITE_FLAG(picHeader->getLmcsChromaResidualScaleFlag(), "pic_chroma_residual_scale_flag");
      }
      else
      {
        picHeader->setLmcsChromaResidualScaleFlag(false);
      }
    }
  }
  else
  {
    picHeader->setLmcsEnabledFlag(false);
    picHeader->setLmcsChromaResidualScaleFlag(false);
  }

  // quantization scaling lists
  if( sps->getScalingListFlag() )
  {
    WRITE_FLAG( picHeader->getScalingListPresentFlag(), "pic_scaling_list_present_flag" );
    if( picHeader->getScalingListPresentFlag() )
    {
      WRITE_CODE( picHeader->getScalingListAPSId(), 3, "pic_scaling_list_aps_id" );
    }
  }
  else 
  {
    picHeader->setScalingListPresentFlag( false );
  }

  // picture header extension
  if(pps->getPictureHeaderExtensionPresentFlag())
  {
    WRITE_UVLC(0,"pic_segment_header_extension_length");
  }
  
  xWriteRbspTrailingBits();
}

#endif
void HLSWriter::codeSliceHeader         ( Slice* pcSlice )
{
#if ENABLE_TRACING
  xTraceSliceHeader ();
#endif

  CodingStructure& cs = *pcSlice->getPic()->cs;
#if JVET_P1006_PICTURE_HEADER
  const PicHeader *picHeader = cs.picHeader;
#endif
  const ChromaFormat format                = pcSlice->getSPS()->getChromaFormatIdc();
  const uint32_t         numberValidComponents = getNumberValidComponents(format);
  const bool         chromaEnabled         = isChromaEnabled(format);

#if JVET_P1006_PICTURE_HEADER
  int pocBits = pcSlice->getSPS()->getBitsForPOC();
  int pocMask = (1 << pocBits) - 1;
  WRITE_CODE(pcSlice->getPOC() & pocMask, pocBits, "slice_pic_order_cnt_lsb");
#endif
  
#if !JVET_P1006_PICTURE_HEADER
  WRITE_UVLC( pcSlice->getPPS()->getPPSId(), "slice_pic_parameter_set_id" );
#endif
  int bitsSliceAddress = 1;
  if (!pcSlice->getPPS()->getRectSliceFlag())
  {
    while (pcSlice->getPPS()->getNumBricksInPic() > (1 << bitsSliceAddress))
    {
      bitsSliceAddress++;
    }
  }
  else
  {
    if (pcSlice->getPPS()->getSignalledSliceIdFlag())
    {
      bitsSliceAddress = pcSlice->getPPS()->getSignalledSliceIdLengthMinus1() + 1;
    }
    else
    {
      while ((pcSlice->getPPS()->getNumSlicesInPicMinus1() + 1) > (1 << bitsSliceAddress))
      {
        bitsSliceAddress++;
      }
    }
  }
  if (pcSlice->getPPS()->getRectSliceFlag() || pcSlice->getPPS()->getNumBricksInPic() > 1)
  {
    if (pcSlice->getPPS()->getRectSliceFlag())
    {
      WRITE_CODE(pcSlice->getPPS()->getSliceId(pcSlice->getSliceIndex()), bitsSliceAddress, "slice_address");
    }
    else
    {
      WRITE_CODE(pcSlice->getSliceCurStartBrickIdx(), bitsSliceAddress, "slice_address");
    }
  }
  if (!pcSlice->getPPS()->getRectSliceFlag() && !pcSlice->getPPS()->getSingleBrickPerSliceFlag())
  {
    WRITE_UVLC(pcSlice->getSliceNumBricks() - 1, "num_bricks_in_slice_minus1");
  }

#if !JVET_P1006_PICTURE_HEADER
    WRITE_FLAG(pcSlice->getNonRefPictFlag() ? 1 : 0, "non_reference_picture_flag");
#endif

    WRITE_UVLC( pcSlice->getSliceType(), "slice_type" );


#if !JVET_P1006_PICTURE_HEADER
    int pocBits = pcSlice->getSPS()->getBitsForPOC();
    int pocMask = (1 << pocBits) - 1;
    WRITE_CODE(pcSlice->getPOC() & pocMask, pocBits, "slice_pic_order_cnt_lsb");
    if (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR)
    {
      int maxPicOrderCntLsb = (int) pow(2, pcSlice->getSPS()->getBitsForPOC());
      CHECK((pcSlice->getRecoveryPocCnt() < maxPicOrderCntLsb), "The value of recovery_poc_cnt exceeds (POC LSB cycle - 1)");
      WRITE_UVLC(pcSlice->getRecoveryPocCnt(), "recovery_poc_cnt");
    }
    if (pcSlice->getRapPicFlag() || (pcSlice->getNalUnitType() == NAL_UNIT_CODED_SLICE_GDR))
    {
      WRITE_FLAG(pcSlice->getNoOutputPriorPicsFlag() ? 1 : 0, "no_output_of_prior_pics_flag");
    }
    if (pcSlice->getPPS()->getOutputFlagPresentFlag())
    {
      WRITE_FLAG(pcSlice->getPicOutputFlag() ? 1 : 0, "pic_output_flag");
    }
#endif

#if JVET_P1006_PICTURE_HEADER
    if( !picHeader->getPicRplPresentFlag() && (!pcSlice->getIdrPicFlag() || pcSlice->getSPS()->getIDRRefParamListPresent()) )
#else
    if( !pcSlice->getIdrPicFlag() || pcSlice->getSPS()->getIDRRefParamListPresent())
#endif
    {
      //Write L0 related syntax elements
      if (pcSlice->getSPS()->getNumRPL0() > 0)
      {
        if (!pcSlice->getPPS()->getPPSRefPicListSPSIdc0())
        {
          WRITE_FLAG(pcSlice->getRPL0idx() != -1 ? 1 : 0, "ref_pic_list_sps_flag[0]");
        }
      }
      if (pcSlice->getRPL0idx() != -1)
      {
        if (pcSlice->getSPS()->getNumRPL0() > 1)
        {
          int numBits = 0;
          while ((1 << numBits) < pcSlice->getSPS()->getNumRPL0())
          {
            numBits++;
          }
          WRITE_CODE(pcSlice->getRPL0idx(), numBits, "ref_pic_list_idx[0]");
        }
      }
      else
      {  //write local RPL0
        xCodeRefPicList( pcSlice->getRPL0(), pcSlice->getSPS()->getLongTermRefsPresent(), pcSlice->getSPS()->getBitsForPOC(), !pcSlice->getSPS()->getUseWP() && !pcSlice->getSPS()->getUseWPBiPred() );
      }
      //Deal POC Msb cycle signalling for LTRP
      if (pcSlice->getRPL0()->getNumberOfLongtermPictures())
      {
        for (int i = 0; i < pcSlice->getRPL0()->getNumberOfLongtermPictures() + pcSlice->getRPL0()->getNumberOfShorttermPictures(); i++)
        {
          if (pcSlice->getRPL0()->isRefPicLongterm(i))
          {
            if (pcSlice->getRPL0()->getLtrpInSliceHeaderFlag())
            {
              WRITE_CODE(pcSlice->getRPL0()->getRefPicIdentifier(i), pcSlice->getSPS()->getBitsForPOC(),
                         "slice_poc_lsb_lt[listIdx][rplsIdx][j]");
            }
            WRITE_FLAG(pcSlice->getLocalRPL0()->getDeltaPocMSBPresentFlag(i) ? 1 : 0, "delta_poc_msb_present_flag[i][j]");
            if (pcSlice->getLocalRPL0()->getDeltaPocMSBPresentFlag(i))
            {
              WRITE_UVLC(pcSlice->getLocalRPL0()->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i][j]");
            }
          }
        }
      }

      //Write L1 related syntax elements
      if (!pcSlice->getPPS()->getRpl1IdxPresentFlag())
      {
        CHECK(pcSlice->getRPL1idx() != pcSlice->getRPL0idx(), "RPL1Idx is not signalled but it is not the same as RPL0Idx");
        if (pcSlice->getRPL1idx() == -1)
        {  //write local RPL1
          xCodeRefPicList( pcSlice->getRPL1(), pcSlice->getSPS()->getLongTermRefsPresent(), pcSlice->getSPS()->getBitsForPOC(), !pcSlice->getSPS()->getUseWP() && !pcSlice->getSPS()->getUseWPBiPred() );
        }
      }
      else
      {
        if (pcSlice->getSPS()->getNumRPL1() > 0)
        {
        if (!pcSlice->getPPS()->getPPSRefPicListSPSIdc1())
        {
          WRITE_FLAG(pcSlice->getRPL1idx() != -1 ? 1 : 0, "ref_pic_list_sps_flag[1]");
        }
        }
        if (pcSlice->getRPL1idx() != -1)
        {
          if (pcSlice->getSPS()->getNumRPL1() > 1)
          {
            int numBits = 0;
            while ((1 << numBits) < pcSlice->getSPS()->getNumRPL1())
            {
              numBits++;
            }
            WRITE_CODE(pcSlice->getRPL1idx(), numBits, "ref_pic_list_idx[1]");
          }
        }
        else
        {  //write local RPL1
          xCodeRefPicList( pcSlice->getRPL1(), pcSlice->getSPS()->getLongTermRefsPresent(), pcSlice->getSPS()->getBitsForPOC(), !pcSlice->getSPS()->getUseWP() && !pcSlice->getSPS()->getUseWPBiPred() );
        }
      }
      //Deal POC Msb cycle signalling for LTRP
      if (pcSlice->getRPL1()->getNumberOfLongtermPictures())
      {
        for (int i = 0; i < pcSlice->getRPL1()->getNumberOfLongtermPictures() + pcSlice->getRPL1()->getNumberOfShorttermPictures(); i++)
        {
          if (pcSlice->getRPL1()->isRefPicLongterm(i))
          {
            if (pcSlice->getRPL1()->getLtrpInSliceHeaderFlag())
            {
              WRITE_CODE(pcSlice->getRPL1()->getRefPicIdentifier(i), pcSlice->getSPS()->getBitsForPOC(),
                         "slice_poc_lsb_lt[listIdx][rplsIdx][j]");
            }
            WRITE_FLAG(pcSlice->getLocalRPL1()->getDeltaPocMSBPresentFlag(i) ? 1 : 0, "delta_poc_msb_present_flag[i][j]");
            if (pcSlice->getLocalRPL1()->getDeltaPocMSBPresentFlag(i))
            {
              WRITE_UVLC(pcSlice->getLocalRPL1()->getDeltaPocMSBCycleLT(i), "delta_poc_msb_cycle_lt[i][j]");
            }
          }
        }
      }
#if JVET_P1006_PICTURE_HEADER
    }

    if( picHeader->getPicRplPresentFlag() || !pcSlice->getIdrPicFlag() || pcSlice->getSPS()->getIDRRefParamListPresent() )
    {
#endif
      //check if numrefidxes match the defaults. If not, override

      if ((!pcSlice->isIntra() && pcSlice->getRPL0()->getNumRefEntries() > 1) ||
          (pcSlice->isInterB() && pcSlice->getRPL1()->getNumRefEntries() > 1) )
      {
        int defaultL0 = std::min<int>(pcSlice->getRPL0()->getNumRefEntries(), pcSlice->getPPS()->getNumRefIdxL0DefaultActive());
        int defaultL1 = pcSlice->isInterB() ? std::min<int>(pcSlice->getRPL1()->getNumRefEntries(), pcSlice->getPPS()->getNumRefIdxL1DefaultActive()) : 0;
        bool overrideFlag = ( pcSlice->getNumRefIdx( REF_PIC_LIST_0 ) != defaultL0 || ( pcSlice->isInterB() && pcSlice->getNumRefIdx( REF_PIC_LIST_1 ) != defaultL1 ) );
        WRITE_FLAG( overrideFlag ? 1 : 0, "num_ref_idx_active_override_flag" );
        if( overrideFlag )
        {
          if(pcSlice->getRPL0()->getNumRefEntries() > 1)
          {
            WRITE_UVLC( pcSlice->getNumRefIdx( REF_PIC_LIST_0 ) - 1, "num_ref_idx_l0_active_minus1" );
          }
          else
          {
            pcSlice->setNumRefIdx( REF_PIC_LIST_0, 1);
          }

          if( pcSlice->isInterB() && pcSlice->getRPL1()->getNumRefEntries() > 1)
          {
            WRITE_UVLC( pcSlice->getNumRefIdx( REF_PIC_LIST_1 ) - 1, "num_ref_idx_l1_active_minus1" );
          }
          else
          {
            pcSlice->setNumRefIdx( REF_PIC_LIST_1, pcSlice->isInterB() ? 1 : 0);
          }
        }
        else
        {
          pcSlice->setNumRefIdx( REF_PIC_LIST_0, defaultL0 );
          pcSlice->setNumRefIdx( REF_PIC_LIST_1, defaultL1 );
        }
      }
      else
      {
        pcSlice->setNumRefIdx( REF_PIC_LIST_0, pcSlice->isIntra() ? 0 : 1 );
        pcSlice->setNumRefIdx( REF_PIC_LIST_1, pcSlice->isInterB() ? 1 : 0 );
      }
    }

#if !JVET_P1006_PICTURE_HEADER
    if (
      pcSlice->getSPS()->getSplitConsOverrideEnabledFlag()
      )
    {
      WRITE_FLAG(pcSlice->getSplitConsOverrideFlag() ? 1 : 0, "partition_constraints_override_flag");
      if (pcSlice->getSplitConsOverrideFlag())
      {
        WRITE_UVLC(floorLog2(pcSlice->getMinQTSize()) - pcSlice->getSPS()->getLog2MinCodingBlockSize(), "slice_log2_diff_min_qt_min_cb");
        WRITE_UVLC(pcSlice->getMaxMTTHierarchyDepth(), "slice_max_mtt_hierarchy_depth_luma");
        if (pcSlice->getMaxMTTHierarchyDepth() != 0)
        {
          CHECK(pcSlice->getMaxBTSize() < pcSlice->getMinQTSize(), "maxBtSize is smaller than minQtSize");
          WRITE_UVLC(floorLog2(pcSlice->getMaxBTSize()) - floorLog2(pcSlice->getMinQTSize()), "slice_log2_diff_max_bt_min_qt");
          CHECK(pcSlice->getMaxTTSize() < pcSlice->getMinQTSize(), "maxTtSize is smaller than minQtSize");
          WRITE_UVLC(floorLog2(pcSlice->getMaxTTSize()) - floorLog2(pcSlice->getMinQTSize()), "slice_log2_diff_max_tt_min_qt");
        }
        if (
          pcSlice->isIntra() && pcSlice->getSPS()->getUseDualITree()
          )
        {
          WRITE_UVLC(floorLog2(pcSlice->getMinQTSizeIChroma()) - pcSlice->getSPS()->getLog2MinCodingBlockSize(), "slice_log2_diff_min_qt_min_cb_chroma");
          WRITE_UVLC(pcSlice->getMaxMTTHierarchyDepthIChroma(), "slice_max_mtt_hierarchy_depth_chroma");
          if (pcSlice->getMaxMTTHierarchyDepthIChroma() != 0)
          {
            CHECK(pcSlice->getMaxBTSizeIChroma() < pcSlice->getMinQTSizeIChroma(), "maxBtSizeC is smaller than minQtSizeC");
            WRITE_UVLC(floorLog2(pcSlice->getMaxBTSizeIChroma()) - floorLog2(pcSlice->getMinQTSizeIChroma()), "slice_log2_diff_max_bt_min_qt_chroma");
            CHECK(pcSlice->getMaxTTSizeIChroma() < pcSlice->getMinQTSizeIChroma(), "maxTtSizeC is smaller than minQtSizeC");
            WRITE_UVLC(floorLog2(pcSlice->getMaxTTSizeIChroma()) - floorLog2(pcSlice->getMinQTSizeIChroma()), "slice_log2_diff_max_tt_min_qt_chroma");
          }
        }
      }
    }

    if(!pcSlice->isIntra())
    {
#if JVET_P0206_TMVP_flags
      if( pcSlice->getSPS()->getSPSTemporalMVPEnabledFlag())
#else
      if( pcSlice->getSPS()->getSPSTemporalMVPEnabledFlag() && !pcSlice->getPPS()->getPPSTemporalMVPEnabledIdc() )
#endif
      {
        WRITE_FLAG( pcSlice->getEnableTMVPFlag() ? 1 : 0, "slice_temporal_mvp_enabled_flag" );
      }
    }

    if( pcSlice->isInterB() )
    {
      if (!pcSlice->getPPS()->getPPSMvdL1ZeroIdc())
      {
        WRITE_FLAG( pcSlice->getMvdL1ZeroFlag() ? 1 : 0, "mvd_l1_zero_flag" );
      }
    }
#endif

    if( !pcSlice->isIntra() )
    {
      if( !pcSlice->isIntra() && pcSlice->getPPS()->getCabacInitPresentFlag() )
      {
        SliceType sliceType = pcSlice->getSliceType();
        SliceType  encCABACTableIdx = pcSlice->getEncCABACTableIdx();
        bool encCabacInitFlag = ( sliceType != encCABACTableIdx && encCABACTableIdx != I_SLICE ) ? true : false;
        pcSlice->setCabacInitFlag( encCabacInitFlag );
        WRITE_FLAG( encCabacInitFlag ? 1 : 0, "cabac_init_flag" );
      }
    }

#if JVET_P1006_PICTURE_HEADER
    if( pcSlice->getPicHeader()->getEnableTMVPFlag() )
#else
    if( pcSlice->getEnableTMVPFlag() )
#endif
    {
      if( pcSlice->getSliceType() == B_SLICE )
      {
        if (!pcSlice->getPPS()->getPPSCollocatedFromL0Idc())
        {
          WRITE_FLAG( pcSlice->getColFromL0Flag(), "collocated_from_l0_flag" );
        }
      }

      if( pcSlice->getSliceType() != I_SLICE &&
        ( ( pcSlice->getColFromL0Flag() == 1 && pcSlice->getNumRefIdx( REF_PIC_LIST_0 ) > 1 ) ||
          ( pcSlice->getColFromL0Flag() == 0 && pcSlice->getNumRefIdx( REF_PIC_LIST_1 ) > 1 ) ) )
      {
        WRITE_UVLC( pcSlice->getColRefIdx(), "collocated_ref_idx" );
      }
    }

    if( ( pcSlice->getPPS()->getUseWP() && pcSlice->getSliceType() == P_SLICE ) || ( pcSlice->getPPS()->getWPBiPred() && pcSlice->getSliceType() == B_SLICE ) )
    {
      xCodePredWeightTable( pcSlice );
    }

#if !JVET_P1006_PICTURE_HEADER
    if (!cs.slice->isIntra())
    {
      CHECK(pcSlice->getMaxNumMergeCand() > MRG_MAX_NUM_CANDS, "More merge candidates signalled than supported");
      if (!pcSlice->getPPS()->getPPSSixMinusMaxNumMergeCandPlus1())
      {
        WRITE_UVLC(MRG_MAX_NUM_CANDS - pcSlice->getMaxNumMergeCand(), "six_minus_max_num_merge_cand");
      }
    }
    if( !pcSlice->isIntra() )
    {
      if (pcSlice->getSPS()->getSBTMVPEnabledFlag() && pcSlice->getEnableTMVPFlag() && !pcSlice->getSPS()->getUseAffine())// ATMVP only
      {
        CHECK( pcSlice->getMaxNumAffineMergeCand() != 1, "Sub-block merge can number should be 1" );
      }
      else
      if (!(pcSlice->getSPS()->getSBTMVPEnabledFlag() && pcSlice->getEnableTMVPFlag()) && !pcSlice->getSPS()->getUseAffine()) // both off
      {
        CHECK( pcSlice->getMaxNumAffineMergeCand() != 0, "Sub-block merge can number should be 0" );
      }
      else
      if ( pcSlice->getSPS()->getUseAffine() )
      {
        CHECK( pcSlice->getMaxNumAffineMergeCand() > AFFINE_MRG_MAX_NUM_CANDS, "More affine merge candidates signalled than supported" );
#if JVET_P0152_REMOVE_PPS_NUM_SUBBLOCK_MERGE_CAND
        WRITE_UVLC( AFFINE_MRG_MAX_NUM_CANDS - pcSlice->getMaxNumAffineMergeCand(), "five_minus_max_num_subblock_merge_cand" );
#else
        if (!pcSlice->getPPS()->getPPSFiveMinusMaxNumSubblockMergeCandPlus1())
        {
          WRITE_UVLC( AFFINE_MRG_MAX_NUM_CANDS - pcSlice->getMaxNumAffineMergeCand(), "five_minus_max_num_subblock_merge_cand" );
        }
#endif
      }
      if ( pcSlice->getSPS()->getFpelMmvdEnabledFlag() )
      {
        WRITE_FLAG( pcSlice->getDisFracMMVD(), "slice_fpel_mmvd_enabled_flag" );
      }
      if (pcSlice->getSPS()->getBdofDmvrSlicePresentFlag())
      {
        WRITE_FLAG(pcSlice->getDisBdofDmvrFlag(), "slice_disable_bdof_dmvr_flag");
      }
      if (pcSlice->getSPS()->getUseTriangle() && pcSlice->getMaxNumMergeCand() >= 2)
      {
        CHECK(pcSlice->getMaxNumMergeCand() < pcSlice->getMaxNumTriangleCand(), "Incorrrect max number of triangle candidates!");
        if (!pcSlice->getPPS()->getPPSMaxNumMergeCandMinusMaxNumTriangleCandPlus1())
        {
          WRITE_UVLC(pcSlice->getMaxNumMergeCand() - pcSlice->getMaxNumTriangleCand(), "max_num_merge_cand_minus_max_num_triangle_cand");
        }
      }
      else
      {
        pcSlice->setMaxNumTriangleCand(0);
      }
    }
    if ( pcSlice->getSPS()->getIBCFlag() )
    {
      CHECK( pcSlice->getMaxNumIBCMergeCand() > IBC_MRG_MAX_NUM_CANDS, "More IBC merge candidates signalled than supported" );
      WRITE_UVLC( IBC_MRG_MAX_NUM_CANDS - pcSlice->getMaxNumIBCMergeCand(), "slice_six_minus_max_num_ibc_merge_cand" );
    }
    if (pcSlice->getSPS()->getJointCbCrEnabledFlag())
    {
      WRITE_FLAG( pcSlice->getJointCbCrSignFlag() ? 1 : 0, "slice_joint_cbcr_sign_flag");
    }
#endif

    int iCode = pcSlice->getSliceQp() - ( pcSlice->getPPS()->getPicInitQPMinus26() + 26 );
    WRITE_SVLC( iCode, "slice_qp_delta" );
    if (pcSlice->getPPS()->getSliceChromaQpFlag())
    {
      if (numberValidComponents > COMPONENT_Cb)
      {
        WRITE_SVLC( pcSlice->getSliceChromaQpDelta(COMPONENT_Cb), "slice_cb_qp_offset" );
      }
      if (numberValidComponents > COMPONENT_Cr)
      {
        WRITE_SVLC( pcSlice->getSliceChromaQpDelta(COMPONENT_Cr), "slice_cr_qp_offset" );
        if (pcSlice->getSPS()->getJointCbCrEnabledFlag())
        {
          WRITE_SVLC( pcSlice->getSliceChromaQpDelta(JOINT_CbCr), "slice_joint_cbcr_qp_offset");
        }
      }
      CHECK(numberValidComponents < COMPONENT_Cr+1, "Too many valid components");
    }

    if (pcSlice->getPPS()->getCuChromaQpOffsetEnabledFlag())
    {
      WRITE_FLAG(pcSlice->getUseChromaQpAdj(), "cu_chroma_qp_offset_enabled_flag");
    }

#if JVET_P1006_PICTURE_HEADER
    if( pcSlice->getSPS()->getSAOEnabledFlag() && !picHeader->getSaoEnabledPresentFlag() )
#else
    if( pcSlice->getSPS()->getSAOEnabledFlag() )
#endif
    {
      WRITE_FLAG( pcSlice->getSaoEnabledFlag( CHANNEL_TYPE_LUMA ), "slice_sao_luma_flag" );
      if( chromaEnabled )
      {
        WRITE_FLAG( pcSlice->getSaoEnabledFlag( CHANNEL_TYPE_CHROMA ), "slice_sao_chroma_flag" );
      }
    }

#if JVET_P1006_PICTURE_HEADER
    if( pcSlice->getSPS()->getALFEnabledFlag() && !picHeader->getAlfEnabledPresentFlag() )
#else
    if( pcSlice->getSPS()->getALFEnabledFlag() )
#endif
    {
      const int alfEnabled = pcSlice->getTileGroupAlfEnabledFlag(COMPONENT_Y);
      WRITE_FLAG(alfEnabled, "slice_alf_enabled_flag");

      if (alfEnabled)
      {
        WRITE_CODE(pcSlice->getTileGroupNumAps(), 3, "slice_num_alf_aps_ids_luma");
        const std::vector<int>&   apsId = pcSlice->getTileGroupApsIdLuma();
        for (int i = 0; i < pcSlice->getTileGroupNumAps(); i++)
        {
          WRITE_CODE(apsId[i], 3, "slice_alf_aps_id_luma");
        }

        const int alfChromaIdc = pcSlice->getTileGroupAlfEnabledFlag(COMPONENT_Cb) + pcSlice->getTileGroupAlfEnabledFlag(COMPONENT_Cr) * 2 ;
        if (chromaEnabled)
        {
          WRITE_CODE(alfChromaIdc, 2, "slice_alf_chroma_idc");
        }
        if (alfChromaIdc)
        {
          WRITE_CODE(pcSlice->getTileGroupApsIdChroma(), 3, "slice_alf_aps_id_chroma");
        }
      }
    }

#if !JVET_P1006_PICTURE_HEADER
    if (!pcSlice->getPPS()->getPPSDepQuantEnabledIdc())
    {
      WRITE_FLAG( pcSlice->getDepQuantEnabledFlag() ? 1 : 0, "dep_quant_enabled_flag" );
    }
    if( !pcSlice->getDepQuantEnabledFlag() )
    {
      WRITE_FLAG( pcSlice->getSignDataHidingEnabledFlag() ? 1 : 0, "sign_data_hiding_enabled_flag" );
    }
    else
    {
      CHECK( pcSlice->getSignDataHidingEnabledFlag(), "sign data hiding not supported when dependent quantization is enabled" );
    }
#endif
    if (pcSlice->getPPS()->getDeblockingFilterControlPresentFlag())
    {
#if JVET_P1006_PICTURE_HEADER
      if (pcSlice->getPPS()->getDeblockingFilterOverrideEnabledFlag() && !picHeader->getDeblockingFilterOverridePresentFlag())
#else
      if (pcSlice->getPPS()->getDeblockingFilterOverrideEnabledFlag() )
#endif
      {
        WRITE_FLAG(pcSlice->getDeblockingFilterOverrideFlag(), "deblocking_filter_override_flag");
      }
#if JVET_P1006_PICTURE_HEADER
      else
      {
        pcSlice->setDeblockingFilterOverrideFlag(0);
      }
#endif
      if (pcSlice->getDeblockingFilterOverrideFlag())
      {
        WRITE_FLAG(pcSlice->getDeblockingFilterDisable(), "slice_deblocking_filter_disabled_flag");
        if(!pcSlice->getDeblockingFilterDisable())
        {
          WRITE_SVLC (pcSlice->getDeblockingFilterBetaOffsetDiv2(), "slice_beta_offset_div2");
          WRITE_SVLC (pcSlice->getDeblockingFilterTcOffsetDiv2(),   "slice_tc_offset_div2");
        }
      }
#if JVET_P1006_PICTURE_HEADER
      else
      {
        pcSlice->setDeblockingFilterDisable       ( picHeader->getDeblockingFilterDisable() );
        pcSlice->setDeblockingFilterBetaOffsetDiv2( picHeader->getDeblockingFilterBetaOffsetDiv2() );
        pcSlice->setDeblockingFilterTcOffsetDiv2  ( picHeader->getDeblockingFilterTcOffsetDiv2() );
      }
#endif
    }
#if JVET_P1006_PICTURE_HEADER
    else
    {
      pcSlice->setDeblockingFilterDisable       ( false );
      pcSlice->setDeblockingFilterBetaOffsetDiv2( 0 );
      pcSlice->setDeblockingFilterTcOffsetDiv2  ( 0 );
    }
#endif

#if !JVET_P1006_PICTURE_HEADER
    bool isSAOEnabled = pcSlice->getSPS()->getSAOEnabledFlag() && (pcSlice->getSaoEnabledFlag(CHANNEL_TYPE_LUMA) || (chromaEnabled && pcSlice->getSaoEnabledFlag(CHANNEL_TYPE_CHROMA)));
    bool isDBFEnabled = (!pcSlice->getDeblockingFilterDisable());

    if(pcSlice->getPPS()->getLoopFilterAcrossSlicesEnabledFlag() && ( isSAOEnabled || isDBFEnabled ))
    {
      WRITE_FLAG(pcSlice->getLFCrossSliceBoundaryFlag()?1:0, "slice_loop_filter_across_slices_enabled_flag");
    }

    if (pcSlice->getSPS()->getUseReshaper())
    {
      WRITE_FLAG( pcSlice->getLmcsEnabledFlag()? 1 : 0, "slice_lmcs_enabled_flag");
      if (pcSlice->getLmcsEnabledFlag())
      {
        WRITE_CODE(pcSlice->getLmcsAPSId(), 2, "slice_lmcs_aps_id");
          if (chromaEnabled)
          {
            WRITE_FLAG(pcSlice->getLmcsChromaResidualScaleFlag(), "slice_chroma_residual_scale_flag");
          }
      }
    }

    if( pcSlice->getSPS()->getScalingListFlag() )
    {
      WRITE_FLAG( pcSlice->getscalingListPresentFlag() ? 1 : 0, "slice_scaling_list_present_flag" );
      if( pcSlice->getscalingListPresentFlag() )
      {
        WRITE_CODE( pcSlice->getscalingListAPSId(), 3, "slice_scaling_list_aps_id" );
      }
    }
#endif
  if(pcSlice->getPPS()->getSliceHeaderExtensionPresentFlag())
  {
    WRITE_UVLC(0,"slice_segment_header_extension_length");
  }

}

void  HLSWriter::codeConstraintInfo  ( const ConstraintInfo* cinfo )
{
  WRITE_FLAG(cinfo->getProgressiveSourceFlag(),   "general_progressive_source_flag"         );
  WRITE_FLAG(cinfo->getInterlacedSourceFlag(),    "general_interlaced_source_flag"          );
  WRITE_FLAG(cinfo->getNonPackedConstraintFlag(), "general_non_packed_constraint_flag"      );
  WRITE_FLAG(cinfo->getFrameOnlyConstraintFlag(), "general_frame_only_constraint_flag"      );
  WRITE_FLAG(cinfo->getIntraOnlyConstraintFlag(),     "intra_only_constraint_flag"      );

  WRITE_CODE(cinfo->getMaxBitDepthConstraintIdc(), 4, "max_bitdepth_constraint_idc" );
  WRITE_CODE(cinfo->getMaxChromaFormatConstraintIdc(), 2, "max_chroma_format_constraint_idc" );

  WRITE_FLAG(cinfo->getNoQtbttDualTreeIntraConstraintFlag() ? 1 : 0, "no_qtbtt_dual_tree_intra_constraint_flag");
  WRITE_FLAG(cinfo->getNoPartitionConstraintsOverrideConstraintFlag() ? 1 : 0, "no_partition_constraints_override_constraint_flag");
  WRITE_FLAG(cinfo->getNoSaoConstraintFlag() ? 1 : 0, "no_sao_constraint_flag");
  WRITE_FLAG(cinfo->getNoAlfConstraintFlag() ? 1 : 0, "no_alf_constraint_flag");
  WRITE_FLAG(cinfo->getNoJointCbCrConstraintFlag() ? 1 : 0, "no_joint_cbcr_constraint_flag");
  WRITE_FLAG(cinfo->getNoRefWraparoundConstraintFlag() ? 1 : 0, "no_ref_wraparound_constraint_flag");
  WRITE_FLAG(cinfo->getNoTemporalMvpConstraintFlag() ? 1 : 0, "no_temporal_mvp_constraint_flag");
  WRITE_FLAG(cinfo->getNoSbtmvpConstraintFlag() ? 1 : 0, "no_sbtmvp_constraint_flag");
  WRITE_FLAG(cinfo->getNoAmvrConstraintFlag() ? 1 : 0, "no_amvr_constraint_flag");
  WRITE_FLAG(cinfo->getNoBdofConstraintFlag() ? 1 : 0, "no_bdof_constraint_flag");
  WRITE_FLAG(cinfo->getNoDmvrConstraintFlag() ? 1 : 0, "no_dmvr_constraint_flag");
  WRITE_FLAG(cinfo->getNoCclmConstraintFlag() ? 1 : 0, "no_cclm_constraint_flag");
  WRITE_FLAG(cinfo->getNoMtsConstraintFlag() ? 1 : 0, "no_mts_constraint_flag");
  WRITE_FLAG(cinfo->getNoSbtConstraintFlag() ? 1 : 0, "no_sbt_constraint_flag");
  WRITE_FLAG(cinfo->getNoAffineMotionConstraintFlag() ? 1 : 0, "no_affine_motion_constraint_flag");
  WRITE_FLAG(cinfo->getNoGbiConstraintFlag() ? 1 : 0, "no_gbi_constraint_flag");
  WRITE_FLAG(cinfo->getNoIbcConstraintFlag() ? 1 : 0, "no_ibc_constraint_flag");
  WRITE_FLAG(cinfo->getNoMhIntraConstraintFlag() ? 1 : 0, "no_mh_intra_constraint_flag");
  WRITE_FLAG(cinfo->getNoFPelMmvdConstraintFlag() ? 1 : 0, "no_fpel_mmvd_constraint_flag");
  WRITE_FLAG(cinfo->getNoTriangleConstraintFlag() ? 1 : 0, "no_triangle_constraint_flag");
  WRITE_FLAG(cinfo->getNoLadfConstraintFlag() ? 1 : 0, "no_ladf_constraint_flag");
  WRITE_FLAG(cinfo->getNoTransformSkipConstraintFlag() ? 1 : 0, "no_transform_skip_constraint_flag");
  WRITE_FLAG(cinfo->getNoBDPCMConstraintFlag() ? 1 : 0, "no_bdpcm_constraint_flag");
  WRITE_FLAG(cinfo->getNoQpDeltaConstraintFlag() ? 1 : 0, "no_qp_delta_constraint_flag");
  WRITE_FLAG(cinfo->getNoDepQuantConstraintFlag() ? 1 : 0, "no_dep_quant_constraint_flag");
  WRITE_FLAG(cinfo->getNoSignDataHidingConstraintFlag() ? 1 : 0, "no_sign_data_hiding_constraint_flag");
#if JVET_P0366_NUT_CONSTRAINT_FLAGS
  WRITE_FLAG(cinfo->getNoTrailConstraintFlag() ? 1 : 0, "no_trail_constraint_flag");
  WRITE_FLAG(cinfo->getNoStsaConstraintFlag() ? 1 : 0, "no_stsa_constraint_flag");
  WRITE_FLAG(cinfo->getNoRaslConstraintFlag() ? 1 : 0, "no_rasl_constraint_flag");
  WRITE_FLAG(cinfo->getNoRadlConstraintFlag() ? 1 : 0, "no_radl_constraint_flag");
  WRITE_FLAG(cinfo->getNoIdrConstraintFlag() ? 1 : 0, "no_idr_constraint_flag");
  WRITE_FLAG(cinfo->getNoCraConstraintFlag() ? 1 : 0, "no_cra_constraint_flag");
  WRITE_FLAG(cinfo->getNoGdrConstraintFlag() ? 1 : 0, "no_gdr_constraint_flag");
  WRITE_FLAG(cinfo->getNoApsConstraintFlag() ? 1 : 0, "no_aps_constraint_flag");
#endif
}


void  HLSWriter::codeProfileTierLevel    ( const ProfileTierLevel* ptl, int maxNumSubLayersMinus1 )
{
  WRITE_CODE( int(ptl->getProfileIdc()), 7 ,   "general_profile_idc"                     );
  WRITE_FLAG( ptl->getTierFlag()==Level::HIGH, "general_tier_flag"                       );

#if JVET_P0217_PTL_SYNTAX_CLEANUP
  codeConstraintInfo( ptl->getConstraintInfo() );

  WRITE_CODE( int( ptl->getLevelIdc() ), 8, "general_level_idc" );
#endif

  WRITE_CODE(ptl->getNumSubProfile(), 8, "num_sub_profiles");
  for (int i = 0; i < ptl->getNumSubProfile(); i++)
  {
    WRITE_CODE(ptl->getSubProfileIdc(i) , 32, "general_sub_profile_idc[i]");
  }

#if !JVET_P0217_PTL_SYNTAX_CLEANUP
  codeConstraintInfo(ptl->getConstraintInfo());

  WRITE_CODE( int(ptl->getLevelIdc()), 8 ,     "general_level_idc"                     );
#endif

  for (int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    WRITE_FLAG( ptl->getSubLayerLevelPresentFlag(i),   "sub_layer_level_present_flag[i]" );
  }

  while (!isByteAligned())
  {
    WRITE_FLAG(0, "ptl_alignment_zero_bit");
  }

  for(int i = 0; i < maxNumSubLayersMinus1; i++)
  {
    if( ptl->getSubLayerLevelPresentFlag(i) )
    {
      WRITE_CODE( int(ptl->getSubLayerLevelIdc(i)), 8, "sub_layer_level_idc[i]" );
    }
  }

}


/**
* Write tiles and wavefront substreams sizes for the slice header (entry points).
*
* \param pSlice Slice structure that contains the substream size information.
*/
void  HLSWriter::codeTilesWPPEntryPoint( Slice* pSlice )
{
  if (pSlice->getPPS()->getSingleTileInPicFlag() && !pSlice->getPPS()->getEntropyCodingSyncEnabledFlag())
  {
    return;
  }
  uint32_t maxOffset = 0;
  for(int idx=0; idx<pSlice->getNumberOfSubstreamSizes(); idx++)
  {
    uint32_t offset=pSlice->getSubstreamSize(idx);
    if ( offset > maxOffset )
    {
      maxOffset = offset;
    }
  }

  // Determine number of bits "offsetLenMinus1+1" required for entry point information
  uint32_t offsetLenMinus1 = 0;
  while (maxOffset >= (1u << (offsetLenMinus1 + 1)))
  {
    offsetLenMinus1++;
    CHECK(offsetLenMinus1 + 1 >= 32, "Invalid offset length minus 1");
  }

#if !JVET_O0145_ENTRYPOINT_SIGNALLING
  WRITE_UVLC(pSlice->getNumberOfSubstreamSizes(), "num_entry_point_offsets");
#endif
  if (pSlice->getNumberOfSubstreamSizes()>0)
  {
    WRITE_UVLC(offsetLenMinus1, "offset_len_minus1");

    for (uint32_t idx=0; idx<pSlice->getNumberOfSubstreamSizes(); idx++)
    {
      WRITE_CODE(pSlice->getSubstreamSize(idx)-1, offsetLenMinus1+1, "entry_point_offset_minus1");
    }
  }
}


// ====================================================================================================================
// Protected member functions
// ====================================================================================================================

//! Code weighted prediction tables
void HLSWriter::xCodePredWeightTable( Slice* pcSlice )
{
  WPScalingParam  *wp;
  const ChromaFormat    format                = pcSlice->getSPS()->getChromaFormatIdc();
  const uint32_t            numberValidComponents = getNumberValidComponents(format);
  const bool            bChroma               = isChromaEnabled(format);
  const int             iNbRef                = (pcSlice->getSliceType() == B_SLICE ) ? (2) : (1);
  bool            bDenomCoded           = false;
  uint32_t            uiTotalSignalledWeightFlags = 0;

  if ( (pcSlice->getSliceType()==P_SLICE && pcSlice->getPPS()->getUseWP()) || (pcSlice->getSliceType()==B_SLICE && pcSlice->getPPS()->getWPBiPred()) )
  {
    for ( int iNumRef=0 ; iNumRef<iNbRef ; iNumRef++ ) // loop over l0 and l1 syntax elements
    {
      RefPicList  eRefPicList = ( iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );

      // NOTE: wp[].uiLog2WeightDenom and wp[].bPresentFlag are actually per-channel-type settings.

      for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
        if ( !bDenomCoded )
        {
          int iDeltaDenom;
          WRITE_UVLC( wp[COMPONENT_Y].uiLog2WeightDenom, "luma_log2_weight_denom" );

          if( bChroma )
          {
            CHECK( wp[COMPONENT_Cb].uiLog2WeightDenom != wp[COMPONENT_Cr].uiLog2WeightDenom, "Chroma blocks of different size not supported" );
            iDeltaDenom = (wp[COMPONENT_Cb].uiLog2WeightDenom - wp[COMPONENT_Y].uiLog2WeightDenom);
            WRITE_SVLC( iDeltaDenom, "delta_chroma_log2_weight_denom" );
          }
          bDenomCoded = true;
        }
        WRITE_FLAG( wp[COMPONENT_Y].bPresentFlag, iNumRef==0?"luma_weight_l0_flag[i]":"luma_weight_l1_flag[i]" );
        uiTotalSignalledWeightFlags += wp[COMPONENT_Y].bPresentFlag;
      }
      if (bChroma)
      {
        for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
        {
          pcSlice->getWpScaling( eRefPicList, iRefIdx, wp );
          CHECK( wp[COMPONENT_Cb].bPresentFlag != wp[COMPONENT_Cr].bPresentFlag, "Inconsistent settings for chroma channels" );
          WRITE_FLAG( wp[COMPONENT_Cb].bPresentFlag, iNumRef==0?"chroma_weight_l0_flag[i]":"chroma_weight_l1_flag[i]" );
          uiTotalSignalledWeightFlags += 2*wp[COMPONENT_Cb].bPresentFlag;
        }
      }

      for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
      {
        pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
        if ( wp[COMPONENT_Y].bPresentFlag )
        {
          int iDeltaWeight = (wp[COMPONENT_Y].iWeight - (1<<wp[COMPONENT_Y].uiLog2WeightDenom));
          WRITE_SVLC( iDeltaWeight, iNumRef==0?"delta_luma_weight_l0[i]":"delta_luma_weight_l1[i]" );
          WRITE_SVLC( wp[COMPONENT_Y].iOffset, iNumRef==0?"luma_offset_l0[i]":"luma_offset_l1[i]" );
        }

        if ( bChroma )
        {
          if ( wp[COMPONENT_Cb].bPresentFlag )
          {
            for ( int j = COMPONENT_Cb ; j < numberValidComponents ; j++ )
            {
              CHECK(wp[COMPONENT_Cb].uiLog2WeightDenom != wp[COMPONENT_Cr].uiLog2WeightDenom, "Chroma blocks of different size not supported");
              int iDeltaWeight = (wp[j].iWeight - (1<<wp[COMPONENT_Cb].uiLog2WeightDenom));
              WRITE_SVLC( iDeltaWeight, iNumRef==0?"delta_chroma_weight_l0[i]":"delta_chroma_weight_l1[i]" );

              int range=pcSlice->getSPS()->getSpsRangeExtension().getHighPrecisionOffsetsEnabledFlag() ? (1<<pcSlice->getSPS()->getBitDepth(CHANNEL_TYPE_CHROMA))/2 : 128;
              int pred = ( range - ( ( range*wp[j].iWeight)>>(wp[j].uiLog2WeightDenom) ) );
              int iDeltaChroma = (wp[j].iOffset - pred);
              WRITE_SVLC( iDeltaChroma, iNumRef==0?"delta_chroma_offset_l0[i]":"delta_chroma_offset_l1[i]" );
            }
          }
        }
      }
    }
    CHECK(uiTotalSignalledWeightFlags>24, "Too many signalled weight flags");
  }
}

/** code quantization matrix
*  \param scalingList quantization matrix information
*/
void HLSWriter::codeScalingList( const ScalingList &scalingList )
{
  //for each size
#if JVET_P0365_SCALING_MATRIX_LFNST
  WRITE_FLAG(scalingList.getDisableScalingMatrixForLfnstBlks(), "scaling_matrix_for_lfnst_disabled_flag"); 
#endif
#if JVET_P01034_PRED_1D_SCALING_LIST
  for (uint32_t scalingListId = 0; scalingListId < 28; scalingListId++)
  {
    bool scalingListCopyModeFlag = scalingList.getScalingListCopyModeFlag(scalingListId);
    WRITE_FLAG(scalingListCopyModeFlag, "scaling_list_copy_mode_flag"); //copy mode
    if (!scalingListCopyModeFlag)// Copy Mode
    {
      WRITE_FLAG(scalingList.getScalingListPreditorModeFlag(scalingListId), "scaling_list_predictor_mode_flag");
    }
    if ((scalingListCopyModeFlag || scalingList.getScalingListPreditorModeFlag(scalingListId)) && scalingListId!= SCALING_LIST_1D_START_2x2 && scalingListId != SCALING_LIST_1D_START_4x4 && scalingListId != SCALING_LIST_1D_START_8x8)
    {
      WRITE_UVLC((int)scalingListId - (int)scalingList.getRefMatrixId(scalingListId), "scaling_list_pred_matrix_id_delta");
    }
    if (!scalingListCopyModeFlag)
    {
      //DPCM
      xCodeScalingList(&scalingList, scalingListId, scalingList.getScalingListPreditorModeFlag(scalingListId));
    }
#else
  for(uint32_t sizeId = SCALING_LIST_FIRST_CODED; sizeId <= SCALING_LIST_LAST_CODED; sizeId++)
  {
    const int predListStep = (sizeId > SCALING_LIST_32x32 ? (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) : 1); // if 64x64, skip over chroma entries.

    for(uint32_t listId = 0; listId < SCALING_LIST_NUM; listId+=predListStep)
    {
      if ((sizeId == SCALING_LIST_2x2) && ((listId % (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES) == 0)))
      {
        continue;
      }
      bool scalingListPredModeFlag = scalingList.getScalingListPredModeFlag(sizeId, listId);
      WRITE_FLAG( scalingListPredModeFlag, "scaling_list_pred_mode_flag" );
      if(!scalingListPredModeFlag)// Copy Mode
      {
        if (sizeId > SCALING_LIST_32x32) //64x64 luma
        {
          // adjust the code, to cope with the missing chroma entries
          WRITE_UVLC( ((int)listId - (int)scalingList.getRefMatrixId(sizeId, listId)) / (SCALING_LIST_NUM / SCALING_LIST_PRED_MODES), "scaling_list_pred_matrix_id_delta");
        }
        else
        {
          WRITE_UVLC( (int)listId - (int)scalingList.getRefMatrixId (sizeId,listId), "scaling_list_pred_matrix_id_delta");
        }
      }
      else// DPCM Mode
      {
        xCodeScalingList(&scalingList, sizeId, listId);
      }
    }
#endif
  }
  return;
}
/** code DPCM
* \param scalingList quantization matrix information
* \param sizeId      size index
* \param listId      list index
*/
#if JVET_P01034_PRED_1D_SCALING_LIST
void HLSWriter::xCodeScalingList(const ScalingList* scalingList, uint32_t scalingListId, bool isPredictor)
#else
void HLSWriter::xCodeScalingList(const ScalingList* scalingList, uint32_t sizeId, uint32_t listId)
#endif
{
#if JVET_P01034_PRED_1D_SCALING_LIST
  int matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : ((scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8);
  int coefNum = matrixSize * matrixSize;
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][SCAN_DIAG][gp_sizeIdxInfo->idxFrom(matrixSize)][gp_sizeIdxInfo->idxFrom(matrixSize)];
  int nextCoef = (isPredictor) ? 0 : SCALING_LIST_START_VALUE;
#else
  int coefNum = std::min( MAX_MATRIX_COEF_NUM, ( int ) g_scalingListSize[sizeId] );
  ScanElement *scan = g_scanOrder[SCAN_UNGROUPED][SCAN_DIAG][gp_sizeIdxInfo->idxFrom(1 << (sizeId == SCALING_LIST_2x2 ? 1 : (sizeId == SCALING_LIST_4x4 ? 2 : 3)))][gp_sizeIdxInfo->idxFrom(1 << (sizeId == SCALING_LIST_2x2 ? 1 : (sizeId == SCALING_LIST_4x4 ? 2 : 3)))];
  int nextCoef = SCALING_LIST_START_VALUE;
#endif

  int data;
#if JVET_P01034_PRED_1D_SCALING_LIST
  const int *src = scalingList->getScalingListAddress(scalingListId);
  int PredListId = scalingList->getRefMatrixId(scalingListId);
  const int *srcPred = (isPredictor) ? ((scalingListId==PredListId) ? scalingList->getScalingListDefaultAddress(scalingListId) : scalingList->getScalingListAddress(PredListId)) : NULL;
  int deltasrc[65] = { 0 };

  if (isPredictor)
  {
    if (scalingListId >= SCALING_LIST_1D_START_16x16)
    {
      deltasrc[64] = scalingList->getScalingListDC(scalingListId) - ((PredListId >= SCALING_LIST_1D_START_16x16) ? ((scalingListId == PredListId) ? 16 : scalingList->getScalingListDC(PredListId)) : srcPred[scan[0].idx]);
    }
    for (int i = 0; i < coefNum; i++)
    {
      deltasrc[i] = (src[scan[i].idx] - srcPred[scan[i].idx]);
    }
  }
  if (scalingListId >= SCALING_LIST_1D_START_16x16)
  {
    if (isPredictor)
    {
      data = deltasrc[64];
      nextCoef = deltasrc[64];
    }
    else
    {
      data = scalingList->getScalingListDC(scalingListId) - nextCoef;
      nextCoef = scalingList->getScalingListDC(scalingListId);
    }
    WRITE_SVLC((int8_t)data, "scaling_list_dc_coef");
#else
  const int *src = scalingList->getScalingListAddress(sizeId, listId);
  if( sizeId > SCALING_LIST_8x8 )
  {
    WRITE_SVLC( scalingList->getScalingListDC(sizeId,listId) - 8, "scaling_list_dc_coef_minus8");
    nextCoef = scalingList->getScalingListDC(sizeId,listId);
#endif
  }
  for(int i=0;i<coefNum;i++)
  {
#if JVET_P01034_PRED_1D_SCALING_LIST
    if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
#else
    if (sizeId == SCALING_LIST_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
#endif
      continue;
#if JVET_P01034_PRED_1D_SCALING_LIST
    data = (isPredictor) ? (deltasrc[i] - nextCoef) : (src[scan[i].idx] - nextCoef);
    nextCoef = (isPredictor) ? deltasrc[i] : src[scan[i].idx];
    WRITE_SVLC((int8_t)data, "scaling_list_delta_coef");
#else
    data = src[scan[i].idx] - nextCoef;
    nextCoef = src[scan[i].idx];
    if (data > 127)
    {
      data = data - 256;
    }
    if (data < -128)
    {
      data = data + 256;
    }

    WRITE_SVLC( data,  "scaling_list_delta_coef");
#endif
  }
}

bool HLSWriter::xFindMatchingLTRP(Slice* pcSlice, uint32_t *ltrpsIndex, int ltrpPOC, bool usedFlag)
{
  // bool state = true, state2 = false;
  int lsb = ltrpPOC & ((1<<pcSlice->getSPS()->getBitsForPOC())-1);
  for (int k = 0; k < pcSlice->getSPS()->getNumLongTermRefPicSPS(); k++)
  {
    if ( (lsb == pcSlice->getSPS()->getLtRefPicPocLsbSps(k)) && (usedFlag == pcSlice->getSPS()->getUsedByCurrPicLtSPSFlag(k)) )
    {
      *ltrpsIndex = k;
      return true;
    }
  }
  return false;
}

void HLSWriter::alfGolombEncode( int coeff, int k, const bool signed_coeff )
{
  unsigned int symbol = abs( coeff );
  while ( symbol >= (unsigned int)( 1 << k ) )
  {
    symbol -= 1 << k;
    k++;
    WRITE_FLAG( 0, "alf_coeff_abs_prefix" );
  }
  WRITE_FLAG( 1, "alf_coeff_abs_prefix" );

  if ( k > 0 )
  {
    WRITE_CODE( symbol, k, "alf_coeff_abs_suffix" );
  }
  if ( signed_coeff && coeff != 0 )
  {
    WRITE_FLAG( (coeff < 0) ? 1 : 0, "alf_coeff_sign" );
  }
}

void HLSWriter::alfFilter( const AlfParam& alfParam, const bool isChroma, const int altIdx )
{
#if !JVET_P0164_ALF_SYNTAX_SIMP
  if( !isChroma )
  {
    WRITE_FLAG( alfParam.alfLumaCoeffDeltaFlag, "alf_luma_coeff_delta_flag" );
  }
#endif
  AlfFilterShape alfShape(isChroma ? 5 : 7);
  const short* coeff = isChroma ? alfParam.chromaCoeff[altIdx] : alfParam.lumaCoeff;
  const short* clipp = isChroma ? alfParam.chromaClipp[altIdx] : alfParam.lumaClipp;
  const int numFilters = isChroma ? 1 : alfParam.numLumaFilters;

  // vlc for all
#if !JVET_P0164_ALF_SYNTAX_SIMP
  if( !isChroma )
  {
    if( alfParam.alfLumaCoeffDeltaFlag )
    {
      for( int ind = 0; ind < numFilters; ++ind )
      {
        WRITE_FLAG( alfParam.alfLumaCoeffFlag[ind], "alf_luma_coeff_flag[i]" );
      }
    }
  }
#endif

  // Filter coefficients
  for( int ind = 0; ind < numFilters; ++ind )
  {
#if !JVET_P0164_ALF_SYNTAX_SIMP
    if( !isChroma && !alfParam.alfLumaCoeffFlag[ind] && alfParam.alfLumaCoeffDeltaFlag )
    {
      continue;
    }
#endif

    for( int i = 0; i < alfShape.numCoeff - 1; i++ )
    {
      alfGolombEncode( coeff[ind* MAX_NUM_ALF_LUMA_COEFF + i], 3 );  // alf_coeff_chroma[i], alf_coeff_luma_delta[i][j]
    }
  }

  // Clipping values coding
  if( alfParam.nonLinearFlag[isChroma][altIdx] )
  {
    for (int ind = 0; ind < numFilters; ++ind)
    {
      for (int i = 0; i < alfShape.numCoeff - 1; i++)
      {
        WRITE_CODE(clipp[ind* MAX_NUM_ALF_LUMA_COEFF + i], 2, "alf_clipping_index");
      }
    }
  }
}


//! \}
