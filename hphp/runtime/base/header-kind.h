/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2016 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_HEADER_KIND_H_
#define incl_HPHP_HEADER_KIND_H_

#include <cstdint>
#include <array>

namespace HPHP {

/*
 * every request-allocated object has a 1-byte kind field in its
 * header, from this enum. If you update the enum, be sure to
 * update header_names[] as well, and either unset or change
 * HHVM_REPO_SCHEMA, because kind values are used in HHBC.
 */
enum class HeaderKind : uint8_t {
  // ArrayKind aliases
  Packed, Mixed, Empty, Apc, Globals, Proxy,
  // Hack arrays
  Dict, VecArray, Keyset,
  // Other ordinary refcounted heap objects
  String, Resource, Ref,
  Object, WaitHandle, AsyncFuncWH, AwaitAllWH, Closure,
  // Collections
  Vector, Map, Set, Pair, ImmVector, ImmMap, ImmSet,
  // other kinds, not used for countable objects.
  AsyncFuncFrame, // NativeNode followed by Frame, Resumable, AFWH
  NativeData, // a NativeData header preceding an HNI ObjectData
  ClosureHdr, // a ClosureHdr preceding a Closure ObjectData
  SmallMalloc, // small req::malloc'd block
  BigMalloc, // big req::malloc'd block
  BigObj, // big size-tracked object (valid header follows MallocNode)
  Free, // small block in a FreeList
  Hole, // wasted space not in any freelist
};
const unsigned NumHeaderKinds = unsigned(HeaderKind::Hole) + 1;
extern const std::array<char*,NumHeaderKinds> header_names;

inline bool haveCount(HeaderKind k) {
  return uint8_t(k) < uint8_t(HeaderKind::AsyncFuncFrame);
}

/*
 * RefCount type for m_count field in refcounted objects
 */
using RefCount = int32_t;

enum class Counted {
  Maybe, // objects can be static or uncounted, and support cow
  Always // objects must be in request-heap with positive refcounts
};

enum GCBits {
  Unmarked = 0,
  Mark = 1,
  CMark = 2,
  DualMark = 3,  // Mark|CMark
};

inline GCBits operator|(GCBits a, GCBits b) {
  return static_cast<GCBits>(
      static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
  );
}
inline bool operator&(GCBits a, GCBits b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/*
 * Common header for all heap-allocated objects. Layout is carefully
 * designed to allow overlapping with the second word of a TypedValue,
 * or to follow a C++ defined vptr.
 *
 * T can be any simple 16-bit type. CNT is Maybe for copy-on-write
 * objects that support being allocated outside the request heap with
 * a count field containing StaticValue or UncountedValue
 */
template<class T = uint16_t, Counted CNT = Counted::Always>
struct HeaderWord {
  union {
    struct {
      mutable RefCount count;
      HeaderKind kind;
      mutable bool weak_refed:1;
      mutable bool partially_inited:1;
      mutable GCBits marks:6;
      T aux;
    };
    struct { uint32_t lo32, hi32; };
    uint64_t q;
  };

  void init(HeaderKind kind, RefCount count) {
    q = uint64_t(kind) << (8 * offsetof(HeaderWord, kind)) |
        uint32_t(count) << (8 * offsetof(HeaderWord, count));
  }

  void init(T aux, HeaderKind kind, RefCount count) {
    q = uint64_t(kind)  << (8 * offsetof(HeaderWord, kind)) |
        uint64_t(uint16_t(aux))   << (8 * offsetof(HeaderWord, aux)) |
        uint32_t(count) << (8 * offsetof(HeaderWord, count));
    static_assert(sizeof(T) == 2, "header layout requres 2-byte aux");
  }

  void init(const HeaderWord<T,CNT>& h, RefCount count) {
    q = uint64_t(h.hi32) << 32 | uint32_t(count);
  }

  bool checkCount() const;
  bool isRefCounted() const;
  bool hasMultipleRefs() const;
  bool hasExactlyOneRef() const;
  bool isStatic() const;
  bool isUncounted() const;
  void incRefCount() const;
  void rawIncRefCount() const;
  void decRefCount() const;
  bool decWillRelease() const;
  bool decReleaseCheck();
};

constexpr auto HeaderOffset = 0;
constexpr auto HeaderKindOffset = HeaderOffset + offsetof(HeaderWord<>, kind);
constexpr auto FAST_REFCOUNT_OFFSET = HeaderOffset +
                                      offsetof(HeaderWord<>, count);

inline bool isObjectKind(HeaderKind k) {
  return k >= HeaderKind::Object && k <= HeaderKind::ImmSet;
}

inline bool isArrayKind(HeaderKind k) {
  return k >= HeaderKind::Packed && k <= HeaderKind::Keyset;
}

enum class CollectionType : uint8_t { // Subset of possible HeaderKind values
  // Values must be contiguous integers (for ArrayIter::initFuncTable).
  Vector = uint8_t(HeaderKind::Vector),
  Map = uint8_t(HeaderKind::Map),
  Set = uint8_t(HeaderKind::Set),
  Pair = uint8_t(HeaderKind::Pair),
  ImmVector = uint8_t(HeaderKind::ImmVector),
  ImmMap = uint8_t(HeaderKind::ImmMap),
  ImmSet = uint8_t(HeaderKind::ImmSet),
};

inline bool isVectorCollection(CollectionType ctype) {
  return ctype == CollectionType::Vector || ctype == CollectionType::ImmVector;
}
inline bool isMapCollection(CollectionType ctype) {
  return ctype == CollectionType::Map || ctype == CollectionType::ImmMap;
}
inline bool isSetCollection(CollectionType ctype) {
  return ctype == CollectionType::Set || ctype == CollectionType::ImmSet;
}
inline bool isValidCollection(CollectionType ctype) {
  return uint8_t(ctype) >= uint8_t(CollectionType::Vector) &&
         uint8_t(ctype) <= uint8_t(CollectionType::ImmSet);
}
inline bool isMutableCollection(CollectionType ctype) {
  return ctype == CollectionType::Vector ||
         ctype == CollectionType::Map ||
         ctype == CollectionType::Set;
}
inline bool isImmutableCollection(CollectionType ctype) {
  return !isMutableCollection(ctype);
}

inline bool collectionAllowsIntStringKeys(CollectionType ctype) {
  return isSetCollection(ctype) || isMapCollection(ctype);
}

}

#endif
