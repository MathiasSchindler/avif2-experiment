Av2hStatus AV2H_KERNEL_NAME(av2h_luma_reconstruct, av2h_luma_reconstruct_u16)(
    Av2hDecodeDiagnostics *diagnostics, const Av2hLeafMode *mode, const Av2hCoeffPrefix *prefix,
    const Av2hTransformType *transform, const Av2hCoeffBlock *coefficients,
    const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor,
#if !AV2H_KERNEL_WIDE
    size_t predictor_stride_samples,
#endif
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
    const Av2hTxDescriptor *descriptor;
#else
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    size_t index;
    size_t count;
    uint8_t first_shift;
    uint8_t second_shift;
    uint8_t rectangular;
    uint8_t identity_rows;
    uint8_t adst_rows;
    uint8_t flip_adst_rows;
    uint8_t identity_columns;
    uint8_t adst_columns;
    uint8_t flip_adst_columns = 0U;
#endif
    Av2hStatus status;
#if AV2H_KERNEL_WIDE
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL) {
#else
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        scratch == NULL || reconstruction == NULL) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
#if AV2H_KERNEL_WIDE
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 && mode->fsc_mode == 0U) {
        status = reconstruct_lossless_4x4_u16(coefficients, predictor, mode->use_dpcm_y,
                                              mode->dpcm_mode_y, 0U, 0U, reconstruction, domain);
    } else if (mode->use_dpcm_y != 0U && mode->fsc_mode != 0U) {
        uint16_t row;
        uint16_t column;
        if (coefficients->coefficient_count !=
                (size_t)descriptor->output_width * descriptor->output_height ||
            coefficients->width != descriptor->output_width ||
            coefficients->height != descriptor->output_height) {
#else
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4 &&
        (transform->primary_type == AV2H_TX_DCT_DCT || transform->primary_type == AV2H_TX_IDTX)) {
        return av2h_reconstruct_lossless_4x4(mode, transform, coefficients, predictor,
                                             predictor_stride_samples, reconstruction);
    }
    if (prefix->all_zero) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        Av2hStatus descriptor_status;
        if (prefix->eob != 0U || coefficients->nonzero_count != 0U || predictor == NULL) {
#endif
            return AV2H_UNSUPPORTED;
#if !AV2H_KERNEL_WIDE
        }
        descriptor_status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
        if (descriptor_status != AV2H_OK)
            return descriptor_status;
        if (predictor_stride_samples < descriptor->output_width ||
            coefficients->coefficient_count != coefficients->width * coefficients->height) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        output_count = (size_t)descriptor->output_width * descriptor->output_height;
        if (output_count > AV2H_RECON_SAMPLE_CAPACITY) {
            return AV2H_LIMIT_EXCEEDED;
#endif
        }
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
#if !AV2H_KERNEL_WIDE
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
        }
        return AV2H_OK;
    }
    if (prefix->q_index == 0U && transform->primary_type == AV2H_TX_IDTX &&
        mode->use_dpcm_y != 0U) {
        const Av2hTxDescriptor *descriptor;
        size_t width;
        size_t height;
        unsigned int scale_bits;
        size_t row;
        size_t column;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK) {
            return AV2H_UNSUPPORTED;
        }
        width = descriptor->output_width;
        height = descriptor->output_height;
        if (coefficients->width != width || coefficients->height != height ||
            coefficients->coefficient_count != width * height || predictor == NULL ||
            predictor_stride_samples < width) {
            return AV2H_UNSUPPORTED;
        }
        scale_bits = 3U - (coefficients->coefficient_count > 256U ? 1U : 0U) -
                     (coefficients->coefficient_count > 1024U ? 1U : 0U);
        reconstruction->width = (uint16_t)width;
        reconstruction->height = (uint16_t)height;
#endif
        if (mode->dpcm_mode_y == 0U) {
#if AV2H_KERNEL_WIDE
            for (column = 0U; column < descriptor->output_width; ++column) {
#else
            for (column = 0U; column < width; ++column) {
#endif
                int32_t delay = 0;
#if AV2H_KERNEL_WIDE
                for (row = 0U; row < descriptor->output_height; ++row) {
                    size_t sample = (size_t)row * descriptor->output_width + column;
                    int32_t current = coefficients->dequantized[sample] / 8;
#else
                for (row = 0U; row < height; ++row) {
                    size_t sample = row * width + column;
                    int32_t current = coefficients->dequantized[sample] >> scale_bits;
#endif
                    int32_t residual = current + delay;
#if AV2H_KERNEL_WIDE
                    reconstruction->residual[sample] = residual;
                    reconstruction->pixels[sample] =
                        av2h_sample_clip_to_domain((int32_t)predictor[sample] + residual, domain);
#else
                    reconstruction->residual[sample] = (int16_t)residual;
                    reconstruction->pixels[sample] = av2h_sample_clip_to_domain(
                        (int32_t)predictor[row * predictor_stride_samples + column] + residual,
                        domain);
#endif
                    delay += current;
                }
            }
        } else {
#if AV2H_KERNEL_WIDE
            for (row = 0U; row < descriptor->output_height; ++row) {
#else
            for (row = 0U; row < height; ++row) {
#endif
                int32_t delay = 0;
#if AV2H_KERNEL_WIDE
                for (column = 0U; column < descriptor->output_width; ++column) {
                    size_t sample = (size_t)row * descriptor->output_width + column;
                    int32_t current = coefficients->dequantized[sample] / 8;
#else
                for (column = 0U; column < width; ++column) {
                    size_t sample = row * width + column;
                    int32_t current = coefficients->dequantized[sample] >> scale_bits;
#endif
                    int32_t residual = current + delay;
#if AV2H_KERNEL_WIDE
                    reconstruction->residual[sample] = residual;
                    reconstruction->pixels[sample] =
                        av2h_sample_clip_to_domain((int32_t)predictor[sample] + residual, domain);
#else
                    reconstruction->residual[sample] = (int16_t)residual;
                    reconstruction->pixels[sample] = av2h_sample_clip_to_domain(
                        (int32_t)predictor[row * predictor_stride_samples + column] + residual,
                        domain);
#endif
                    delay += current;
                }
            }
        }
#if AV2H_KERNEL_WIDE
        status = AV2H_OK;
    } else {
        av2h_reconstruction_diagnostics_set_luma_stage(diagnostics, 2U);
        status = av2h_reconstruct_transform_generic_u16(
            diagnostics, mode, prefix, transform, coefficients, predictor, descriptor->output_width,
            scratch, reconstruction, domain);
#else
        return AV2H_OK;
#endif
    }
#if AV2H_KERNEL_WIDE
    return status;
#else
    if (av2h_reconstruct_generic_selected(mode, transform, prefix->tx_size)) {
        return av2h_reconstruct_transform_generic(diagnostics, mode, prefix, transform,
                                                  coefficients, predictor, predictor_stride_samples,
                                                  scratch, reconstruction);
#endif
}
#if !AV2H_KERNEL_WIDE
if ((mode->mode != AV2H_INTRA_DC && mode->mode != AV2H_INTRA_VERTICAL &&
     mode->mode != AV2H_INTRA_HORIZONTAL && mode->mode != AV2H_INTRA_PAETH &&
     mode->mode != AV2H_INTRA_SMOOTH && mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
     mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL && mode->mode != AV2H_INTRA_D67 &&
     mode->mode != AV2H_INTRA_D113 && mode->mode != AV2H_INTRA_D135 &&
     mode->mode != AV2H_INTRA_D157 && mode->mode != AV2H_INTRA_D45 &&
     mode->mode != AV2H_INTRA_D203) ||
    coefficients->width == 0U || coefficients->height == 0U ||
    coefficients->coefficient_count != coefficients->width * coefficients->height) {
    return AV2H_UNSUPPORTED;
}
if (mode->use_intrabc && mode->skip_transform) {
    const Av2hTxDescriptor *descriptor;
    size_t output_count;
    if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK || descriptor == NULL ||
        mode->intrabc_mode > 1U || mode->mv_precision != 3U || !prefix->all_zero ||
        prefix->eob != 0U || transform->primary_type != AV2H_TX_DCT_DCT ||
        coefficients->width != descriptor->coefficient_width ||
        coefficients->height != descriptor->coefficient_height ||
        coefficients->coefficient_count != descriptor->eob_capacity ||
        coefficients->nonzero_count != 0U || predictor == NULL ||
        predictor_stride_samples < descriptor->output_width) {
        return AV2H_UNSUPPORTED;
    }
    output_count = (size_t)descriptor->output_width * descriptor->output_height;
    reconstruction->width = descriptor->output_width;
    reconstruction->height = descriptor->output_height;
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
            return AV2H_UNSUPPORTED;
        }
    }
    for (index = 0U; index < output_count; ++index) {
        size_t row = index / descriptor->output_width;
        size_t column = index % descriptor->output_width;
        reconstruction->residual[index] = 0;
        reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
    }
    return AV2H_OK;
}
if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 && mode->mrl_index == 0U &&
    mode->enable_ibp == 1U && mode->use_intrabc == 0U && mode->skip_transform == 0U &&
    mode->fsc_mode == 0U && prefix->tx_size == AV2H_TX_16X16 && prefix->all_zero &&
    prefix->eob == 0U && transform->primary_type == AV2H_TX_DCT_DCT && coefficients->width == 16U &&
    coefficients->height == 16U && coefficients->coefficient_count == 256U &&
    coefficients->nonzero_count == 0U && predictor != NULL && predictor_stride_samples >= 16U) {
    reconstruction->width = 16U;
    reconstruction->height = 16U;
    for (index = 0U; index < 256U; ++index) {
        size_t row = index >> 4U;
        size_t column = index & 15U;
        if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
            return AV2H_UNSUPPORTED;
        }
        reconstruction->residual[index] = 0;
        reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
    }
    return AV2H_OK;
}
if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 && mode->mrl_index == 0U &&
    mode->enable_ibp == 1U && mode->use_intrabc == 0U && mode->skip_transform == 0U &&
    mode->fsc_mode == 0U && prefix->tx_size == AV2H_TX_64X16 && prefix->all_zero &&
    prefix->eob == 0U && transform->primary_type == AV2H_TX_DCT_DCT && coefficients->width == 32U &&
    coefficients->height == 16U && coefficients->coefficient_count == 512U &&
    coefficients->nonzero_count == 0U && predictor != NULL && predictor_stride_samples >= 64U) {
    reconstruction->width = 64U;
    reconstruction->height = 16U;
    for (index = 0U; index < 512U; ++index) {
        if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
            return AV2H_UNSUPPORTED;
        }
    }
    for (index = 0U; index < 1024U; ++index) {
        size_t row = index >> 6U;
        size_t column = index & 63U;
        reconstruction->residual[index] = 0;
        reconstruction->pixels[index] = predictor[row * predictor_stride_samples + column];
    }
    return AV2H_OK;
}
if (mode->mode == AV2H_INTRA_SMOOTH &&
    !((prefix->tx_size == AV2H_TX_4X4 && (transform->primary_type == AV2H_TX_DCT_ADST ||
                                          transform->primary_type == AV2H_TX_DCT_DCT)) ||
      (prefix->tx_size == AV2H_TX_4X8 && (transform->primary_type == AV2H_TX_ADST_DCT ||
                                          transform->primary_type == AV2H_TX_ADST_ADST)) ||
      (prefix->tx_size == AV2H_TX_8X8 && (transform->primary_type == AV2H_TX_DCT_DCT ||
                                          transform->primary_type == AV2H_TX_ADST_ADST)) ||
      (prefix->tx_size == AV2H_TX_16X8 && (transform->primary_type == AV2H_TX_ADST_ADST ||
                                           transform->primary_type == AV2H_TX_DCT_DCT)) ||
      (prefix->tx_size == AV2H_TX_16X16 && transform->primary_type == AV2H_TX_ADST_ADST) ||
      (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT) ||
      (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_H_DCT))) {
    return AV2H_UNSUPPORTED;
}
if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if ((prefix->tx_size == AV2H_TX_4X8 || prefix->tx_size == AV2H_TX_8X4) &&
           transform->primary_type == AV2H_TX_H_ADST &&
           (mode->mode == AV2H_INTRA_HORIZONTAL ||
            (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL && prefix->tx_size == AV2H_TX_8X4))) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_D67 || mode->mode == AV2H_INTRA_VERTICAL ||
            mode->mode == AV2H_INTRA_D157)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_HORIZONTAL ||
            mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_SMOOTH ||
            mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL || mode->mode == AV2H_INTRA_D135)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_IDTX &&
           mode->fsc_mode) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_V_ADST &&
           mode->mode == AV2H_INTRA_VERTICAL) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_IDTX &&
           mode->fsc_mode) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
           mode->mode == AV2H_INTRA_SMOOTH) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_FLIPADST_ADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
    flip_adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_VERTICAL)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D67 ||
            mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_PAETH)) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X16 && transform->primary_type == AV2H_TX_DCT_ADST) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_DCT_ADST &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_SMOOTH)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_ADST_DCT) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_ADST_ADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_V_ADST &&
           mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_IDTX &&
           (mode->use_intrabc || mode->fsc_mode || mode->mode == AV2H_INTRA_D45 ||
            mode->mode == AV2H_INTRA_D67)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_H_ADST &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_HORIZONTAL)) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X4 && transform->primary_type == AV2H_TX_H_FLIPADST &&
           mode->use_intrabc) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X4 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
           mode->mode == AV2H_INTRA_D45) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_4X8 && transform->primary_type == AV2H_TX_V_DCT) {
    first_shift = 7U;
    second_shift = 10U;
    rectangular = 1U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_V_DCT) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
           mode->mode == AV2H_INTRA_D67) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
            mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D45)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_ADST_DCT) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_8X8 && transform->primary_type == AV2H_TX_DCT_ADST &&
           mode->mode == AV2H_INTRA_HORIZONTAL) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X16 && transform->primary_type == AV2H_TX_IDTX &&
           (mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_D135) && mode->fsc_mode) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D45 ||
            mode->mode == AV2H_INTRA_D113 || mode->mode == AV2H_INTRA_D67 ||
            mode->mode == AV2H_INTRA_D157 || mode->mode == AV2H_INTRA_SMOOTH ||
            mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_DC ||
            mode->mode == AV2H_INTRA_D67)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D113 ||
            mode->mode == AV2H_INTRA_D67)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_16X8 && transform->primary_type == AV2H_TX_ADST_FLIPADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_16X16 && transform->primary_type == AV2H_TX_ADST_ADST &&
           (mode->mode == AV2H_INTRA_SMOOTH || mode->mode == AV2H_INTRA_D157)) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_8X16 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_D67 ||
            mode->mode == AV2H_INTRA_DC)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_VERTICAL ||
            mode->mode == AV2H_INTRA_SMOOTH_VERTICAL || mode->mode == AV2H_INTRA_PAETH ||
            mode->mode == AV2H_INTRA_HORIZONTAL)) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_4X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_VERTICAL || mode->mode == AV2H_INTRA_DC)) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_8X32 && transform->primary_type == AV2H_TX_H_DCT &&
           (mode->mode == AV2H_INTRA_D203 || mode->mode == AV2H_INTRA_SMOOTH)) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
           mode->mode == AV2H_INTRA_DC) {
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        if (coefficients->dequantized[index] != 0 && index != 0U && index != 16U && index != 17U &&
            index != 32U) {
            return AV2H_UNSUPPORTED;
        }
    }
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_DCT &&
           mode->mode == AV2H_INTRA_SMOOTH) {
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        if (coefficients->dequantized[index] != 0 && index != 0U && index != 1U) {
            return AV2H_UNSUPPORTED;
        }
    }
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_FLIPADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 1U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X32 && transform->primary_type == AV2H_TX_DCT_ADST &&
           mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 1U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_D45 || mode->mode == AV2H_INTRA_D113)) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_H_ADST &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 1U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_ADST_DCT &&
           mode->mode == AV2H_INTRA_DC) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_16X4 && transform->primary_type == AV2H_TX_IDTX &&
           mode->mode == AV2H_INTRA_D45 && mode->fsc_mode) {
    first_shift = 6U;
    second_shift = 12U;
    rectangular = 0U;
    identity_rows = 1U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_32X4 && transform->primary_type == AV2H_TX_H_DCT &&
           mode->mode == AV2H_INTRA_D67) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 1U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_32X4 && transform->primary_type == AV2H_TX_FLIPADST_DCT &&
           (mode->mode == AV2H_INTRA_D67 || mode->mode == AV2H_INTRA_D157)) {
    first_shift = 7U;
    second_shift = 11U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
    flip_adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_32X8 && transform->primary_type == AV2H_TX_DCT_DCT &&
           (mode->mode == AV2H_INTRA_DC || mode->mode == AV2H_INTRA_D45)) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else if (prefix->tx_size == AV2H_TX_32X8 && transform->primary_type == AV2H_TX_ADST_DCT &&
           mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 1U;
} else if (prefix->tx_size == AV2H_TX_32X32 && transform->primary_type == AV2H_TX_DCT_DCT) {
    first_shift = 6U;
    second_shift = 13U;
    rectangular = 0U;
    identity_rows = 0U;
    adst_rows = 0U;
    flip_adst_rows = 0U;
    identity_columns = 0U;
    adst_columns = 0U;
} else {
    return AV2H_UNSUPPORTED;
}
reconstruction->width = coefficients->width;
reconstruction->height = coefficients->height;
count = coefficients->coefficient_count;
if (transform->secondary_symbol != 0U) {
    for (index = 0U; index < count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
    status =
        av2h_inverse_secondary_transform(mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK)
        return status;
    if (rectangular) {
        for (index = 0U; index < count; ++index) {
            scratch->scaled[index] = av2h_sample_clamp(
                av2h_sample_round_pow2((int64_t)scratch->scaled[index] * 2896, 12U), -32768, 32767);
        }
    }
} else {
    for (index = 0U; index < count; ++index) {
        scratch->scaled[index] =
            rectangular
                ? av2h_sample_round_pow2((int64_t)coefficients->dequantized[index] * 2896, 12U)
                : coefficients->dequantized[index];
        scratch->scaled[index] = av2h_sample_clamp(scratch->scaled[index], -32768, 32767);
    }
}
status =
    av2h_inverse_rows(scratch->scaled, scratch->transposed, coefficients->width,
                      coefficients->height, first_shift, identity_rows, adst_rows, flip_adst_rows);
if (status != AV2H_OK)
    return status;
status = av2h_inverse_columns(scratch->transposed, scratch->residual, coefficients->width,
                              coefficients->height, second_shift, identity_columns, adst_columns,
                              flip_adst_columns);
if (status != AV2H_OK)
    return status;
for (index = 0U; index < count; ++index) {
    int32_t predicted = predictor == NULL
                            ? domain->midpoint
                            : predictor[(index / coefficients->width) * predictor_stride_samples +
                                        index % coefficients->width];
    int32_t pixel = av2h_sample_clip_to_domain(predicted + scratch->residual[index], domain);
    reconstruction->residual[index] = (int16_t)scratch->residual[index];
    reconstruction->pixels[index] = (uint8_t)pixel;
}
return AV2H_OK;
}
#endif
Av2hStatus AV2H_KERNEL_NAME(av2h_chroma_reconstruct, av2h_chroma_reconstruct_u16)(
    Av2hDecodeDiagnostics *diagnostics, const Av2hChromaLeafMode *mode,
    const Av2hCoeffPrefix *prefix, const Av2hTransformType *transform,
    const Av2hCoeffBlock *coefficients, const AV2H_KERNEL_TYPE(uint8_t, uint16_t) * predictor,
#if !AV2H_KERNEL_WIDE
    uint16_t width, uint16_t height,
#endif
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
#endif
    Av2hLeafMode luma_mode;
#if AV2H_KERNEL_WIDE
    const Av2hTxDescriptor *descriptor;
    Av2hStatus status;
    if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
        predictor == NULL || scratch == NULL || reconstruction == NULL) {
#else
        Av2hCoeffPrefix effective_prefix;
        size_t index;
        if (mode == NULL || prefix == NULL || transform == NULL || coefficients == NULL ||
            predictor == NULL || scratch == NULL || reconstruction == NULL || width == 0U ||
            height == 0U || (size_t)width * height > AV2H_RECON_SAMPLE_CAPACITY) {
#endif
        return AV2H_INVALID_ARGUMENT;
    }
#if AV2H_KERNEL_WIDE
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK)
        return status;
    if (prefix->q_index == 0U && prefix->tx_size == AV2H_TX_4X4) {
        status = reconstruct_lossless_4x4_u16(coefficients, predictor, mode->use_dpcm_uv,
                                              mode->dpcm_mode_uv,
                                              (uint8_t)(transform->primary_type == AV2H_TX_IDTX),
                                              mode->use_dpcm_uv, reconstruction, domain);
    } else {
#else
        if (prefix->all_zero && coefficients->nonzero_count == 0U) {
            for (index = 0U; index < coefficients->coefficient_count; ++index) {
                if (coefficients->quantized[index] != 0 || coefficients->dequantized[index] != 0) {
                    return AV2H_UNSUPPORTED;
                }
            }
            reconstruction->width = width;
            reconstruction->height = height;
            for (index = 0U; index < (size_t)width * height; ++index) {
                reconstruction->residual[index] = 0;
                reconstruction->pixels[index] = predictor[index];
            }
            return AV2H_OK;
        }
#endif
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
#if !AV2H_KERNEL_WIDE
        if (mode->is_cfl) {
            luma_mode.mode = AV2H_INTRA_DC;
        } else {
#endif
            luma_mode.mode = mode->mode;
            luma_mode.angle_delta = mode->angle_delta;
#if AV2H_KERNEL_WIDE
            luma_mode.edge_ibp = mode->edge_ibp;
            luma_mode.above_smooth = mode->above_smooth;
            luma_mode.left_smooth = mode->left_smooth;
            status = av2h_reconstruct_transform_generic_u16(
                diagnostics, &luma_mode, prefix, transform, coefficients, predictor,
                descriptor->output_width, scratch, reconstruction, domain);
#endif
        }
#if AV2H_KERNEL_WIDE
        return status;
#else
    luma_mode.use_dpcm_y = mode->use_dpcm_uv;
    luma_mode.dpcm_mode_y = mode->dpcm_mode_uv;
    luma_mode.fsc_mode = transform->primary_type == AV2H_TX_IDTX ? 1U : 0U;
    effective_prefix = *prefix;
    if (coefficients->nonzero_count != 0U) {
        effective_prefix.all_zero = 0U;
#endif
    }
#if !AV2H_KERNEL_WIDE
    return av2h_luma_reconstruct(diagnostics, &luma_mode, &effective_prefix, transform,
                                 coefficients, predictor, width, scratch, reconstruction);
}
#endif
