/*
 * Generated by asn1c-0.9.24 (http://lionet.info/asn1c)
 * From ASN.1 module "EUTRA-RRC-Definitions"
 * 	found in "fixed_grammar.asn"
 * 	`asn1c -gen-PER`
 */

#include "PhysCellIdGERAN.h"

static int
memb_networkColourCode_constraint_1(asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	const BIT_STRING_t *st = (const BIT_STRING_t *)sptr;
	size_t size;
	
	if(!sptr) {
		_ASN_CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	if(st->size > 0) {
		/* Size in bits */
		size = 8 * st->size - (st->bits_unused & 0x07);
	} else {
		size = 0;
	}
	
	if((size == 3)) {
		/* Constraint check succeeded */
		return 0;
	} else {
		_ASN_CTFAIL(app_key, td, sptr,
			"%s: constraint failed (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
}

static int
memb_baseStationColourCode_constraint_1(asn_TYPE_descriptor_t *td, const void *sptr,
			asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	const BIT_STRING_t *st = (const BIT_STRING_t *)sptr;
	size_t size;
	
	if(!sptr) {
		_ASN_CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
	
	if(st->size > 0) {
		/* Size in bits */
		size = 8 * st->size - (st->bits_unused & 0x07);
	} else {
		size = 0;
	}
	
	if((size == 3)) {
		/* Constraint check succeeded */
		return 0;
	} else {
		_ASN_CTFAIL(app_key, td, sptr,
			"%s: constraint failed (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}
}

static asn_per_constraints_t asn_PER_memb_networkColourCode_constr_2 GCC_NOTUSED = {
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	{ APC_CONSTRAINED,	 0,  0,  3,  3 }	/* (SIZE(3..3)) */,
	0, 0	/* No PER value map */
};
static asn_per_constraints_t asn_PER_memb_baseStationColourCode_constr_3 GCC_NOTUSED = {
	{ APC_UNCONSTRAINED,	-1, -1,  0,  0 },
	{ APC_CONSTRAINED,	 0,  0,  3,  3 }	/* (SIZE(3..3)) */,
	0, 0	/* No PER value map */
};
static asn_TYPE_member_t asn_MBR_PhysCellIdGERAN_1[] = {
	{ ATF_NOFLAGS, 0, offsetof(struct PhysCellIdGERAN, networkColourCode),
		(ASN_TAG_CLASS_CONTEXT | (0 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_BIT_STRING,
		memb_networkColourCode_constraint_1,
		&asn_PER_memb_networkColourCode_constr_2,
		0,
		"networkColourCode"
		},
	{ ATF_NOFLAGS, 0, offsetof(struct PhysCellIdGERAN, baseStationColourCode),
		(ASN_TAG_CLASS_CONTEXT | (1 << 2)),
		-1,	/* IMPLICIT tag at current level */
		&asn_DEF_BIT_STRING,
		memb_baseStationColourCode_constraint_1,
		&asn_PER_memb_baseStationColourCode_constr_3,
		0,
		"baseStationColourCode"
		},
};
static ber_tlv_tag_t asn_DEF_PhysCellIdGERAN_tags_1[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (16 << 2))
};
static asn_TYPE_tag2member_t asn_MAP_PhysCellIdGERAN_tag2el_1[] = {
    { (ASN_TAG_CLASS_CONTEXT | (0 << 2)), 0, 0, 0 }, /* networkColourCode at 3071 */
    { (ASN_TAG_CLASS_CONTEXT | (1 << 2)), 1, 0, 0 } /* baseStationColourCode at 3072 */
};
static asn_SEQUENCE_specifics_t asn_SPC_PhysCellIdGERAN_specs_1 = {
	sizeof(struct PhysCellIdGERAN),
	offsetof(struct PhysCellIdGERAN, _asn_ctx),
	asn_MAP_PhysCellIdGERAN_tag2el_1,
	2,	/* Count of tags in the map */
	0, 0, 0,	/* Optional elements (not needed) */
	-1,	/* Start extensions */
	-1	/* Stop extensions */
};
asn_TYPE_descriptor_t asn_DEF_PhysCellIdGERAN = {
	"PhysCellIdGERAN",
	"PhysCellIdGERAN",
	SEQUENCE_free,
	SEQUENCE_print,
	SEQUENCE_constraint,
	SEQUENCE_decode_ber,
	SEQUENCE_encode_der,
	SEQUENCE_decode_xer,
	SEQUENCE_encode_xer,
	SEQUENCE_decode_uper,
	SEQUENCE_encode_uper,
	SEQUENCE_decode_aper,
	SEQUENCE_encode_aper,
	0,	/* Use generic outmost tag fetcher */
	asn_DEF_PhysCellIdGERAN_tags_1,
	sizeof(asn_DEF_PhysCellIdGERAN_tags_1)
		/sizeof(asn_DEF_PhysCellIdGERAN_tags_1[0]), /* 1 */
	asn_DEF_PhysCellIdGERAN_tags_1,	/* Same as above */
	sizeof(asn_DEF_PhysCellIdGERAN_tags_1)
		/sizeof(asn_DEF_PhysCellIdGERAN_tags_1[0]), /* 1 */
	0,	/* No PER visible constraints */
	asn_MBR_PhysCellIdGERAN_1,
	2,	/* Elements count */
	&asn_SPC_PhysCellIdGERAN_specs_1	/* Additional specs */
};

