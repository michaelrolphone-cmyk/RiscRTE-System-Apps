#pragma once
#include <cstddef>
#include <cstdint>

namespace RiscWebDav {
constexpr size_t PropertyXmlMax = 4096;
constexpr size_t PropertyCountMax = 32;
constexpr size_t PropertyXmlDepthMax = 8;
constexpr size_t PropertyNamespaceMax = 255;
constexpr size_t PropertyLocalNameMax = 127;
constexpr size_t PropertyStringStorageMax = 2048;
namespace detail { class PropertyParser; }

enum class PropertyMode : uint8_t { All, Names, Named };

/* A copied, pointer-free request. Names are expanded XML names, not prefixes.
 * For All, the entries are DAV:include additions; for Named they are DAV:prop
 * selections. Names has no entries. Duplicates are coalesced in document order.
 * Unknown properties are retained: the response must report their names in a
 * 404 propstat rather than silently discard them. Escape namespaceUri() as an
 * XML attribute when rendering; localName() is a validated XML NCName.
 * Copies remain valid after the input buffer is changed or released. */
struct PropertyRequest {
    PropertyMode mode = PropertyMode::All;
    uint8_t count = 0;
    const char* namespaceUri(size_t index) const {
        return index < count ? strings_ + names_[index].uri : nullptr;
    }
    const char* localName(size_t index) const {
        return index < count ? strings_ + names_[index].local : nullptr;
    }
private:
    friend class detail::PropertyParser;
    struct Name { uint16_t uri = 0, local = 0; } names_[PropertyCountMax]{};
    char strings_[PropertyStringStorageMax]{};
    uint16_t used_ = 1;
};

/* Parse RFC 4918 sections 9.1, 14.20 and 17 (including allprop/include).
 * Empty input means allprop. Nonempty input must be well-formed UTF-8 XML 1.0.
 * Returns 0, 400 (malformed/unsafe XML), 413 (a documented bound exceeded), or
 * 415 (unsupported XML encoding/version). Failure resets out to its default.
 * No allocation, recursion, DTD, custom entity expansion, filesystem, or I/O.
 * Only predefined entities and numeric character references are decoded.
 * Bounds also include 16 attributes per element, 32 active namespace bindings,
 * and 32 property occurrences (including duplicates). Unknown protocol
 * extensions are ignored after validating their XML and applying these bounds.
 * Official references:
 * https://www.rfc-editor.org/rfc/rfc4918.html#section-14.20
 * https://www.rfc-editor.org/rfc/rfc4918.html#section-17
 * https://www.w3.org/TR/xml-names/
 * https://www.w3.org/TR/xml/
 */
unsigned parseProperties(const char* data, size_t size, PropertyRequest& out);
}
