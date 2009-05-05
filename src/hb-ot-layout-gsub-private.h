/*
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, an OpenType Layout engine library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUB_PRIVATE_H
#define HB_OT_LAYOUT_GSUB_PRIVATE_H

#include "hb-ot-layout-private.h"

#include "hb-ot-layout-open-private.h"
#include "hb-ot-layout-gdef-private.h"

#include "harfbuzz-buffer-private.h" /* XXX */

#define DEFINE_GET_GLYPH_COVERAGE(name) \
  inline unsigned int get_##name (hb_codepoint_t glyph) const { \
    const Coverage &c = get_coverage (); \
    return c.get_coverage (glyph); \
  }

#define SUBTABLE_SUBSTITUTE_ARGS_DEF \
	hb_ot_layout_t *layout, \
	hb_buffer_t    *buffer, \
	unsigned int    context_length, \
	unsigned int    nesting_level_left HB_GNUC_UNUSED, \
	unsigned int    lookup_flag
#define SUBTABLE_SUBSTITUTE_ARGS \
	layout, \
	buffer, \
	context_length, \
	nesting_level_left, \
	lookup_flag

struct SingleSubstFormat1 {

  friend struct SingleSubst;

  private:
  DEFINE_GET_ACCESSOR (Coverage, coverage, coverage);
  DEFINE_GET_GLYPH_COVERAGE (glyph_coverage);

  inline bool single_substitute (hb_codepoint_t &glyph_id) const {

    unsigned int index = get_glyph_coverage (glyph_id);
    if (NOT_COVERED == index)
      return false;

    glyph_id += deltaGlyphID;

    return true;
  }

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  SHORT		deltaGlyphID;		/* Add to original GlyphID to get
					 * substitute GlyphID */
};
ASSERT_SIZE (SingleSubstFormat1, 6);

struct SingleSubstFormat2 {

  friend struct SingleSubst;

  private:
  DEFINE_GET_ACCESSOR (Coverage, coverage, coverage);
  DEFINE_GET_GLYPH_COVERAGE (glyph_coverage);

  inline bool single_substitute (hb_codepoint_t &glyph_id) const {

    unsigned int index = get_glyph_coverage (glyph_id);

    if (index >= glyphCount)
      return false;

    glyph_id = substitute[index];
    return true;
  }

  private:
  USHORT	substFormat;		/* Format identifier--format = 2 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	glyphCount;		/* Number of GlyphIDs in the Substitute
					 * array */
  GlyphID	substitute[];		/* Array of substitute
					 * GlyphIDs--ordered by Coverage  Index */
};
ASSERT_SIZE (SingleSubstFormat2, 6);

struct SingleSubst {

  friend struct SubstLookupSubTable;

  private:

  unsigned int get_size (void) const {
    switch (u.substFormat) {
    case 1: return sizeof (u.format1);
    case 2: return sizeof (u.format2);
    default:return sizeof (u.substFormat);
    }
  }

  inline bool single_substitute (hb_codepoint_t &glyph_id) const {
    switch (u.substFormat) {
    case 1: return u.format1.single_substitute (glyph_id);
    case 2: return u.format2.single_substitute (glyph_id);
    default:return false;
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {

    if (HB_UNLIKELY (context_length < 1))
      return false;

    unsigned int property;
    if (!_hb_ot_layout_check_glyph_property (layout, IN_CURITEM (), lookup_flag, &property))
      return false;

    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    if (!single_substitute (glyph_id))
      return false;

    _hb_buffer_replace_output_glyph (buffer, glyph_id, context_length == NO_CONTEXT);

    if ( _hb_ot_layout_has_new_glyph_classes (layout) )
    {
      /* we inherit the old glyph class to the substituted glyph */
      _hb_ot_layout_set_glyph_property (layout, glyph_id, property);
    }

    return true;
  }

  private:
  union {
  USHORT	substFormat;	/* Format identifier */
  SingleSubstFormat1	format1;
  SingleSubstFormat2	format2;
  } u;
};
DEFINE_NULL (SingleSubst, 2);


struct Sequence {

  friend struct MultipleSubstFormat1;

  private:
  /* GlyphID tables, in Coverage Index order */
  DEFINE_OFFSET_ARRAY_TYPE (GlyphID, substitute, glyphCount);

  inline void set_glyph_class (hb_ot_layout_t *layout, unsigned int property) const {

    unsigned int count = glyphCount;
    for (unsigned int n = 0; n < count; n++)
      _hb_ot_layout_set_glyph_property (layout, substitute[n], property);
  }

  private:
  USHORT	glyphCount;		/* Number of GlyphIDs in the Substitute
					 * array. This should always  be
					 * greater than 0. */
  GlyphID	substitute[];		/* String of GlyphIDs to substitute */
};
DEFINE_NULL_ASSERT_SIZE (Sequence, 2);

struct MultipleSubstFormat1 {

  friend struct MultipleSubst;

  private:
  /* Sequence tables, in Coverage Index order */
  DEFINE_OFFSET_ARRAY_TYPE (Sequence, sequence, sequenceCount);
  DEFINE_GET_ACCESSOR (Coverage, coverage, coverage);
  DEFINE_GET_GLYPH_COVERAGE (glyph_coverage);

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {

    if (HB_UNLIKELY (context_length < 1))
      return false;

    unsigned int property;
    if (!_hb_ot_layout_check_glyph_property (layout, IN_CURITEM (), lookup_flag, &property))
      return false;

    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    unsigned int index = get_glyph_coverage (glyph_id);

    const Sequence &seq = (*this)[index];

    if (HB_UNLIKELY (!seq.get_len ()))
      return false;

    _hb_buffer_add_output_glyph_ids (buffer, 1,
				     seq.glyphCount, seq.substitute,
				     0xFFFF, 0xFFFF);

    if ( _hb_ot_layout_has_new_glyph_classes (layout) )
    {
      /* this is a guess only ... */

      if ( property == HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE )
        property = HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH;

      seq.set_glyph_class (layout, property);
    }

    return true;
  }

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	sequenceCount;		/* Number of Sequence table offsets in
					 * the Sequence array */
  Offset	sequence[];		/* Array of offsets to Sequence
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * Coverage Index */
};
ASSERT_SIZE (MultipleSubstFormat1, 6);

struct MultipleSubst {

  friend struct SubstLookupSubTable;

  private:

  unsigned int get_size (void) const {
    switch (u.substFormat) {
    case 1: return sizeof (u.format1);
    default:return sizeof (u.substFormat);
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {
    switch (u.substFormat) {
    case 1: return u.format1.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    default:return false;
    }
  }

  private:
  union {
  USHORT	substFormat;	/* Format identifier */
  MultipleSubstFormat1	format1;
  } u;
};
DEFINE_NULL (MultipleSubst, 2);


struct AlternateSet {

  /* GlyphIDs, in no particular order */
  DEFINE_ARRAY_TYPE (GlyphID, alternate, glyphCount);

  private:
  USHORT	glyphCount;		/* Number of GlyphIDs in the Alternate
					 * array */
  GlyphID	alternate[];		/* Array of alternate GlyphIDs--in
					 * arbitrary order */
};
DEFINE_NULL_ASSERT_SIZE (AlternateSet, 2);

struct AlternateSubstFormat1 {

  friend struct AlternateSubst;

  private:
  /* AlternateSet tables, in Coverage Index order */
  DEFINE_OFFSET_ARRAY_TYPE (AlternateSet, alternateSet, alternateSetCount);
  DEFINE_GET_ACCESSOR (Coverage, coverage, coverage);
  DEFINE_GET_GLYPH_COVERAGE (glyph_coverage);

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {

    if (HB_UNLIKELY (context_length < 1))
      return false;

    unsigned int property;
    if (!_hb_ot_layout_check_glyph_property (layout, IN_CURITEM (), lookup_flag, &property))
      return false;

    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    unsigned int index = get_glyph_coverage (glyph_id);

    const AlternateSet &alt_set = (*this)[index];

    if (HB_UNLIKELY (!alt_set.get_len ()))
      return false;

    unsigned int alt_index = 0;

    /* XXX callback to user to choose alternate
    if ( gsub->altfunc )
      alt_index = (gsub->altfunc)( buffer->out_pos, glyph_id,
				   aset.GlyphCount, aset.Alternate,
				   gsub->data );
				   */

    if (HB_UNLIKELY (alt_index >= alt_set.get_len ()))
      return false;

    glyph_id = alt_set[alt_index];

    _hb_buffer_replace_output_glyph (buffer, glyph_id, context_length == NO_CONTEXT);

    if ( _hb_ot_layout_has_new_glyph_classes (layout) )
    {
      /* we inherit the old glyph class to the substituted glyph */
      _hb_ot_layout_set_glyph_property (layout, glyph_id, property);
    }

    return true;
  }

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	alternateSetCount;	/* Number of AlternateSet tables */
  Offset	alternateSet[];		/* Array of offsets to AlternateSet
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * Coverage Index */
};
ASSERT_SIZE (AlternateSubstFormat1, 6);

struct AlternateSubst {

  friend struct SubstLookupSubTable;

  private:

  unsigned int get_size (void) const {
    switch (u.substFormat) {
    case 1: return sizeof (u.format1);
    default:return sizeof (u.substFormat);
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {
    switch (u.substFormat) {
    case 1: return u.format1.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    default:return false;
    }
  }

  private:
  union {
  USHORT	substFormat;	/* Format identifier */
  AlternateSubstFormat1	format1;
  } u;
};
DEFINE_NULL (AlternateSubst, 2);


struct Ligature {

  friend struct LigatureSet;

  private:
  DEFINE_ARRAY_TYPE (GlyphID, component, (compCount ? compCount - 1 : 0));

  inline bool substitute_ligature (SUBTABLE_SUBSTITUTE_ARGS_DEF, bool is_mark) const {

    unsigned int i, j;
    unsigned int property;
    unsigned int count = compCount;

    if (HB_UNLIKELY (buffer->in_pos + count > buffer->in_length ||
		     context_length < count))
      return false; /* Not enough glyphs in input or context */

    for (i = 1, j = buffer->in_pos + 1; i < count; i++, j++) {
      while (!_hb_ot_layout_check_glyph_property (layout, IN_ITEM (j), lookup_flag, &property)) {
	if (HB_UNLIKELY (j + count - i == buffer->in_length))
	  return false;
	j++;
      }

      if (!(property == HB_OT_LAYOUT_GLYPH_CLASS_MARK ||
	    property &  LookupFlag::MarkAttachmentType))
	is_mark = FALSE;

      if (HB_LIKELY (IN_GLYPH(j) != (*this)[i - 1]))
        return false;
    }
    if ( _hb_ot_layout_has_new_glyph_classes (layout) )
      /* this is just a guess ... */
      hb_ot_layout_set_glyph_class (layout, ligGlyph,
				    is_mark ? HB_OT_LAYOUT_GLYPH_CLASS_MARK
					    : HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE);

    if (j == buffer->in_pos + i) /* No input glyphs skipped */
      /* We don't use a new ligature ID if there are no skipped
	 glyphs and the ligature already has an ID. */
      _hb_buffer_add_output_glyph_ids (buffer, i,
				       1, &ligGlyph,
				       0xFFFF,
				       IN_LIGID (buffer->in_pos) ?
				       0xFFFF : _hb_buffer_allocate_ligid (buffer));
    else
    {
      unsigned int lig_id = _hb_buffer_allocate_ligid (buffer);
      _hb_buffer_add_output_glyph (buffer, ligGlyph, 0xFFFF, lig_id);

      /* Now we must do a second loop to copy the skipped glyphs to
	 `out' and assign component values to it.  We start with the
	 glyph after the first component.  Glyphs between component
	 i and i+1 belong to component i.  Together with the lig_id
	 value it is later possible to check whether a specific
	 component value really belongs to a given ligature. */

      for ( i = 0; i < count - 1; i++ )
      {
	while (!_hb_ot_layout_check_glyph_property (layout, IN_CURITEM(), lookup_flag, &property))
	  _hb_buffer_add_output_glyph (buffer, IN_CURGLYPH(), i, lig_id);

	(buffer->in_pos)++;
      }

      /* XXX We  should possibly reassign lig_id and component for any
       * components of a previous ligature that s now being removed as part of
       * this ligature. */
    }

    return true;
  }

  private:
  GlyphID	ligGlyph;		/* GlyphID of ligature to substitute */
  USHORT	compCount;		/* Number of components in the ligature */
  GlyphID	component[];		/* Array of component GlyphIDs--start
					 * with the second  component--ordered
					 * in writing direction */
};
DEFINE_NULL_ASSERT_SIZE (Ligature, 4);

struct LigatureSet {

  friend struct LigatureSubstFormat1;

  private:
  DEFINE_OFFSET_ARRAY_TYPE (Ligature, ligature, ligatureCount);

  inline bool substitute_ligature (SUBTABLE_SUBSTITUTE_ARGS_DEF, bool is_mark) const {

    unsigned int num_ligs = get_len ();
    for (unsigned int i = 0; i < num_ligs; i++) {
      const Ligature &lig = (*this)[i];
      if (lig.substitute_ligature (SUBTABLE_SUBSTITUTE_ARGS, is_mark))
        return true;
    }

    return false;
  }

  private:
  USHORT	ligatureCount;		/* Number of Ligature tables */
  Offset	ligature[];		/* Array of offsets to Ligature
					 * tables--from beginning of
					 * LigatureSet table--ordered by
					 * preference */
};
DEFINE_NULL_ASSERT_SIZE (LigatureSet, 2);

struct LigatureSubstFormat1 {

  friend struct LigatureSubst;

  private:
  /* LigatureSet tables, in Coverage Index order */
  DEFINE_OFFSET_ARRAY_TYPE (LigatureSet, ligatureSet, ligSetCount);
  DEFINE_GET_ACCESSOR (Coverage, coverage, coverage);
  DEFINE_GET_GLYPH_COVERAGE (glyph_coverage);

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {

    unsigned int property;
    if (!_hb_ot_layout_check_glyph_property (layout, IN_CURITEM (), lookup_flag, &property))
      return false;

    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    unsigned int index = get_glyph_coverage (glyph_id);

    bool first_is_mark = (property == HB_OT_LAYOUT_GLYPH_CLASS_MARK ||
			  property &  LookupFlag::MarkAttachmentType);

    const LigatureSet &lig_set = (*this)[index];
    return lig_set.substitute_ligature (SUBTABLE_SUBSTITUTE_ARGS, first_is_mark);
  }

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	ligSetCount;		/* Number of LigatureSet tables */
  Offset	ligatureSet[];		/* Array of offsets to LigatureSet
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * Coverage Index */
};
ASSERT_SIZE (LigatureSubstFormat1, 6);

struct LigatureSubst {

  friend struct SubstLookupSubTable;

  private:

  unsigned int get_size (void) const {
    switch (u.substFormat) {
    case 1: return sizeof (u.format1);
    default:return sizeof (u.substFormat);
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {
    switch (u.substFormat) {
    case 1: return u.format1.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    default:return false;
    }
  }

  private:
  union {
  USHORT	substFormat;	/* Format identifier */
  LigatureSubstFormat1	format1;
  } u;
};
DEFINE_NULL (LigatureSubst, 2);


struct SubstLookupRecord {
  /* TODO */

  private:
  USHORT	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  USHORT	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
};
DEFINE_NULL_ASSERT_SIZE (SubstLookupRecord, 4);

struct ContextSubstFormat1 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	subRuleSetCount;	/* Number of SubRuleSet tables--must
					 * equal GlyphCount in Coverage  table */
  Offset	subRuleSet[];		/* Array of offsets to SubRuleSet
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * Coverage Index */
};
ASSERT_SIZE (ContextSubstFormat1, 6);

struct SubRuleSet {
  /* TODO */

  private:
  USHORT	subRuleCount;		/* Number of SubRule tables */
  Offset	subRule[];		/* Array of offsets to SubRule
					 * tables--from beginning of SubRuleSet
					 * table--ordered by preference */
};
DEFINE_NULL_ASSERT_SIZE (SubRuleSet, 2);

struct SubRule {
  /* TODO */

  private:
  USHORT	glyphCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the  first
					 * glyph */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  GlyphID	input[];		/* Array of input GlyphIDs--start with
					 * second glyph */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order */
};
DEFINE_NULL_ASSERT_SIZE (SubRule, 4);

struct ContextSubstFormat2 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 2 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  Offset	classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of Substitution  table */
  USHORT	subClassSetCnt;		/* Number of SubClassSet tables */
  Offset	subClassSet[];		/* Array of offsets to SubClassSet
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * class--may be NULL */
};
ASSERT_SIZE (ContextSubstFormat2, 8);

struct SubClassSet {
  /* TODO */

  private:
  USHORT	subClassRuleCnt;	/* Number of SubClassRule tables */
  Offset	subClassRule[];		/* Array of offsets to SubClassRule
					 * tables--from beginning of
					 * SubClassSet--ordered by preference */
};
DEFINE_NULL_ASSERT_SIZE (SubClassSet, 2);

struct SubClassRule {
  /* TODO */

  private:
  USHORT	glyphCount;		/* Total number of classes
					 * specified for the context in the
					 * rule--includes the first class */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  USHORT	klass[];		/* Array of classes--beginning with the
					 * second class--to be matched  to the
					 * input glyph class sequence */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order */
};
DEFINE_NULL_ASSERT_SIZE (SubClassRule, 4);

struct ContextSubstFormat3 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 3 */
  USHORT	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  Offset	coverage[];		/* Array of offsets to Coverage
					 * table--from beginning of
					 * Substitution table--in glyph
					 * sequence order */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order */
};
ASSERT_SIZE (ContextSubstFormat3, 6);

struct ChainContextSubstFormat1 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  USHORT	chainSubRuleSetCount;	/* Number of ChainSubRuleSet
					 * tables--must equal GlyphCount in
					 * Coverage table */
  Offset	chainSubRuleSet[];	/* Array of offsets to ChainSubRuleSet
					 * tables--from beginning of
					 * Substitution table--ordered by
					 * Coverage Index */
};
ASSERT_SIZE (ChainContextSubstFormat1, 6);

struct ChainSubRuleSet {
  /* TODO */

  private:
  USHORT	chainSubRuleCount;	/* Number of ChainSubRule tables */
  Offset	chainSubRule[];		/* Array of offsets to ChainSubRule
					 * tables--from beginning of
					 * ChainSubRuleSet table--ordered
					 * by preference */
};
DEFINE_NULL_ASSERT_SIZE (ChainSubRuleSet, 2);

struct ChainSubRule {
  /* TODO */

  private:
  USHORT	backtrackGlyphCount;	/* Total number of glyphs in the
					 * backtrack sequence (number of
					 * glyphs to be matched before the
					 * first glyph) */
  GlyphID	backtrack[];		/* Array of backtracking GlyphID's
					 * (to be matched before the input
					 * sequence) */
  USHORT	inputGlyphCount;	/* Total number of glyphs in the input
					 * sequence (includes the first  glyph) */
  GlyphID	input[];		/* Array of input GlyphIDs (start with
					 * second glyph) */
  USHORT	lookaheadGlyphCount;	/* Total number of glyphs in the look
					 * ahead sequence (number of  glyphs to
					 * be matched after the input sequence) */
  GlyphID	lookAhead[];		/* Array of lookahead GlyphID's (to be
					 * matched after  the input sequence) */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order) */
};
DEFINE_NULL_ASSERT_SIZE (ChainSubRule, 8);

struct ChainContextSubstFormat2 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 2 */
  Offset	coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  Offset	backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of Substitution
					 * table */
  Offset	inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of Substitution
					 * table */
  Offset	lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of Substitution
					 * table */
  USHORT	chainSubClassSetCnt;	/* Number of ChainSubClassSet tables */
  Offset	chainSubClassSet[];	/* Array of offsets to ChainSubClassSet
					 * tables--from beginning of
					 * Substitution table--ordered by input
					 * class--may be NULL */
};
ASSERT_SIZE (ChainContextSubstFormat2, 12);

struct ChainSubClassSet {
  /* TODO */

  private:
  USHORT	chainSubClassRuleCnt;	/* Number of ChainSubClassRule tables */
  Offset	chainSubClassRule[];	/* Array of offsets
					 * to ChainSubClassRule
					 * tables--from beginning of
					 * ChainSubClassSet--ordered by
					 * preference */
};
DEFINE_NULL_ASSERT_SIZE (ChainSubClassSet, 2);

struct ChainSubClassRule {
  /* TODO */

  private:
  USHORT	backtrackGlyphCount;	/* Total number of glyphs in the
					 * backtrack sequence (number of
					 * glyphs to be matched before the
					 * first glyph) */
  USHORT	backtrack[];		/* Array of backtracking classes(to be
					 * matched before the input  sequence) */
  USHORT	inputGlyphCount;	/* Total number of classes in the input
					 * sequence (includes the  first class) */
  USHORT	input[];		/* Array of input classes(start with
					 * second class; to  be matched with
					 * the input glyph sequence) */
  USHORT	lookaheadGlyphCount;	/* Total number of classes in the
					 * look ahead sequence (number of
					 * classes to be matched after the
					 * input sequence) */
  USHORT	lookAhead[];		/* Array of lookahead classes(to be
					 * matched after the  input sequence) */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order) */
};
DEFINE_NULL_ASSERT_SIZE (ChainSubClassRule, 8);

struct ChainContextSubstFormat3 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 3 */
  USHORT	backtrackGlyphCount;	/* Number of glyphs in the backtracking
					 * sequence */
  Offset	backtrackCoverage[];	/* Array of offsets to coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  USHORT	inputGlyphCount;	/* Number of glyphs in input sequence */
  Offset	inputCoverage[];	/* Array of offsets to coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  USHORT	lookaheadGlyphCount;	/* Number of glyphs in lookahead
					 * sequence */
  Offset	lookaheadCoverage[];	/* Array of offsets to coverage tables
					 * in lookahead sequence, in  glyph
					 * sequence order */
  USHORT	substCount;		/* Number of SubstLookupRecords */
  SubstLookupRecord substLookupRecord[];/* Array of SubstLookupRecords--in
					 * design order */
};
ASSERT_SIZE (ChainContextSubstFormat3, 10);


struct ExtensionSubstFormat1 {

  friend struct ExtensionSubst;

  private:
  inline unsigned int get_type (void) const { return extensionLookupType; }
  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const;

  private:
  USHORT	substFormat;		/* Format identifier. Set to 1. */
  USHORT	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  ULONG		extensionOffset;	/* Offset to the extension subtable,
					 * of lookup type  subtable. */
};
ASSERT_SIZE (ExtensionSubstFormat1, 8);

struct ExtensionSubst {

  friend struct SubstLookup;
  friend struct SubstLookupSubTable;

  private:

  unsigned int get_size (void) const {
    switch (u.substFormat) {
    case 1: return sizeof (u.format1);
    default:return sizeof (u.substFormat);
    }
  }

  inline unsigned int get_type (void) const {
    switch (u.substFormat) {
    case 1: return u.format1.get_type ();
    default:return 0;
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {
    switch (u.substFormat) {
    case 1: return u.format1.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    default:return false;
    }
  }

  private:
  union {
  USHORT	substFormat;	/* Format identifier */
  ExtensionSubstFormat1	format1;
  } u;
};
DEFINE_NULL (ExtensionSubst, 2);



struct ReverseChainSingleSubstFormat1 {
  /* TODO */

  private:
  USHORT	substFormat;		/* Format identifier--format = 1 */
  Offset	coverage;		/* Offset to Coverage table -- from
					 * beginning of Substitution table */
  USHORT	backtrackGlyphCount;	/* Number of glyphs in the backtracking
					 * sequence */
  Offset	backtrackCoverage[];	/* Array of offsets to coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  USHORT	lookaheadGlyphCount;	/* Number of glyphs in lookahead
					 * sequence */
  Offset	lookaheadCoverage[];	/* Array of offsets to coverage tables
					 * in lookahead sequence, in  glyph
					 * sequence order */
  USHORT	glyphCount;		/* Number of GlyphIDs in the Substitute
					 * array */
  GlyphID	substitute[];		/* Array of substitute
					 * GlyphIDs--ordered by Coverage  Index */
};
ASSERT_SIZE (ReverseChainSingleSubstFormat1, 10);

/*
 * SubstLookup
 */

enum {
  GSUB_Single				= 1,
  GSUB_Multiple				= 2,
  GSUB_Alternate			= 3,
  GSUB_Ligature				= 4,
  GSUB_Context				= 5,
  GSUB_ChainingContext			= 6,
  GSUB_Extension			= 7,
  GSUB_ReverseChainingContextSingle	= 8,
};

struct SubstLookupSubTable {
  DEFINE_NON_INSTANTIABLE(SubstLookupSubTable);

  friend struct SubstLookup;

  unsigned int get_size (unsigned int lookup_type) const {
    switch (lookup_type) {
    case GSUB_Single:				return u.single.get_size ();
    case GSUB_Multiple:				return u.multiple.get_size ();
    case GSUB_Alternate:			return u.alternate.get_size ();
    case GSUB_Ligature:				return u.ligature.get_size ();
   /*
    case GSUB_Context:
    case GSUB_ChainingContext:
   */
    case GSUB_Extension:			return u.extension.get_size ();
 /*
    case GSUB_ReverseChainingContextSingle:
  */
    default:return sizeof (LookupSubTable);
    }
  }

  inline bool substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF,
			  unsigned int    lookup_type) const {
    switch (lookup_type) {
    case GSUB_Single:				return u.single.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    case GSUB_Multiple:				return u.multiple.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    case GSUB_Alternate:			return u.alternate.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    case GSUB_Ligature:				return u.ligature.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    /*
    case GSUB_Context:				return u.context.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    case GSUB_ChainingContext:			return u.chainingContext.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    */
    case GSUB_Extension:			return u.extension.substitute (SUBTABLE_SUBSTITUTE_ARGS);
			/*
    case GSUB_ReverseChainingContextSingle:	return u.reverseChainingContextSingle.substitute (SUBTABLE_SUBSTITUTE_ARGS);
    */
    default:return false;
    }
  }

  private:
  union {
  USHORT				substFormat;
  SingleSubst				single;
  MultipleSubst				multiple;
  AlternateSubst			alternate;
  LigatureSubst				ligature;
  /*
  ContextSubst				context;
  ChainingContextSubst			chainingContext;
  */
  ExtensionSubst			extension;
  /*
  ReverseChainingContextSingleSubst	reverseChainingContextSingle;
  */
  } u;
};


/* Out-of-class implementation for methods chaining */

inline bool ExtensionSubstFormat1::substitute (SUBTABLE_SUBSTITUTE_ARGS_DEF) const {
  return (*(SubstLookupSubTable *)(((char *) this) + extensionOffset)).substitute (SUBTABLE_SUBSTITUTE_ARGS,
										   get_type ());
}



struct SubstLookup : Lookup {

  DEFINE_NON_INSTANTIABLE(SubstLookup);

  inline const SubstLookupSubTable& get_subtable (unsigned int i) const {
    return *(SubstLookupSubTable*)&(((Lookup *)this)->get_subtable (i));
  }

  /* Like get_type(), but looks through extension lookups.
   * Never returns Extension */
  inline unsigned int get_effective_type (void) const {
    unsigned int type = get_type ();

    if (HB_UNLIKELY (type == GSUB_Extension)) {
      /* Return lookup type of first extension subtable.
       * The spec says all of them should have the same type.
       * XXX check for that somehow */
      type = get_subtable(0).u.extension.get_type ();
    }

    return type;
  }

  inline bool is_reverse (void) const {
    switch (get_effective_type ()) {
    case GSUB_ReverseChainingContextSingle:	return true;
    default:					return false;
    }
  }

  bool substitute_once (hb_ot_layout_t *layout,
			hb_buffer_t    *buffer,
			unsigned int    context_length,
			unsigned int    nesting_level_left) const {

    unsigned int lookup_type = get_type ();
    unsigned int lookup_flag = get_flag ();

    if (HB_UNLIKELY (nesting_level_left == 0))
      return false;
    nesting_level_left--;

    for (unsigned int i = 0; i < get_subtable_count (); i++)
      if (get_subtable (i).substitute (SUBTABLE_SUBSTITUTE_ARGS,
				       lookup_type))
	return true;

    return false;
  }

  bool substitute_string (hb_ot_layout_t *layout,
			  hb_buffer_t    *buffer,
			  hb_ot_layout_feature_mask_t mask) const {

    bool ret = false;

    if (HB_LIKELY (!is_reverse ())) {

	/* in/out forward substitution */
	_hb_buffer_clear_output (buffer);
	buffer->in_pos = 0;
	while (buffer->in_pos < buffer->in_length) {

	  if ((~IN_PROPERTIES (buffer->in_pos) & mask) &&
	      substitute_once (layout, buffer, NO_CONTEXT, MAX_NESTING_LEVEL))
	    ret = true;
	  else
	    _hb_buffer_copy_output_glyph (buffer);

	}
	if (ret)
	  _hb_buffer_swap (buffer);

    } else {

	/* in-place backward substitution */
	buffer->in_pos = buffer->in_length - 1;
	do {

	  if ((~IN_PROPERTIES (buffer->in_pos) & mask) &&
	      substitute_once (layout, buffer, NO_CONTEXT, MAX_NESTING_LEVEL))
	    ret = true;
	  else
	    buffer->in_pos--;

	} while ((int) buffer->in_pos >= 0);
    }

    return ret;
  }
};
DEFINE_NULL_ALIAS (SubstLookup, Lookup);

/*
 * GSUB
 */

struct GSUB : GSUBGPOS {
  static const hb_tag_t Tag		= HB_TAG ('G','S','U','B');

  STATIC_DEFINE_GET_FOR_DATA (GSUB);
  /* XXX check version here? */

  inline const SubstLookup& get_lookup (unsigned int i) const {
    return *(SubstLookup*)&(((GSUBGPOS *)this)->get_lookup (i));
  }


};
DEFINE_NULL_ALIAS (GSUB, GSUBGPOS);


#endif /* HB_OT_LAYOUT_GSUB_PRIVATE_H */
