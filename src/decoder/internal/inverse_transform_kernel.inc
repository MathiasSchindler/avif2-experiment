Av2hStatus AV2H_KERNEL_NAME(av2h_reconstruct_transform_generic,
                            av2h_reconstruct_transform_generic_u16)(
    Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode, const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    AV2H_KERNEL_TYPE(Av2hReconstructionScratch, Av2hReconstructionScratchU16) * scratch,
    AV2H_KERNEL_TYPE(Av2hBlockReconstruction, Av2hBlockReconstructionU16) * reconstruction
#if AV2H_KERNEL_WIDE
    ,
    const Av2hSampleDomain *domain
#endif
) {
#if AV2H_KERNEL_WIDE
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
#endif
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    uint16_t coeff_width;
    uint16_t coeff_height;
    uint8_t rectangular;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    size_t index;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 1U);
#if AV2H_KERNEL_WIDE
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL ||
        recon_geometry(prefix->tx_size, &output_width, &output_height, &row_shift, &column_shift) !=
            AV2H_OK) {
        return AV2H_INVALID_ARGUMENT;
#else
    if (recon_geometry(prefix->tx_size, &output_width, &output_height, &row_shift, &column_shift) !=
        AV2H_OK) {
        return AV2H_UNSUPPORTED;
#endif
    }
    coeff_width = output_width > 32U ? 32U : output_width;
    coeff_height = output_height > 32U ? 32U : output_height;
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 10U);
#else
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 2U);
#endif
    if (coefficients->width != coeff_width || coefficients->height != coeff_height ||
        coefficients->coefficient_count != (uint32_t)coeff_width * coeff_height) {
        return AV2H_UNSUPPORTED;
    }
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 11U);
    if (predictor_stride_samples < output_width ||
        (size_t)output_width * output_height > AV2H_RECON_SAMPLE_CAPACITY) {
        return AV2H_UNSUPPORTED;
#else
    if (predictor != NULL && predictor_stride_samples < output_width) {
        return AV2H_INVALID_ARGUMENT;
#endif
    }
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 12U);
    if (mode->skip_transform != 0U &&
        !(prefix->all_zero && prefix->eob == 0U && coefficients->nonzero_count == 0U)) {
        return AV2H_UNSUPPORTED;
#else
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        (transform->primary_type == AV2H_TX_DCT_DCT || transform->primary_type == AV2H_TX_IDTX)) {
        return av2h_reconstruct_lossless_4x4(mode, transform, coefficients, predictor,
                                             predictor_stride_samples, reconstruction);
#endif
    }
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 13U);
    reconstruction->width = output_width;
    reconstruction->height = output_height;
#endif
    if (prefix->all_zero && prefix->eob == 0U && coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
#if AV2H_KERNEL_WIDE
                return AV2H_INVALID_DATA;
#else
                return AV2H_UNSUPPORTED;
#endif
            }
        }
#if !AV2H_KERNEL_WIDE
        reconstruction->width = output_width;
        reconstruction->height = output_height;
#endif
        for (row = 0U; row < output_height; ++row) {
            for (column = 0U; column < output_width; ++column) {
#if AV2H_KERNEL_WIDE
                index = (size_t)row * output_width + column;
                reconstruction->residual[index] = 0;
                reconstruction->pixels[index] =
                    predictor[(size_t)row * predictor_stride_samples + column];
#else
                size_t out_index = (size_t)row * output_width + column;
                int32_t sample = predictor == NULL
                                     ? domain->midpoint
                                     : predictor[(size_t)row * predictor_stride_samples + column];
                reconstruction->residual[out_index] = 0;
                reconstruction->pixels[out_index] = (uint8_t)sample;
#endif
            }
        }
        return AV2H_OK;
    }
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 2U);
#else
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 3U);
#endif
    if (recon_transform_kinds(transform->primary_type, &row_kind, &column_kind) != AV2H_OK ||
        recon_kind_supported(row_kind, coeff_width) != AV2H_OK ||
        recon_kind_supported(column_kind, coeff_height) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
#if !AV2H_KERNEL_WIDE
    if (transform->primary_type == AV2H_TX_IDTX && prefix->tx_size == RECON_TX_32X4) {
        row_shift = 7U;
        column_shift = 11U;
    }
    reconstruction->width = output_width;
    reconstruction->height = output_height;
#endif
    rectangular =
        (uint8_t)((av2h_sample_block_log2(output_width) ^ av2h_sample_block_log2(output_height)) &
                  1U);
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 3U);
#else
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 4U);
#endif
    status =
        av2h_inverse_secondary_transform(mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK)
        return status;
#if AV2H_KERNEL_WIDE
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 4U);
#else
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, 5U);
#endif
    for (row = 0U; row < coeff_height; ++row) {
        int32_t row_input[32U];
        int32_t row_output[32U];
        for (column = 0U; column < coeff_width; ++column) {
            int32_t value = scratch->scaled[(size_t)row * coeff_width + column];
            row_input[column] =
                rectangular ? av2h_sample_round_pow2((int64_t)value * 2896, 12U) : value;
        }
#if AV2H_KERNEL_WIDE
        recon_transform_1d(row_input, row_output, row_kind, coeff_width, row_shift, 0U,
                           domain->bit_depth);
#else
        recon_transform_1d(row_input, row_output, row_kind, coeff_width, row_shift, 0U, 8U);
#endif
        for (column = 0U; column < coeff_width; ++column) {
            scratch->transposed[(size_t)row * coeff_width + column] = row_output[column];
        }
    }
    for (column = 0U; column < coeff_width; ++column) {
        int32_t column_input[32U];
        int32_t column_output[32U];
        for (row = 0U; row < coeff_height; ++row) {
            column_input[row] = scratch->transposed[(size_t)row * coeff_width + column];
        }
#if AV2H_KERNEL_WIDE
        recon_transform_1d(column_input, column_output, column_kind, coeff_height, column_shift, 1U,
                           domain->bit_depth);
#else
        recon_transform_1d(column_input, column_output, column_kind, coeff_height, column_shift, 1U,
                           8U);
#endif
        for (row = 0U; row < coeff_height; ++row) {
            scratch->residual[(size_t)row * coeff_width + column] = column_output[row];
        }
    }
    for (row = 0U; row < output_height; ++row) {
        uint16_t source_row = coeff_height != output_height ? (uint16_t)(row >> 1U) : row;
        for (column = 0U; column < output_width; ++column) {
            uint16_t source_column =
                coeff_width != output_width ? (uint16_t)(column >> 1U) : column;
            int32_t residual = scratch->residual[(size_t)source_row * coeff_width + source_column];
#if AV2H_KERNEL_WIDE
            index = (size_t)row * output_width + column;
            reconstruction->residual[index] = residual;
            reconstruction->pixels[index] = av2h_sample_clip_to_domain(
                (int32_t)predictor[(size_t)row * predictor_stride_samples + column] + residual,
                domain);
#else
            size_t out_index = (size_t)row * output_width + column;
            int32_t sample = predictor == NULL
                                 ? domain->midpoint
                                 : predictor[(size_t)row * predictor_stride_samples + column];
            reconstruction->residual[out_index] = (int16_t)residual;
            reconstruction->pixels[out_index] =
                av2h_sample_clip_to_domain(sample + residual, domain);
#endif
        }
    }
    return AV2H_OK;
}