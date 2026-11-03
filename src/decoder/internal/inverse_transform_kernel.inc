static Av2hStatus AV2H_KERNEL_NAME(inverse_transform_samples, inverse_transform_samples_u16)(
    Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode, const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor, size_t predictor_stride_samples,
    AV2H_KERNEL_TYPE(Av2hReconstructionScratch, Av2hReconstructionScratchU16) * scratch,
    AV2H_KERNEL_TYPE(Av2hBlockReconstruction, Av2hBlockReconstructionU16) * reconstruction,
    const Av2hInverseTransformPlan *plan, uint8_t first_stage, const Av2hSampleDomain *domain) {
    uint16_t row;
    uint16_t column;
    size_t index;
    uint8_t rectangular = (uint8_t)((av2h_sample_block_log2(plan->output_width) ^
                                     av2h_sample_block_log2(plan->output_height)) &
                                    1U);
    Av2hStatus status;
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics, first_stage);
    status =
        av2h_inverse_secondary_transform(mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK)
        return status;
    av2h_reconstruction_diagnostics_set_inverse_transform_stage(diagnostics,
                                                                (uint8_t)(first_stage + 1U));
    for (row = 0U; row < plan->coeff_height; ++row) {
        int32_t row_input[32U];
        int32_t row_output[32U];
        for (column = 0U; column < plan->coeff_width; ++column) {
            int32_t value = scratch->scaled[(size_t)row * plan->coeff_width + column];
            row_input[column] =
                rectangular ? av2h_sample_round_pow2((int64_t)value * 2896, 12U) : value;
        }
        recon_transform_1d(row_input, row_output, plan->row_kind, plan->coeff_width,
                           plan->row_shift, 0U, domain->bit_depth);
        for (column = 0U; column < plan->coeff_width; ++column) {
            scratch->transposed[(size_t)row * plan->coeff_width + column] = row_output[column];
        }
    }
    for (column = 0U; column < plan->coeff_width; ++column) {
        int32_t column_input[32U];
        int32_t column_output[32U];
        for (row = 0U; row < plan->coeff_height; ++row) {
            column_input[row] = scratch->transposed[(size_t)row * plan->coeff_width + column];
        }
        recon_transform_1d(column_input, column_output, plan->column_kind, plan->coeff_height,
                           plan->column_shift, 1U, domain->bit_depth);
        for (row = 0U; row < plan->coeff_height; ++row) {
            scratch->residual[(size_t)row * plan->coeff_width + column] = column_output[row];
        }
    }
    for (row = 0U; row < plan->output_height; ++row) {
        uint16_t source_row =
            plan->coeff_height != plan->output_height ? (uint16_t)(row >> 1U) : row;
        for (column = 0U; column < plan->output_width; ++column) {
            uint16_t source_column =
                plan->coeff_width != plan->output_width ? (uint16_t)(column >> 1U) : column;
            int32_t residual =
                scratch->residual[(size_t)source_row * plan->coeff_width + source_column];
            size_t output = (size_t)row * plan->output_width + column;
            int32_t sample = predictor == NULL
                                 ? domain->midpoint
                                 : predictor[(size_t)row * predictor_stride_samples + column];
            reconstruction->residual[output] = (AV2H_KERNEL_TYPE(int16_t, int32_t))residual;
            reconstruction->pixels[output] =
                (AV2H_KERNEL_TYPE(uint8_t, uint16_t))av2h_sample_clip_to_domain(sample + residual,
                                                                                domain);
        }
    }
    return AV2H_OK;
}