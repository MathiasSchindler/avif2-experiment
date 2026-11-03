#ifndef AV2H_CONTAINER_H
#define AV2H_CONTAINER_H

#include "av2heif.h"

#define AV2H_FOURCC(a, b, c, d)                                                                    \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

enum {
    BOX_FTYP = AV2H_FOURCC('f', 't', 'y', 'p'),
    BOX_META = AV2H_FOURCC('m', 'e', 't', 'a'),
    BOX_HDLR = AV2H_FOURCC('h', 'd', 'l', 'r'),
    BOX_PITM = AV2H_FOURCC('p', 'i', 't', 'm'),
    BOX_ILOC = AV2H_FOURCC('i', 'l', 'o', 'c'),
    BOX_IINF = AV2H_FOURCC('i', 'i', 'n', 'f'),
    BOX_INFE = AV2H_FOURCC('i', 'n', 'f', 'e'),
    BOX_IPRP = AV2H_FOURCC('i', 'p', 'r', 'p'),
    BOX_IPCO = AV2H_FOURCC('i', 'p', 'c', 'o'),
    BOX_IPMA = AV2H_FOURCC('i', 'p', 'm', 'a'),
    BOX_AV2C = AV2H_FOURCC('a', 'v', '2', 'C'),
    BOX_ISPE = AV2H_FOURCC('i', 's', 'p', 'e'),
    BOX_PIXI = AV2H_FOURCC('p', 'i', 'x', 'i'),
    BOX_COLR = AV2H_FOURCC('c', 'o', 'l', 'r'),
    BOX_MDAT = AV2H_FOURCC('m', 'd', 'a', 't'),
    BRAND_MIF1 = AV2H_FOURCC('m', 'i', 'f', '1'),
    BRAND_AVIF = AV2H_FOURCC('a', 'v', 'i', 'f'),
    HANDLER_PICT = AV2H_FOURCC('p', 'i', 'c', 't'),
    ITEM_AV02 = AV2H_FOURCC('a', 'v', '0', '2'),
    COLOR_NCLX = AV2H_FOURCC('n', 'c', 'l', 'x')
};

typedef struct {
    Av2hSpan av2c_payload;
    Av2hSpan item_payload;
    uint8_t has_mif1_brand;
    uint8_t has_avif_brand;
    uint8_t has_primary_av02;
    uint8_t av2c_associated;
} Av2hContainerProbe;

typedef struct {
    uint8_t channel_count;
    uint8_t bits_per_channel;
} ContainerPixelMetadata;

Av2hStatus av2h_container_probe(Av2hSpan input, Av2hContainerProbe *probe, Av2hError *error);
Av2hStatus av2h_container_fail(Av2hError *error, Av2hStatus status, size_t offset);
Av2hStatus av2h_container_sequence_pixel_metadata(const Av2hSequenceInfo *sequence,
                                                  ContainerPixelMetadata *metadata);
int av2h_container_color_description_equal(const Av2hColorInfo *left, const Av2hColorInfo *right);

#endif
