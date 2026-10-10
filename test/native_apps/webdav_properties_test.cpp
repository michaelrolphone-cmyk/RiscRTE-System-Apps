#include "WebDavProperties.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace RiscWebDav;

static unsigned checks = 0;
static PropertyRequest parse(const std::string& xml, unsigned expected = 0) {
    PropertyRequest result;
    const unsigned status = parseProperties(xml.data(), xml.size(), result);
    if (status != expected) {
        std::fprintf(stderr, "Expected %u, received %u for %.200s\n", expected, status, xml.c_str());
        std::abort();
    }
    ++checks;
    if (expected) assert(result.mode == PropertyMode::All && result.count == 0);
    return result;
}
static std::string request(const std::string& child) {
    return "<D:propfind xmlns:D='DAV:'>" + child + "</D:propfind>";
}
static void named(const PropertyRequest& p, size_t index, const char* uri, const char* name) {
    assert(index < p.count);
    assert(!std::strcmp(p.namespaceUri(index), uri));
    assert(!std::strcmp(p.localName(index), name));
    ++checks;
}
static void valid() {
    auto p = parse(""); assert(p.mode == PropertyMode::All && p.count == 0);
    assert(parseProperties(nullptr, 0, p) == 0);
    p = parse(request("<D:allprop/>")); assert(p.mode == PropertyMode::All);
    p = parse(request("<D:propname/>")); assert(p.mode == PropertyMode::Names && p.count == 0);
    p = parse(request("<D:prop/>")); assert(p.mode == PropertyMode::Named && p.count == 0);
    p = parse("<propfind xmlns='DAV:'><prop><resourcetype/><getetag/></prop></propfind>");
    assert(p.mode == PropertyMode::Named && p.count == 2);
    named(p, 0, "DAV:", "resourcetype"); named(p, 1, "DAV:", "getetag");
    p = parse("<a:propfind xmlns:a='DAV:' xmlns:b='DAV:' xmlns:x='urn:custom'><b:prop>"
              "<a:getetag/><b:getetag/><x:getetag/><unknown xmlns=''/><x:unknown/>"
              "</b:prop></a:propfind>");
    assert(p.count == 4); named(p, 0, "DAV:", "getetag"); named(p, 1, "urn:custom", "getetag");
    named(p, 2, "", "unknown"); named(p, 3, "urn:custom", "unknown");
    assert(p.localName(p.count) == nullptr && p.namespaceUri(p.count) == nullptr);
    p = parse(request("<D:prop xmlns:x='urn:outer'><x:a xmlns:x='urn:inner'/>"
                      "<x:a/><b xmlns='urn:default'/><D:getetag xmlns='urn:default'/>"
                      "<xml:lang/></D:prop>"));
    assert(p.count == 5); named(p, 0, "urn:inner", "a"); named(p, 1, "urn:outer", "a");
    named(p, 2, "urn:default", "b"); named(p, 3, "DAV:", "getetag");
    named(p, 4, "http://www.w3.org/XML/1998/namespace", "lang");
    p = parse(request("<D:allprop/><D:include xmlns:C='urn:custom'><D:getetag/><C:missing/></D:include>"));
    assert(p.mode == PropertyMode::All && p.count == 2); named(p, 1, "urn:custom", "missing");
    p = parse(request("<D:include><D:missing/></D:include><D:allprop/>"));
    assert(p.mode == PropertyMode::All && p.count == 1);
    p = parse("\xef\xbb\xbf<?xml version='1.0' encoding='uTf-8' standalone='yes'?>"
              "<!--test--><?test hello?><D:propfind xmlns:D='DAV:' xml:lang='en'>"
              "<D:prop><D:getetag> \n\t&#32;&#x9;<![CDATA[ ]]></D:getetag></D:prop>"
              "</D:propfind>\n<!--after--><?done?>");
    named(p, 0, "DAV:", "getetag");
    p = parse("<?xml version='1.0' standalone='no'?><propfind xmlns='DAV:'><allprop/></propfind>");
    assert(p.mode == PropertyMode::All);
    parse("<?xml version='1.0' encoding='US-ASCII'?>" + request("<D:allprop/>"));
    p = parse(request("<D:prop xmlns:a='urn:a&amp;b&#x2f;&#47;&quot;&apos;'><a:custom/></D:prop>"));
    named(p, 0, "urn:a&b//\"'", "custom");
    p = parse(request("<D:prop><a:属性 xmlns:a='urn:日本語'/><a:𐀀 xmlns:a='urn:emoji'/></D:prop>"));
    named(p, 0, "urn:日本語", "属性"); named(p, 1, "urn:emoji", "𐀀");
    p = parse("<a:propfind z:flag='v' xmlns:z='urn:flags' xmlns:a='DAV:'>"
              "<z:extension><![CDATA[any <text> & literal]]><a:propname/></z:extension>"
              "<a:allprop ignored='&lt;'/></a:propfind>");
    assert(p.mode == PropertyMode::All && !p.count);
    p = parse(request("<D:allprop><x xmlns='urn:extension'>any text &amp; more</x></D:allprop>"));
    assert(p.mode == PropertyMode::All);
    p = parse(request("<D:prop><D:x flag='1' a:flag='2' xmlns:a='urn:a'/></D:prop>"));
    assert(p.count == 1);
    // Result storage survives both destruction/mutation of the body and value copies.
    std::string body = request("<D:prop xmlns:a='urn:a'><a:copied/></D:prop>");
    p = parse(body); const PropertyRequest copied = p;
    body.assign(body.size(), '!'); p = {};
    named(copied, 0, "urn:a", "copied");
}
static void invalid() {
    const std::vector<std::string> bad = {
        " ", "<!--nothing-->", "<propfind><prop/></propfind>",
        "<x:propfind xmlns:x='urn:not-dav'><x:allprop/></x:propfind>",
        request(""), request("<D:allprop/><D:propname/>"), request("<D:allprop/><D:allprop/>"),
        request("<D:prop/><D:include/>"), request("<D:propname/><D:include/>"),
        request("<D:include/>"), request("<D:allprop/><D:include/><D:include/>"),
        request("<D:prop><D:getetag>value</D:getetag></D:prop>"),
        request("<D:prop><D:getetag><value/></D:getetag></D:prop>"),
        request("<D:allprop>value</D:allprop>"), request("text<D:allprop/>"),
        request("<D:prop><unbound:x/></D:prop>"), request("<D:allprop unbound:x='1'/>"),
        request("<D:allprop/>garbage"), request("<D:allprop/>") + "garbage",
        request("<D:allprop/>") + request("<D:allprop/>"),
        "<D:propfind xmlns:D='DAV:'><D:allprop/></Z:propfind>",
        "<a:propfind xmlns:a='DAV:' xmlns:b='DAV:'><a:allprop/></b:propfind>",
        "<D:propfind xmlns:D='DAV:'><D:allprop>",
        "<D:propfind xmlns:D='DAV:'><D:allprop/></D:propfind junk>",
        request("<D:allprop a='1'a='2'/>"), request("<D:allprop a='1' a='2'/>"),
        request("<D:allprop xmlns:a='urn:x' xmlns:b='urn:x' a:v='1' b:v='2'/>"),
        request("<D:allprop xmlns:a='urn:x' xmlns:a='urn:y'/>"),
        request("<D:allprop xmlns:xml='urn:fake'/>"), request("<D:allprop xmlns:xmlns='urn:fake'/>"),
        request("<D:allprop xmlns:a='http://www.w3.org/XML/1998/namespace'/>"),
        request("<D:allprop xmlns='http://www.w3.org/XML/1998/namespace'/>"),
        request("<D:allprop xmlns:a='http://www.w3.org/2000/xmlns/'/>"),
        request("<D:allprop xmlns='http://www.w3.org/2000/xmlns/'/>"),
        request("<D:allprop xmlns:a=''/>"), request("<xmlns:allprop/>"),
        request("<D:prop><:x/></D:prop>"), request("<D:prop><a:/></D:prop>"),
        request("<D:prop><a:b:c/></D:prop>"), request("<D:prop><9bad/></D:prop>"),
        request("<D:prop><a&b/></D:prop>"), request("<D:allprop a='<bad'/>"),
        request("<D:allprop a=&quot;/>"), request("<D:allprop a='missing/>"),
        request("<D:allprop a='&bogus;'/>"), request("<D:allprop a='&amp'/>"),
        request("<D:allprop a='&#0;'/>"), request("<D:allprop a='&#xD800;'/>"),
        request("<D:allprop a='&#x110000;'/>"), request("<D:allprop a='&#99999999999999999999;'/>"),
        request("<D:allprop a='&#;'/>"), request("<D:allprop a='&#x;'/>"),
        request("<D:allprop a='&#X20;'/>"), request("<D:allprop a='&#12;'/>"),
        request("<D:allprop/>]]>"), request("<D:allprop/><![CDATA[value]]>"),
        request("<D:allprop/><![CDATA[unfinished"),
        request("<!-- bad -- comment --><D:allprop/>"), request("<!-- unfinished"),
        request("<D:allprop/><? unfinished?>"), request("<D:allprop/><?test unfinished"),
        request("<?XML version='1.0'?><D:allprop/>"),
        " <?xml version='1.0'?>" + request("<D:allprop/>"),
        "<?xml?>" + request("<D:allprop/>"), "<?xml encoding='UTF-8'?>" + request("<D:allprop/>"),
        "<?xml version='1.0' version='1.0'?>" + request("<D:allprop/>"),
        "<?xml version='1.0' standalone='maybe'?>" + request("<D:allprop/>"),
        "<?xml version='1.0' standalone='yes' encoding='UTF-8'?>" + request("<D:allprop/>"),
        "<!DOCTYPE propfind SYSTEM 'file:///etc/passwd'>" + request("<D:allprop/>"),
        "<!DOCTYPE propfind [<!ENTITY leak SYSTEM 'http://example.test/'>]>" + request("<D:prop><D:a>&leak;</D:a></D:prop>"),
        "<!DOCTYPE propfind [<!ENTITY a 'aa'><!ENTITY b '&a;&a;'>]>" + request("<D:allprop/>"),
        request("<!ENTITY x 'x'><D:allprop/>"),
        request("<D:allprop/>") + "&amp;"
    };
    for (const auto& s : bad) parse(s, 400);
    for (const std::string& invalidUtf8 : {std::string("\0", 1), std::string("\x01", 1),
         std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3), std::string("\xf4\x90\x80\x80", 4),
         std::string("\xe2\x82", 2), std::string("\x80", 1), std::string("\xef\xbf\xbe", 3)})
        parse(request("<D:allprop a='" + invalidUtf8 + "'/>"), 400);
    parse("<?xml version='1.1'?>" + request("<D:allprop/>"), 415);
    parse("<?xml version='1.0' encoding='UTF-16'?>" + request("<D:allprop/>"), 415);
    parse("<?xml version='1.0' encoding='ISO-8859-1'?>" + request("<D:allprop/>"), 415);
    parse(std::string("\xff\xfe", 2) + "x", 415);
    parse(std::string("\xfe\xff", 2) + "x", 415);
    parse("<?xml version='1.0' encoding='US-ASCII'?>" + request("<D:allprop a='é'/>"), 400);
    PropertyRequest p; assert(parseProperties(nullptr, 1, p) == 400);
    assert(parseProperties(nullptr, PropertyXmlMax + 1, p) == 413);
}
static void bounds() {
    std::string body = request("<D:allprop/>");
    body += std::string(PropertyXmlMax - body.size(), ' '); parse(body);
    parse(body + ' ', 413);
    body = "<D:prop>";
    for (size_t i = 0; i < PropertyCountMax; ++i) body += "<D:p" + std::to_string(i) + "/>";
    auto p = parse(request(body + "</D:prop>")); assert(p.count == PropertyCountMax);
    parse(request(body + "<D:extra/></D:prop>"), 413);
    body = "<D:prop>";
    for (size_t i = 0; i < PropertyCountMax; ++i) body += "<D:a/>";
    p = parse(request(body + "</D:prop>")); assert(p.count == 1);
    parse(request(body + "<D:a/></D:prop>"), 413);
    const std::string local(PropertyLocalNameMax, 'a');
    parse(request("<D:prop><D:" + local + "/></D:prop>"));
    parse(request("<D:prop><D:" + local + "a/></D:prop>"), 413);
    const std::string uri = "urn:" + std::string(PropertyNamespaceMax - 4, 'a');
    parse(request("<D:prop><x:a xmlns:x='" + uri + "'/></D:prop>"));
    parse(request("<D:prop><x:a xmlns:x='" + uri + "a'/></D:prop>"), 413);
    body = "<D:allprop/>";
    for (size_t i = 1; i < PropertyXmlDepthMax; ++i) body = "<extension>" + body + "</extension>";
    parse(request("<D:allprop/>" + body), 413);
    body = "<ignored/>";
    for (size_t i = 2; i < PropertyXmlDepthMax; ++i) body = "<extension>" + body + "</extension>";
    parse(request("<D:allprop/>" + body));
    body = "<D:allprop";
    for (size_t i = 0; i < 16; ++i) body += " a" + std::to_string(i) + "='1'";
    parse(request(body + "/>")); parse(request(body + " extra='1'/>"), 413);
    body = "<extension";
    for (size_t i = 0; i < 16; ++i) body += " xmlns:a" + std::to_string(i) + "='urn:a'";
    body += "><extension";
    for (size_t i = 16; i < 31; ++i) body += " xmlns:a" + std::to_string(i) + "='urn:a'";
    parse(request("<D:allprop/>" + body + "/></extension>"));
    parse(request("<D:allprop/>" + body + " xmlns:extra='urn:a'/></extension>"), 413);
    body = "<D:prop>";
    for (unsigned i = 0; i < 16; ++i) body += "<x:a xmlns:x='urn:" + std::to_string(i) + std::string(125, 'a') + "'/>";
    parse(request(body + "</D:prop>"), 413); // Copied-string pool exhausted, input still under 4096.
    // A failed parse must erase names accumulated before the error.
    p = parse(request("<D:prop><D:getetag/><unbound:x/></D:prop>"), 400);
    assert(!p.count && p.namespaceUri(0) == nullptr);
}
static void adversarial() {
    const std::vector<std::string> seeds = {
        request("<D:prop xmlns:x='urn:a&amp;b'><D:getetag/><x:missing/></D:prop>"),
        "<?xml version='1.0'?>" + request("<!--note--><D:allprop/><D:include><D:x/></D:include>"),
        request("<D:propname/><extension attr='&#x1f600;'><![CDATA[ignored]]></extension>")
    };
    auto exercise = [](const std::string& body) {
        PropertyRequest p;
        const auto status = parseProperties(body.data(), body.size(), p);
        assert(status == 0 || status == 400 || status == 413 || status == 415);
        if (status) assert(!p.count && p.mode == PropertyMode::All);
        else {
            assert(p.count <= PropertyCountMax);
            for (size_t i = 0; i < p.count; ++i) {
                assert(std::strlen(p.namespaceUri(i)) <= PropertyNamespaceMax);
                assert(std::strlen(p.localName(i)) <= PropertyLocalNameMax);
            }
        }
        ++checks;
    };
    for (const auto& seed : seeds) {
        for (size_t i = 0; i < seed.size(); ++i) {
            exercise(seed.substr(0, i));
            for (const unsigned char c : std::vector<unsigned char>{0, 1, 0xff, '<', '>', '&', ':', '/', '\'', '"', '?', ' '}) {
                std::string mutation = seed; mutation[i] = static_cast<char>(c); exercise(mutation);
            }
        }
    }
    uint32_t state = 0x517cc1b7;
    auto next = [&]() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned n = 0; n < 12000; ++n) {
        std::string noise(next() % (PropertyXmlMax + 16), ' ');
        for (char& c : noise) c = static_cast<char>(next() & 255);
        exercise(noise);
    }
}
int main() {
    static_assert(sizeof(PropertyRequest) <= 2304, "Keep the copied request small");
    valid(); invalid(); bounds(); adversarial();
    std::printf("WebDAV PROPFIND parser: %u normal, bounds, and adversarial checks passed\n", checks);
}
