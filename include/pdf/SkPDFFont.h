
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkPDFFont_DEFINED
#define SkPDFFont_DEFINED

#include "SkAdvancedTypefaceMetrics.h"
#include "SkBitSet.h"
#include "SkPDFTypes.h"
#include "SkTDArray.h"
#include "SkThread.h"
#include "SkTypeface.h"

class SkPaint;
class SkPDFCatalog;
class SkPDFFont;

class SkPDFGlyphSet : public SkNoncopyable {
public:
    SkPDFGlyphSet();

    void set(const uint16_t* glyphIDs, int numGlyphs);
    bool has(uint16_t glyphID) const;
    void merge(const SkPDFGlyphSet& usage);
    void exportTo(SkTDArray<uint32_t>* glyphIDs) const;

private:
    SkBitSet fBitSet;
};

class SkPDFGlyphSetMap : public SkNoncopyable {
public:
    struct FontGlyphSetPair {
        FontGlyphSetPair(SkPDFFont* font, SkPDFGlyphSet* glyphSet);

        SkPDFFont* fFont;
        SkPDFGlyphSet* fGlyphSet;
    };

    SkPDFGlyphSetMap();
    ~SkPDFGlyphSetMap();

    class F2BIter {
    public:
        explicit F2BIter(const SkPDFGlyphSetMap& map);
        FontGlyphSetPair* next() const;
        void reset(const SkPDFGlyphSetMap& map);

    private:
        const SkTDArray<FontGlyphSetPair>* fMap;
        mutable int fIndex;
    };

    void merge(const SkPDFGlyphSetMap& usage);
    void reset();

    void noteGlyphUsage(SkPDFFont* font, const uint16_t* glyphIDs,
                        int numGlyphs);

private:
    SkPDFGlyphSet* getGlyphSetForFont(SkPDFFont* font);

    SkTDArray<FontGlyphSetPair> fMap;
};


/** \class SkPDFFont
    A PDF Object class representing a font.  The font may have resources
    attached to it in order to embed the font.  SkPDFFonts are canonicalized
    so that resource deduplication will only include one copy of a font.
    This class uses the same pattern as SkPDFGraphicState, a static weak
    reference to each instantiated class.
*/
class SkPDFFont : public SkPDFDict {
public:
    SK_API virtual ~SkPDFFont();

    SK_API virtual void getResources(SkTDArray<SkPDFObject*>* resourceList);

    /** Returns the typeface represented by this class. Returns NULL for the
     *  default typeface.
     */
    SK_API SkTypeface* typeface();

    /** Returns the font type represented in this font.  For Type0 fonts,
     *  returns the type of the decendant font.
     */
    SK_API virtual SkAdvancedTypefaceMetrics::FontType getType();

    /** Returns true if this font encoding supports glyph IDs above 255.
     */
    SK_API virtual bool multiByteGlyphs() const = 0;

    /** Return true if this font has an encoding for the passed glyph id.
     */
    SK_API bool hasGlyph(uint16_t glyphID);

    /** Convert (in place) the input glyph IDs into the font encoding.  If the
     *  font has more glyphs than can be encoded (like a type 1 font with more
     *  than 255 glyphs) this method only converts up to the first out of range
     *  glyph ID.
     *  @param glyphIDs       The input text as glyph IDs.
     *  @param numGlyphs      The number of input glyphs.
     *  @return               Returns the number of glyphs consumed.
     */
    SK_API size_t glyphsToPDFFontEncoding(uint16_t* glyphIDs, size_t numGlyphs);

    /** Get the font resource for the passed typeface and glyphID. The
     *  reference count of the object is incremented and it is the caller's
     *  responsibility to unreference it when done.  This is needed to
     *  accommodate the weak reference pattern used when the returned object
     *  is new and has no other references.
     *  @param typeface  The typeface to find.
     *  @param glyphID   Specify which section of a large font is of interest.
     */
    SK_API static SkPDFFont* GetFontResource(SkTypeface* typeface,
                                             uint16_t glyphID);

    /** Subset the font based on usage set. Returns a SkPDFFont instance with
     *  subset.
     *  @param usage  Glyph subset requested.
     *  @return       NULL if font does not support subsetting, a new instance
     *                of SkPDFFont otherwise.
     */
    SK_API virtual SkPDFFont* getFontSubset(const SkPDFGlyphSet* usage);

protected:
    // Common constructor to handle common members.
    SkPDFFont(SkAdvancedTypefaceMetrics* fontInfo, SkTypeface* typeface,
              uint16_t glyphID, bool descendantFont);

    // Accessors for subclass.
    SkAdvancedTypefaceMetrics* fontInfo();
    void setFontInfo(SkAdvancedTypefaceMetrics* info);
    uint16_t firstGlyphID() const;
    uint16_t lastGlyphID() const;
    void setLastGlyphID(uint16_t glyphID);

    // Add object to resource list.
    void addResource(SkPDFObject* object);

    // Accessors for FontDescriptor associated with this object.
    SkPDFDict* getFontDescriptor();
    void setFontDescriptor(SkPDFDict* descriptor);

    // Add common entries to FontDescriptor.
    bool addCommonFontDescriptorEntries(int16_t defaultWidth);

    /** Set fFirstGlyphID and fLastGlyphID to span at most 255 glyphs,
     *  including the passed glyphID.
     */
    void adjustGlyphRangeForSingleByteEncoding(int16_t glyphID);

    // Generate ToUnicode table according to glyph usage subset.
    // If subset is NULL, all available glyph ids will be used.
    void populateToUnicodeTable(const SkPDFGlyphSet* subset);

    // Create instances of derived types based on fontInfo.
    static SkPDFFont* Create(SkAdvancedTypefaceMetrics* fontInfo,
                             SkTypeface* typeface, uint16_t glyphID,
                             SkPDFDict* fontDescriptor);

    static bool Find(uint32_t fontID, uint16_t glyphID, int* index);

private:
    class FontRec {
    public:
        SkPDFFont* fFont;
        uint32_t fFontID;
        uint16_t fGlyphID;

        // A fGlyphID of 0 with no fFont always matches.
        bool operator==(const FontRec& b) const;
        FontRec(SkPDFFont* font, uint32_t fontID, uint16_t fGlyphID);
    };

    SkRefPtr<SkTypeface> fTypeface;

    // The glyph IDs accessible with this font.  For Type1 (non CID) fonts,
    // this will be a subset if the font has more than 255 glyphs.
    uint16_t fFirstGlyphID;
    uint16_t fLastGlyphID;
    // The font info is only kept around after construction for large
    // Type1 (non CID) fonts that need multiple "fonts" to access all glyphs.
    SkRefPtr<SkAdvancedTypefaceMetrics> fFontInfo;
    SkTDArray<SkPDFObject*> fResources;
    SkRefPtr<SkPDFDict> fDescriptor;

    SkAdvancedTypefaceMetrics::FontType fFontType;

    // This should be made a hash table if performance is a problem.
    static SkTDArray<FontRec>& CanonicalFonts();
    static SkMutex& CanonicalFontsMutex();
};

#endif
